// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/server.h>

#include "chunk.h"
#include "queue.h"
#include "uvtools.h"

#if defined(_WIN32) || defined(__unix__) || defined(__unix) || defined(unix) ||                                        \
    (defined(__APPLE__) && defined(__MACH__))
#define ZERO_PTRLIST(TYPE, BASE, LEN) memset((BASE), 0, sizeof *(BASE) * (LEN))
#else
#define ZERO_PTRLIST(TYPE, BASE, END)                                                                                  \
    for (TYPE *it = (BASE); it != (END); ++it) {                                                                       \
        *it = NULL;                                                                                                    \
    }
#endif

#define BASE_CAP 128

struct send_request {
    size_t target;
    struct dicey_packet packet;
};

struct send_request *make_request(size_t target, struct dicey_packet packet) {
    struct send_request *const req = malloc(sizeof *req);
    if (!req) {
        return NULL;
    }

    *req = (struct send_request) {
        .target = target,
        .packet = packet,
    };

    return req;
}

void free_request(void *const ptr) {
    struct send_request *const req = ptr;
    if (req) {
        dicey_packet_deinit(&req->packet);
        free(req);
    }
}

enum server_state {
    SERVER_STATE_UNINIT,
    SERVER_STATE_INIT,
    SERVER_STATE_RUNNING,
    SERVER_STATE_DEAD,
};

enum client_state {
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_RUNNING,
    CLIENT_STATE_DEAD,
};

struct client_data {
    uv_pipe_t pipe;

    enum client_state state;

    uint32_t seq_cnt;

    struct dicey_client_info info;
    struct dicey_version version;

    struct dicey_chunk *chunk;

    struct dicey_server *parent;
};

struct write_request {
    uv_write_t req;
    struct dicey_server *server;

    ptrdiff_t client_id;

    struct dicey_packet packet;
};

struct dicey_server {
    // first member is the uv_pipe_t to allow for type punning
    uv_pipe_t pipe;

    enum server_state state;

    uv_loop_t loop;
    uv_async_t async;

    struct dicey_queue queue;

    dicey_server_on_connect_fn *on_connect;
    dicey_server_on_disconnect_fn *on_disconnect;
    dicey_server_on_error_fn *on_error;
    dicey_server_on_message_fn *on_message;

    struct client_data **clients;
    size_t num;
    size_t cap;
};

static void on_write(uv_write_t *req, int status);

static bool is_server_msg(const enum dicey_op op) {
    switch (op) {
    case DICEY_OP_RESPONSE:
    case DICEY_OP_EVENT:
        return true;

    default:
        return false;
    }
}

static enum dicey_error server_sendpkt(
    struct dicey_server *const server,
    struct client_data *const client,
    struct dicey_packet packet
) {
    assert(server && client && dicey_packet_is_valid(packet));

    if (packet.nbytes > UINT_MAX) {
        return DICEY_EOVERFLOW;
    }

    struct dicey_message msg = { 0 };
    if (!dicey_packet_as_message(packet, &msg) && !is_server_msg(msg.type)) {
        return DICEY_EINVAL;
    }

    enum dicey_error err = DICEY_OK;

    struct write_request *const req = malloc(sizeof *req);
    if (!req) {
        err = DICEY_ENOMEM;

        goto fail;
    }

    *req = (struct write_request) {
        .server = server,
        .client_id = client->info.id,
        .packet = packet,
    };

    uv_buf_t buf = uv_buf_init(packet.payload, (unsigned int) packet.nbytes);

    return dicey_error_from_uv(uv_write((uv_write_t *) req, (uv_stream_t *) client, &buf, 1, &on_write));

fail:
    dicey_packet_deinit(&packet);
    free(req);

    return err;
}

static size_t compute_cap(size_t cap, const size_t new_num) {
    while (cap < new_num) {
        cap = cap * 3 / 2;
    }

    return cap;
}

#define READ_MINBUF 256U // 256B

