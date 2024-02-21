// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/client.h>

#include "chunk.h"
#include "queue.h"
#include "uvtools.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#if defined(__linux__)
#define HAS_ABSTRACT_SOCKETS 1
#else
#define HAS_ABSTRACT_SOCKETS 0
#endif

#define READ_MINBUF 256U // 256B

enum client_state {
    CLIENT_STATE_UNINIT,
    CLIENT_STATE_INIT,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_HELLO_SENT,
    CLIENT_STATE_RUNNING,

    CLIENT_STATE_DEAD,
    CLIENT_STATE_CONN_FAIL,
};

static bool is_server_msg(const enum dicey_op op) {
    switch (op) {
    case DICEY_OP_RESPONSE:
    case DICEY_OP_EVENT:
        return true;

    default:
        return false;
    }
}

struct dicey_client {
    uv_pipe_t pipe;

    uv_thread_t thread;

    dicey_client_at_exit_fn *at_exit;
    dicey_client_on_connect_fn *on_connect;
    dicey_client_on_disconnect_fn *on_disconnect;
    dicey_client_on_error_fn *on_error;
    dicey_client_on_message_recv_fn *on_message_recv;
    dicey_client_on_message_sent_fn *on_message_sent;

    enum client_state state;
    uint32_t seq_cnt; // client-initiated packets are even, server-initiated are odd

    uv_loop_t loop;
    uv_async_t async;

    struct dicey_chunk *chunk;

    struct dicey_queue queue;

    void *data;
};

struct send_request {
    struct dicey_packet packet;
};

struct write_request {
    uv_write_t req;

    struct dicey_client *client;
    struct dicey_packet packet;
};

static void dummy_error_handler(
    const struct dicey_client *const client,
    const enum dicey_error err,
    const char *const msg,
    ...
) {
    (void) client;
    (void) err;
    (void) msg;
}

static void on_close(uv_handle_t *const handle) {
    struct dicey_client *const client = (struct dicey_client *) handle;

    if (client->on_disconnect && client->state != CLIENT_STATE_CONN_FAIL) {
        client->on_disconnect(client);
    }

    client->state = CLIENT_STATE_DEAD;

    uv_stop(&client->loop);
}

static void client_quit(struct dicey_client *const client, const enum dicey_error err) {
    assert(client);

    if (client->state == CLIENT_STATE_DEAD) {
        return;
    }

    client->state = CLIENT_STATE_DEAD;

    if (err) {
        client->on_error(client, err, "client quit: %s", dicey_error_name(err));
    }

    uv_close((uv_handle_t *) &client->pipe, &on_close);
}

static void on_write(uv_write_t *const req, const int status) {
    assert(req);

    struct write_request *const write_req = (struct write_request *) req;
    struct dicey_client *const client = write_req->client;

    assert(client);

    if (status < 0) {
        client_quit(client, dicey_error_from_uv(status));
        goto cleanup;
    }

    switch (dicey_packet_get_kind(write_req->packet)) {
    case DICEY_PACKET_KIND_BYE:
        client_quit(client, DICEY_OK);
        break;

    case DICEY_PACKET_KIND_HELLO:
        assert(client->state == CLIENT_STATE_CONNECTING);
        client->state = CLIENT_STATE_HELLO_SENT;

        break;

    default:
        break;
    }

cleanup:
    dicey_packet_deinit(&write_req->packet);
    free(write_req);
}

static void data_available(uv_async_t *const async) {
    assert(async && async->data);

    struct dicey_client *const client = async->data;

    assert(client);

    struct dicey_packet packet = { 0 };
    while (dicey_queue_pop_packet(&client->queue, &packet, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        struct write_request *const write_req = malloc(sizeof *write_req);
        if (!write_req) {
            dicey_packet_deinit(&packet);

            continue;
        }

        if (packet.nbytes > UINT_MAX) {
            client_quit(client, DICEY_EOVERFLOW);

            free(write_req);
            dicey_packet_deinit(&packet);

            continue;
        }

        uv_buf_t buf = uv_buf_init(packet.payload, (unsigned int) packet.nbytes);

        *write_req = (struct write_request) {
            .client = client,
            .packet = packet,
        };

        const int err = uv_write((uv_write_t *) &write_req, (uv_stream_t *) &client->pipe, &buf, 1, &on_write);
        if (err) {
            client->on_error(client, dicey_error_from_uv(err), "uv_write: %s", uv_strerror(err));

            free(write_req);
            dicey_packet_deinit(&packet);
        }
    }
}

void dicey_client_delete(struct dicey_client *const client) {
    if (client) {
        uv_close((uv_handle_t *) &client->pipe, NULL);

        uv_loop_close(&client->loop);

        free(client);
    }
}

enum dicey_error dicey_client_new(struct dicey_client **const dest, const struct dicey_client_args *const args) {
    assert(dest);

    struct dicey_client *const client = malloc(sizeof *client);
    if (!client) {
        return DICEY_ENOMEM;
    }

    *client = (struct dicey_client) {
        .state = CLIENT_STATE_INIT,
        .on_error = &dummy_error_handler,
    };

    if (args) {
        client->at_exit = args->at_exit;
        client->on_connect = args->on_connect;
        client->on_disconnect = args->on_disconnect;
        client->on_message_recv = args->on_message_recv;
        client->on_message_sent = args->on_message_sent;

        if (args->on_error) {
            client->on_error = args->on_error;
        }
    }

    enum dicey_error err = DICEY_OK;

    int uverr = uv_loop_init(&client->loop);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_struct;
    }

    uverr = dicey_queue_init(&client->queue);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_loop;
    }

    uverr = uv_async_init(&client->loop, &client->async, &data_available);
    if (uverr < 0) {
        err = dicey_error_from_uv(uverr);

        goto free_queue;
    }

    client->async.data = client;

    uverr = uv_pipe_init(&client->loop, &client->pipe, 0);
    if (uverr) {
        err = dicey_error_from_uv(uverr);

        goto free_async;
    }

    *dest = client;

    return DICEY_OK;

