// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/dicey.h>

#include <util/dumper.h>
#include <util/packet-dump.h>
#include <util/uvtools.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#if defined(__linux__) || defined(_WIN32)
#define PIPE_NEEDS_CLEANUP false
#if defined(_WIN32)
#define PIPE_NAME "\\\\.\\pipe\\uvsock"
#else
#define PIPE_NAME "\0/tmp/.uvsock"
#endif
#else
#define PIPE_NEEDS_CLEANUP true
#define PIPE_NAME "/tmp/.uvsock"
#endif

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

struct dicey_client_info {
    size_t id;
    void *user_data;
};

typedef bool on_connect_fn(const size_t id, void **user_data);
typedef void on_disconnect_fn(const struct dicey_client_info *cln);
typedef void on_error_fn(enum dicey_error err, const struct dicey_client_info *cln, const char *msg, ...);
typedef void on_message_fn(const struct dicey_client_info *cln, struct dicey_packet packet);

struct dicey_server_args {
    on_connect_fn *on_connect;
    on_disconnect_fn *on_disconnect;
    on_error_fn *on_error;
    on_message_fn *on_message;
};

struct send_request {
    size_t target;
    struct dicey_packet packet;
};

#define REQUEST_QUEUE_CAP 128

struct request_queue {
    uv_mutex_t mutex;
    uv_cond_t cond;

    struct send_request data[REQUEST_QUEUE_CAP];
    ptrdiff_t head;
    ptrdiff_t tail;
};

int request_queue_init(struct request_queue *const queue) {
    *queue = (struct request_queue) { 0 };

    int error = uv_mutex_init(&queue->mutex);
    if (error < 0) {
        return error;
    }

    error = uv_cond_init(&queue->cond);
    if (error < 0) {
        uv_mutex_destroy(&queue->mutex);

        return error;
    }

    return 0;
}

void request_queue_deinit(struct request_queue *const queue) {
    uv_mutex_destroy(&queue->mutex);
    uv_cond_destroy(&queue->cond);

    for (ptrdiff_t i = queue->head; i != queue->tail; i = (i + 1) % REQUEST_QUEUE_CAP) {
        dicey_packet_deinit(&queue->data[i].packet);
    }
}

size_t request_queue_size(const struct request_queue *const queue) {
    return (size_t) ((queue->tail - queue->head + REQUEST_QUEUE_CAP) % REQUEST_QUEUE_CAP);
}

enum locking_policy {
    LOCKING_POLICY_BLOCKING,
    LOCKING_POLICY_NONBLOCKING,
};

bool request_queue_pop(
    struct request_queue *const queue,
    struct send_request *const req,
    const enum locking_policy policy
) {
    uv_mutex_lock(&queue->mutex);

    while (queue->head == queue->tail) {
        if (policy == LOCKING_POLICY_NONBLOCKING) {
            uv_mutex_unlock(&queue->mutex);

            return false;
        }

        uv_cond_wait(&queue->cond, &queue->mutex);
    }

    *req = queue->data[queue->head];
    queue->head = (queue->head + 1) % REQUEST_QUEUE_CAP;

    uv_cond_signal(&queue->cond);
    uv_mutex_unlock(&queue->mutex);

    return true;
}