static void client_data_delete(struct client_data *const client) {
    if (!client) {
        return;
    }

    free(client->chunk);
    free(client);
}

static uint32_t client_data_next_seq(struct client_data *const client) {
    const uint32_t next = client->seq_cnt;

    client->seq_cnt += 2U;

    if (client->seq_cnt < next) {
        abort(); // TODO: handle overflow
    }

    return next;
}

static struct client_data *client_data_new(struct dicey_server *const parent, const size_t id) {
    struct client_data *new_client = calloc(1U, sizeof *new_client);
    if (!new_client) {
        return NULL;
    }

    *new_client = (struct client_data) {
        .seq_cnt = 1U, // server-initiated packets are odd

        .info = {
            .id = id,
        },

        .parent = parent,
    };

    return new_client;
}

static void on_client_end(uv_handle_t *const handle) {
    struct client_data *const client = (struct client_data *) handle;

    if (client->parent->on_disconnect) {
        client->parent->on_disconnect(&client->info);
    }

    client_data_delete(client);
}

static enum dicey_error server_ensure_cap(struct dicey_server *const state, const size_t new_num) {
    if (new_num > state->cap) {
        const size_t new_cap = compute_cap(state->cap, new_num);

        if (new_cap > (size_t) PTRDIFF_MAX) {
            return DICEY_EOVERFLOW;
        }

        struct client_data **const new_clients = realloc(state->clients, sizeof(*state->clients) * new_cap);
        ZERO_PTRLIST(struct dicey_client_state, new_clients + state->cap, new_cap - state->cap);

        if (!new_clients) {
            return DICEY_ENOMEM;
        }

        state->clients = new_clients;
        state->cap = new_cap;
    }

    return DICEY_OK;
}

static enum dicey_error server_pick_empty_bucket(struct dicey_server *const server, struct client_data **const dest) {
    const enum dicey_error err = server_ensure_cap(server, server->num + 1);
    if (err) {
        return err;
    }

    for (size_t i = 0; i < server->cap; ++i) {
        if (!server->clients[i]) {
            struct client_data *const new_client = client_data_new(server, i);
            if (!new_client) {
                return DICEY_ENOMEM;
            }

            server->clients[i] = new_client;
            ++server->num;

            *dest = new_client;

            return DICEY_OK;
        }
    }

    abort(); // unreachable, server_ensure_cap guarantees a free slot, if it fails, it's a hard bug
}

static ptrdiff_t server_add_client(struct dicey_server *const server, struct client_data **const dest) {
    struct client_data *client = NULL;

    const enum dicey_error err = server_pick_empty_bucket(server, &client);
    if (err) {
        return err;
    }

    if (uv_pipe_init(&server->loop, &client->pipe, 0)) {
        free(client);

        return DICEY_EUV_UNKNOWN;
    }

    *dest = client;

    return client->info.id;
}

static struct client_data **server_get_client_bucket(const struct dicey_server *const server, const size_t id) {
    assert(server);

    return id < server->cap ? server->clients + id : NULL;
}

static struct client_data *server_get_client(const struct dicey_server *const server, const size_t id) {
    struct client_data *const *const bucket = server_get_client_bucket(server, id);

    return bucket ? *bucket : NULL;
}

static enum dicey_error server_kick_client(
    struct dicey_server *const server,
    const size_t id,
    const enum dicey_bye_reason reason
) {
    struct client_data *client = server_get_client(server, id);
    if (!client) {
        return DICEY_EINVAL;
    }

    struct dicey_packet packet = { 0 };
    enum dicey_error err = dicey_packet_bye(&packet, client_data_next_seq(client), reason);
    if (!err) {
        return err;
    }

    return server_sendpkt(server, client, packet);
}

static enum dicey_error server_remove_client(struct dicey_server *const server, const size_t index) {
    if (!server || !server->num) {
        return DICEY_EINVAL;
    }

    struct client_data **const bucket = server_get_client_bucket(server, index);

    if (!bucket) {
        return DICEY_EINVAL;
    }