free_async:
    uv_close((uv_handle_t *) &client->async, NULL);

free_queue:
    dicey_queue_deinit(&client->queue, NULL);

free_loop:
    uv_loop_close(&client->loop);

free_struct:
    free(client);

    return err;
}

#if HAS_ABSTRACT_SOCKETS
static char *connstr_fixup(const char *connstr) {
    assert(connstr);

    char *const fixed = strdup(connstr);
    if (!fixed) {
        return NULL;
    }

    if (*fixed == '@') {
        *fixed = '\0';
    }

    return fixed;
}
#endif

static enum dicey_error client_sendpkt(struct dicey_client *const client, struct dicey_packet packet) {
    assert(client && dicey_packet_is_valid(packet));

    if (packet.nbytes > UINT_MAX) {
        return DICEY_EOVERFLOW;
    }

    struct dicey_message msg = { 0 };
    if (!dicey_packet_as_message(packet, &msg) && is_server_msg(msg.type)) {
        return DICEY_EINVAL;
    }

    enum dicey_error err = DICEY_OK;

    struct write_request *const req = malloc(sizeof *req);
    if (!req) {
        err = DICEY_ENOMEM;

        goto fail;
    }

    *req = (struct write_request) {
        .client = client,
        .packet = packet,
    };

    uv_buf_t buf = uv_buf_init(packet.payload, (unsigned int) packet.nbytes);

    return dicey_error_from_uv(uv_write((uv_write_t *) req, (uv_stream_t *) client, &buf, 1, &on_write));

fail:
    dicey_packet_deinit(&packet);
    free(req);

    return err;
}

static uint32_t client_data_next_seq(struct dicey_client *const client) {
    const uint32_t next = client->seq_cnt;

    client->seq_cnt += 2U;

    if (client->seq_cnt < next) {
        abort(); // TODO: handle overflow
    }

    return next;
}

static enum dicey_error client_signal_quit(struct dicey_client *const client, const enum dicey_bye_reason reason) {
    assert(client && client->state != CLIENT_STATE_DEAD);

    struct dicey_packet packet = { 0 };
    enum dicey_error err = dicey_packet_bye(&packet, client_data_next_seq(client), reason);
    if (!err) {
        return err;
    }

    return client_sendpkt(client, packet);
}

static enum dicey_error client_raised_error(struct dicey_client *const client, const enum dicey_error err) {
    assert(client && client->state != CLIENT_STATE_INIT);

    client->state = CLIENT_STATE_DEAD;

    client->on_error(client, err, "client raised error: %s", dicey_error_name(err));

    return client_signal_quit(client, DICEY_BYE_REASON_ERROR);
}