bool request_queue_push(struct request_queue *const queue, struct send_request req, const enum locking_policy policy) {
    uv_mutex_lock(&queue->mutex);

    const ptrdiff_t new_tail = (queue->tail + 1) % REQUEST_QUEUE_CAP;

    while (new_tail == queue->head) {
        if (policy == LOCKING_POLICY_NONBLOCKING) {
            uv_mutex_unlock(&queue->mutex);

            return false;
        }

        uv_cond_wait(&queue->cond, &queue->mutex);
    }

    queue->data[queue->tail] = req;
    queue->tail = new_tail;

    uv_cond_signal(&queue->cond);
    uv_mutex_unlock(&queue->mutex);

    return true;
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

    struct growable_chunk *chunk;

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

    struct request_queue queue;

    on_connect_fn *on_connect;
    on_disconnect_fn *on_disconnect;
    on_error_fn *on_error;
    on_message_fn *on_message;

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

    return util_uverr_to_dicey(uv_write((uv_write_t *) req, (uv_stream_t *) client, &buf, 1, &on_write));

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

struct growable_chunk {
    size_t len;
    size_t cap;
    char bytes[];
};

#define BUFFER_MINCAP 1024U // 1KB
#define READ_MINBUF 256U    // 256B

static struct growable_chunk *chunk_grow(struct growable_chunk *buf) {
    const bool zero = !buf;
    const size_t new_cap = buf && buf->cap ? buf->cap * 3 / 2 : BUFFER_MINCAP;

    buf = realloc(buf, new_cap);
    if (buf) {
        if (zero) {
            *buf = (struct growable_chunk) { 0 };
        }

        buf->cap = new_cap;
    }

    return buf;
}

static size_t chunk_get_free(struct growable_chunk *buf) {
    return buf ? buf->cap - buf->len : 0U;
}

static uv_buf_t chunk_get_buf(struct growable_chunk **const buf, const size_t min) {
    assert(buf);

    while (chunk_get_free(*buf) < min) {
        *buf = chunk_grow(*buf);
    }

    struct growable_chunk *const cnk = *buf;
    const size_t avail = cnk->cap - cnk->len - sizeof *cnk;

    return uv_buf_init(cnk->bytes + cnk->len, avail > UINT32_MAX ? UINT32_MAX : (uint32_t) avail);
}

static void chunk_clear(struct growable_chunk *const buffer) {
    assert(buffer);

    buffer->len = 0;
}

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

static ptrdiff_t client_got_hello(struct client_data *client, const struct dicey_hello hello) {
    assert(client);

    if (client->state != CLIENT_STATE_CONNECTED) {
        return DICEY_EINVAL;
    }

    if (dicey_version_cmp(hello.version, DICEY_PROTO_VERSION_CURRENT) < 0) {
        return DICEY_ECLIENT_TOO_OLD;
    }

    struct dicey_packet hello_repl = { 0 };

    enum dicey_error err = dicey_packet_hello(&hello_repl, client_data_next_seq(client), DICEY_PROTO_VERSION_CURRENT);
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
        err = client_got_hello(client, *(struct dicey_hello *) packet.payload);
        break;

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
        server->on_error(util_uverr_to_dicey(status), info, "write error %s\n", uv_strerror(status));
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

    *buf = chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    (void) buf;

    struct client_data *const client = (struct client_data *) stream;
    assert(client && client->parent && client->chunk);

    if (nread < 0) {
        if (nread != UV_EOF) {
            const int uverr = (int) nread;

            client->parent->on_error(util_uverr_to_dicey(uverr), &client->info, "Read error %s\n", uv_strerror(uverr));
        }

        const enum dicey_error remove_err = server_remove_client(client->parent, client->info.id);
        if (remove_err) {
            client->parent->on_error(
                remove_err, &client->info, "server_remove_client: %s\n", dicey_error_name(remove_err)
            );
        }

        return;
    }

    struct growable_chunk *const chunk = client->chunk;

    const void *base = chunk->bytes;
    size_t remainder = chunk->len;

    // attempt parsing a packet
    struct dicey_packet packet = { 0 };
    const enum dicey_error err = dicey_packet_load(&packet, &base, &remainder);
    switch (err) {
    case DICEY_OK:
        (void) client_got_packet(client, packet);

        chunk_clear(chunk);

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

    struct send_request req;
    while (request_queue_pop(&server->queue, &req, LOCKING_POLICY_NONBLOCKING)) {
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
                server->on_error(util_uverr_to_dicey(err), &client->info, "uv_write: %s", uv_strerror(err));

                free(write_req);
                dicey_packet_deinit(&req.packet);
            }
        }
    }
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

    request_queue_push(
        &server->queue,
        (struct send_request) {
            .target = id,
            .packet = packet,
        },
        LOCKING_POLICY_BLOCKING
    );

    return util_uverr_to_dicey(uv_async_send(&server->async));
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

static enum dicey_error dicey_server_new(struct dicey_server **const dest, const struct dicey_server_args *args) {
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
        err = util_uverr_to_dicey(uverr);

        goto free_clients;
    }

    uverr = request_queue_init(&server->queue);
    if (uverr < 0) {
        err = util_uverr_to_dicey(uverr);

        goto free_loop;
    }

    uverr = uv_async_init(&server->loop, &server->async, &data_available);
    if (uverr < 0) {
        err = util_uverr_to_dicey(uverr);

        goto free_queue;
    }

    server->async.data = server;

    uverr = uv_pipe_init(&server->loop, &server->pipe, 0);
    if (uverr) {
        err = util_uverr_to_dicey(uverr);

        goto free_async;
    }

    *dest = server;

    return DICEY_OK;

free_async:
    uv_close((uv_handle_t *) &server->async, NULL);

free_queue:
    request_queue_deinit(&server->queue);

free_loop:
    uv_loop_close(&server->loop);

free_clients:
    free(server->clients);

free_struct:
    free(server);

    return err;
}