    uv_close((uv_handle_t *) *bucket, &on_client_end);

    *bucket = NULL;
    --server->num;

    return DICEY_OK;
}

static ptrdiff_t client_got_bye(struct client_data *client, const struct dicey_bye bye) {
    (void) bye; // unused

    assert(client);

    server_remove_client(client->parent, client->info.id);

    return CLIENT_STATE_DEAD;
}

static ptrdiff_t client_got_hello(struct client_data *client, const uint32_t seq, const struct dicey_hello hello) {
    assert(client);

    if (client->state != CLIENT_STATE_CONNECTED) {
        return DICEY_EINVAL;
    }

    if (seq) {
        client->parent->on_error(
            DICEY_EINVAL, &client->info, "unexpected seq number %" PRIu32 "in hello packet, must be 0", seq
        );

        return DICEY_EINVAL;
    }

    if (dicey_version_cmp(hello.version, DICEY_PROTO_VERSION_CURRENT) < 0) {
        return DICEY_ECLIENT_TOO_OLD;
    }

    struct dicey_packet hello_repl = { 0 };

    // reply with the same seq
    enum dicey_error err = dicey_packet_hello(&hello_repl, seq, DICEY_PROTO_VERSION_CURRENT);
    if (err) {
        return err;
    }

    err = server_sendpkt(client->parent, client, hello_repl);

    return CLIENT_STATE_RUNNING;
}

static ptrdiff_t client_got_message(struct client_data *client, const struct dicey_packet packet) {
    assert(client);

    struct dicey_message message = { 0 };
    if (dicey_packet_as_message(packet, &message) != DICEY_OK || is_server_msg(message.type)) {
        return DICEY_EINVAL;
    }

    if (client->state != CLIENT_STATE_RUNNING) {
        return DICEY_EINVAL;
    }

    if (client->parent->on_message) {
        client->parent->on_message(&client->info, packet);
    }

    return CLIENT_STATE_RUNNING;
}

static enum dicey_error client_raised_error(struct client_data *client, const enum dicey_error err) {
    assert(client && client->state != CLIENT_STATE_CONNECTED);

    client->state = CLIENT_STATE_DEAD;

    client->parent->on_error(err, &client->info, "client error: %s", dicey_error_name(err));

    return server_kick_client(client->parent, client->info.id, DICEY_BYE_REASON_ERROR);
}

static enum dicey_error client_got_packet(struct client_data *client, struct dicey_packet packet) {
    assert(client && dicey_packet_is_valid(packet));

    enum dicey_error err = DICEY_OK;

    switch (dicey_packet_get_kind(packet)) {
    case DICEY_PACKET_KIND_HELLO:
        {
            uint32_t seq = 0U;
            err = dicey_packet_get_seq(packet, &seq);
            if (err) {
                break;
            }

            err = client_got_hello(client, seq, *(struct dicey_hello *) packet.payload);
            break;
        }

    case DICEY_PACKET_KIND_BYE:
        err = client_got_bye(client, *(struct dicey_bye *) packet.payload);
        break;

    case DICEY_PACKET_KIND_MESSAGE:
        err = client_got_message(client, packet);
        break;

    default:
        abort(); // unreachable, dicey_packet_is_valid guarantees a valid packet
    }

    return err ? client_raised_error(client, err) : DICEY_OK;
}

static void on_write(uv_write_t *const req, const int status) {
    assert(req);

    struct write_request *const write_req = (struct write_request *) req;
    const struct dicey_server *const server = write_req->server;

    assert(server);

    const struct client_data *const client = server_get_client(server, write_req->client_id);
    assert(client);

    const struct dicey_client_info *const info = &client->info;

    if (status < 0) {
        server->on_error(dicey_error_from_uv(status), info, "write error %s\n", uv_strerror(status));
    }

    if (dicey_packet_get_kind(write_req->packet) == DICEY_PACKET_KIND_BYE) {
        const enum dicey_error err = server_remove_client(write_req->server, write_req->client_id);
        if (err) {
            server->on_error(err, info, "server_remove_client: %s\n", dicey_error_name(err));
        }
    }

    dicey_packet_deinit(&write_req->packet);
    free(write_req);
}