static void alloc_buffer(uv_handle_t *const handle, const size_t suggested_size, uv_buf_t *const buf) {
    (void) suggested_size; // useless, always 65k (max UDP packet size)

    struct dicey_client *const client = (struct dicey_client *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static ptrdiff_t client_got_bye(struct dicey_client *client, const struct dicey_bye bye) {
    (void) bye; // unused

    assert(client && client->state >= CLIENT_STATE_CONNECTING);

    client_quit(client, DICEY_OK);

    return CLIENT_STATE_DEAD;
}

static ptrdiff_t client_got_hello(struct dicey_client *client, const struct dicey_hello hello) {
    (void) hello;

    assert(client);

    if (client->state != CLIENT_STATE_HELLO_SENT) {
        return DICEY_EINVAL;
    }

    // all server versions are considered acceptable for now

    if (client->on_connect) {
        client->on_connect(client);
    }

    return CLIENT_STATE_RUNNING;
}

static ptrdiff_t client_got_message(struct dicey_client *client, const struct dicey_packet packet) {
    assert(client);

    if (client->state != CLIENT_STATE_RUNNING) {
        return DICEY_EINVAL;
    }

    struct dicey_message message = { 0 };
    if (dicey_packet_as_message(packet, &message) != DICEY_OK && !is_server_msg(message.type)) {
        return DICEY_EINVAL;
    }

    if (client->on_message_recv) {
        client->on_message_recv(client, packet);
    }

    return CLIENT_STATE_RUNNING;
}

static enum dicey_error client_got_packet(struct dicey_client *client, struct dicey_packet packet) {
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

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    (void) buf; // unused

    struct dicey_client *const client = (struct dicey_client *) stream;
    assert(client && client->chunk);

    if (nread < 0) {
        client_quit(client, nread == UV_EOF ? DICEY_OK : dicey_error_from_uv((int) nread));

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

static void on_connect(uv_connect_t *const conn, const int status) {
    assert(conn && conn->data);

    struct dicey_client *const client = conn->data;
    free(conn);

    if (status < 0) {
        client->on_error(client, dicey_error_from_uv(status), "uv_connect: %s", uv_strerror(status));

        client->state = CLIENT_STATE_CONN_FAIL;

        uv_close((uv_handle_t *) client, &on_close);

        return;
    }

    client->state = CLIENT_STATE_CONNECTING;

    const int read_err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    if (read_err < 0) {
        client_quit(client, dicey_error_from_uv(status));

        return;
    }

    struct dicey_packet packet = { 0 };
    enum dicey_error err = dicey_packet_hello(&packet, client_data_next_seq(client), DICEY_PROTO_VERSION_CURRENT);
    if (err) {
        client_quit(client, err);

        return;
    }

    err = client_sendpkt(client, packet);
    if (err) {
        client_quit(client, err);
    }
}

struct connect_args {
    struct dicey_client *client;
    char *addr;
    size_t addr_len;
};

static void connect_and_loop(void *const arg) {
    assert(arg);

    struct connect_args *const args = arg;

    struct dicey_client *const client = args->client;
    char *const addr = args->addr;
    const size_t addr_len = args->addr_len;

    free(args);

    assert(client);
    assert(addr);

    uv_connect_t *const conn = malloc(sizeof *conn);
    if (!conn) {
        client->on_error(client, DICEY_ENOMEM, "malloc failed");

        return;
    }

    *conn = (uv_connect_t) { .data = client };

    client->state = CLIENT_STATE_CONNECTING;

    int err = uv_pipe_connect2(conn, &client->pipe, addr, addr_len, 0, &on_connect);
    if (err < 0) {
        free(conn);

        client->on_error(client, dicey_error_from_uv(err), "uv_pipe_connect2: %s\n", uv_strerror(err));
    }

    err = uv_run(&client->loop, UV_RUN_DEFAULT);
    if (err < 0) {
        client->on_error(client, dicey_error_from_uv(err), "uv_run: %s\n", uv_strerror(err));
    }

    free(addr);

    if (client->at_exit) {
        client->at_exit(client);
    }
}

struct dicey_addr dicey_addr_from_str(const char *const str) {
    assert(str);

    return (struct dicey_addr) {
        .addr = str,
        .len = strlen(str),
    };
}

enum dicey_error dicey_client_connect(struct dicey_client *const client, const struct dicey_addr addr) {
    assert(client && addr.addr);

    if (client->state != CLIENT_STATE_INIT) {
        return DICEY_EINVAL;
    }

    struct connect_args *const args = malloc(sizeof *args);
    if (!args) {
        return DICEY_ENOMEM;
    }

#if HAS_ABSTRACT_SOCKETS
    char *const addr_copy = connstr_fixup(addr.addr);
#else
    char *const addr_copy = strdup(addr.addr);
#endif

    if (!addr_copy) {
        free(args);

        return DICEY_ENOMEM;
    }

    *args = (struct connect_args) {
        .client = client,
        .addr = addr_copy,
        .addr_len = addr.len,
    };

    const int thread_res = uv_thread_create(&client->thread, &connect_and_loop, args);
    if (thread_res < 0) {
        free(args);
        free(addr_copy);

        return dicey_error_from_uv(thread_res);
    }

    return DICEY_OK;
}

void *dicey_client_get_data(const struct dicey_client *client) {
    return client ? client->data : NULL;
}

enum dicey_error dicey_client_send(struct dicey_client *const client, struct dicey_packet packet) {
    assert(client);

    if (!dicey_packet_is_valid(packet)) {
        return DICEY_EINVAL;
    }

    enum dicey_error err = dicey_packet_set_seq(packet, client_data_next_seq(client));
    if (err) {
        dicey_packet_deinit(&packet);

        return err;
    }

    assert(packet.nbytes && packet.payload);

    if (!dicey_queue_push_packet(&client->queue, packet, DICEY_LOCKING_POLICY_BLOCKING)) {
        dicey_packet_deinit(&packet);

        return DICEY_ENOMEM;
    }

    return dicey_error_from_uv(uv_async_send(&client->async));
}

void *dicey_client_set_data(struct dicey_client *const client, void *const data) {
    if (!client) {
        return NULL;
    }

    void *const old_data = client->data;

    client->data = data;

    return old_data;
}