void server_delete(struct dicey_server *const state) {
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

    request_queue_deinit(&state->queue);

    uv_loop_close(&state->loop);

    free(state->clients);
    free(state);
}

static void on_connect(uv_stream_t *const stream, const int status) {
    assert(stream);

    struct dicey_server *const server = (struct dicey_server *) stream;

    if (status < 0) {
        server->on_error(util_uverr_to_dicey(status), NULL, "New connection error %s", uv_strerror(status));

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
        server->on_error(util_uverr_to_dicey(accept_err), NULL, "uv_accept: %s", uv_strerror(accept_err));

        server_remove_client(server, (size_t) id);
    }

    if (server->on_connect && !server->on_connect((size_t) id, &client->info.user_data)) {
        server->on_error(DICEY_ECONNREFUSED, &client->info, "connection refused by user code");

        server_remove_client(server, (size_t) id);

        return;
    }

    const int err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    if (err < 0) {
        server->on_error(util_uverr_to_dicey(err), &client->info, "read_start fail: %s", uv_strerror(err));

        server_remove_client(server, (size_t) id);
    }
}

static bool on_client_connect(const size_t id, void **const user_data) {
    printf("info: client %zu connected\n", id);

    *user_data = NULL;

    return true;
}

static void on_client_disconnect(const struct dicey_client_info *const cln) {
    printf("info: client %zu disconnected\n", cln->id);
}

static void on_client_error(
    const enum dicey_error err,
    const struct dicey_client_info *const cln,
    const char *const msg,
    ...
) {
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "%sError (%s)", dicey_error_name(err), dicey_error_msg(err));

    if (cln) {
        fprintf(stderr, " (on client %zu)", cln->id);
    }

    fprintf(stderr, ": ");
    vfprintf(stderr, msg, args);
    fputc('\n', stderr);

    va_end(args);
}

static void on_packet_received(const struct dicey_client_info *const cln, struct dicey_packet packet) {
    printf("info: received packet from client %zu\n", cln->id);

    struct util_dumper dumper = { 0 };

    util_dumper_dump_packet(&dumper, packet);
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
    server_delete(server);

quit:
    return util_uverr_to_dicey(err);
}

#if PIPE_NEEDS_CLEANUP
static enum dicey_error remove_socket_if_present(uv_loop_t *const loop) {
    const int err = uv_fs_unlink(loop, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

    return err == UV_ENOENT ? 0 : util_uverr_to_dicey(err);
}
#endif

int main(void) {
    uv_loop_t loop = { 0 };

    uv_loop_init(&loop);

    struct dicey_server *server = NULL;

    enum dicey_error err = dicey_server_new(
        &server,
        &(struct dicey_server_args) {
            .on_connect = &on_client_connect,
            .on_disconnect = &on_client_disconnect,
            .on_error = &on_client_error,
            .on_message = &on_packet_received,
        }
    );

    if (err) {
        fprintf(stderr, "dicey_server_init: %s\n", dicey_error_name(err));

        goto quit;
    }

#if PIPE_NEEDS_CLEANUP
    err = remove_socket_if_present(&loop);
    if (err) {
        fprintf(stderr, "uv_fs_unlink: %s\n", uv_err_name(err));

        goto quit;
    }
#endif

    err = dicey_server_start(server, PIPE_NAME, sizeof PIPE_NAME - 1U);
    if (err) {
        fprintf(stderr, "dicey_server_start: %s\n", dicey_error_name(err));

        goto quit;
    }

    uv_fs_unlink(&loop, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

quit:
    server_delete(server);

    return err == DICEY_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