static void alloc_buffer(uv_handle_t *const handle, const size_t suggested_size, uv_buf_t *const buf) {
    (void) suggested_size; // useless, always 65k (max UDP packet size)

    struct client_data *const client = (struct client_data *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    (void) buf;

    struct client_data *const client = (struct client_data *) stream;
    assert(client && client->parent && client->chunk);

    if (nread < 0) {
        if (nread != UV_EOF) {
            const int uverr = (int) nread;

            client->parent->on_error(dicey_error_from_uv(uverr), &client->info, "Read error %s\n", uv_strerror(uverr));
        }

        const enum dicey_error remove_err = server_remove_client(client->parent, client->info.id);
        if (remove_err) {
            client->parent->on_error(
                remove_err, &client->info, "server_remove_client: %s\n", dicey_error_name(remove_err)
            );
        }

        return;
    }

    struct dicey_chunk *const chunk = client->chunk;

    const void *base = chunk->bytes;
    size_t remainder = chunk->len;

    // attempt parsing a packet
    struct dicey_packet packet = { 0 };
    const enum dicey_error err = dicey_packet_load(&packet, &base, &remainder);
    switch (err) {
    case DICEY_OK:
        (void) client_got_packet(client, packet);

        dicey_chunk_clear(chunk);

        break;

    case DICEY_EAGAIN:
        break; // not enough data to parse a packet

    default:
        (void) client_raised_error(client, err);

        break;
    }
}

static void data_available(uv_async_t *const async) {
    assert(async && async->data);

    struct dicey_server *const server = async->data;

    assert(server);

    void *item;
    while (dicey_queue_pop(&server->queue, &item, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(item);

        struct send_request req = *(struct send_request *) item;
        free(item);

        struct client_data *const client = server_get_client(server, req.target);

        if (client) {
            if (req.packet.nbytes > UINT_MAX) {
                dicey_packet_deinit(&req.packet);
                server->on_error(DICEY_EOVERFLOW, &client->info, "packet too large");

                continue;
            }

            uv_write_t *const write_req = malloc(sizeof *write_req);
            if (!write_req) {
                dicey_packet_deinit(&req.packet);

                continue;
            }

            uv_buf_t buf = uv_buf_init(req.packet.payload, (unsigned int) req.packet.nbytes);

            write_req->data = req.packet.payload;

            const int err = uv_write(write_req, (uv_stream_t *) &client->pipe, &buf, 1, &on_write);
            if (err) {
                server->on_error(dicey_error_from_uv(err), &client->info, "uv_write: %s", uv_strerror(err));

                free(write_req);
                dicey_packet_deinit(&req.packet);
            }
        }
    }
}

static void on_connect(uv_stream_t *const stream, const int status) {
    assert(stream);

    struct dicey_server *const server = (struct dicey_server *) stream;

    if (status < 0) {
        server->on_error(dicey_error_from_uv(status), NULL, "New connection error %s", uv_strerror(status));

        return;
    }

    struct client_data *client = NULL;

    const ptrdiff_t id = server_add_client(server, &client);
    if (id < 0) {
        server->on_error(id, NULL, "server_add_client: %s", dicey_error_name(id));

        return;
    }

    assert(client);

    const int accept_err = uv_accept(stream, (uv_stream_t *) client);
    if (accept_err) {
        server->on_error(dicey_error_from_uv(accept_err), NULL, "uv_accept: %s", uv_strerror(accept_err));

        server_remove_client(server, (size_t) id);
    }

    if (server->on_connect && !server->on_connect((size_t) id, &client->info.user_data)) {
        server->on_error(DICEY_ECONNREFUSED, &client->info, "connection refused by user code");

        server_remove_client(server, (size_t) id);

        return;
    }

    const int err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    if (err < 0) {
        server->on_error(dicey_error_from_uv(err), &client->info, "read_start fail: %s", uv_strerror(err));

        server_remove_client(server, (size_t) id);
    }
}

static void dummy_error_handler(
    const enum dicey_error err,
    const struct dicey_client_info *const cln,
    const char *const msg,
    ...
) {
    (void) err;
    (void) cln;
    (void) msg;
}

void dicey_server_delete(struct dicey_server *const state) {
    if (!state) {
        return;
    }

    struct client_data *const *const end = state->clients + state->num;

    for (struct client_data *const *client = state->clients; client < end; ++client) {
        uv_close((uv_handle_t *) *client, NULL);

        free(*client);
    }

    uv_close((uv_handle_t *) &state->pipe, NULL);
    uv_close((uv_handle_t *) &state->async, NULL);

    dicey_queue_deinit(&state->queue, &free_request);

    uv_loop_close(&state->loop);

    free(state->clients);
    free(state);
}

enum dicey_error dicey_server_new(struct dicey_server **const dest, const struct dicey_server_args *const args) {
    assert(dest);

    struct dicey_server *const server = malloc(sizeof *server);
    if (!server) {
        return DICEY_ENOMEM;
    }

    enum dicey_error err = DICEY_OK;

    *server = (struct dicey_server) {
        .state = SERVER_STATE_INIT,

        .on_error = &dummy_error_handler,

        .clients = malloc(sizeof(*server->clients) * BASE_CAP),
        .num = 0,
        .cap = BASE_CAP,
    };

    if (args) {
        server->on_connect = args->on_connect;
        server->on_disconnect = args->on_disconnect;
        server->on_message = args->on_message;

        if (args->on_error) {
            server->on_error = args->on_error;
        }
    }

    if (!server->clients) {
        err = DICEY_ENOMEM;

        goto free_struct;
    }

    int uverr = uv_loop_init(&server->loop);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_clients;
    }

    uverr = dicey_queue_init(&server->queue);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_loop;
    }

    uverr = uv_async_init(&server->loop, &server->async, &data_available);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_queue;
    }

    server->async.data = server;

    uverr = uv_pipe_init(&server->loop, &server->pipe, 0);
    if (uverr) {
        err = dicey_error_from_uv(uverr);

        goto free_async;
    }

    *dest = server;

    return DICEY_OK;

free_async:
    uv_close((uv_handle_t *) &server->async, NULL);

free_queue:
    dicey_queue_deinit(&server->queue, &free_request);

free_loop:
    uv_loop_close(&server->loop);

free_clients:
    free(server->clients);

free_struct:
    free(server);

    return err;
}

enum dicey_error dicey_server_send(
    struct dicey_server *const server,
    const size_t id,
    const struct dicey_packet packet
) {
    assert(server);

    if (!packet.nbytes || !packet.payload) {
        return DICEY_EINVAL;
    }

    struct send_request *const req = malloc(sizeof *req);
    if (!req) {
        return DICEY_ENOMEM;
    }

    *req = (struct send_request) {
        .target = id,
        .packet = packet,
    };

    const bool success = dicey_queue_push(&server->queue, req, DICEY_LOCKING_POLICY_BLOCKING);

    assert(success);
    (void) success; // suppress unused variable warning with NDEBUG and MSVC

    return dicey_error_from_uv(uv_async_send(&server->async));
}

enum dicey_error dicey_server_start(struct dicey_server *const server, const char *const name, const size_t len) {
    assert(server && name && len);

    int err = uv_pipe_bind2(&server->pipe, name, len, 0U);

    if (err < 0) {
        goto quit;
    }

    err = uv_listen((uv_stream_t *) &server->pipe, 128, &on_connect);

    if (err < 0) {
        goto quit;
    }

    err = uv_run(&server->loop, UV_RUN_DEFAULT);

    if (err < 0) {
        goto quit;
    }

    // the loop terminated. Free everything and quit
    dicey_server_delete(server);

quit:
    return dicey_error_from_uv(err);
}
