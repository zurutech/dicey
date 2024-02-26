// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/client.h>

#include "sup/asprintf.h"
#include "sup/assume.h"
#include "sup/util.h"

#include "chunk.h"
#include "pending-list.h"
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

struct send_request {
    uint32_t seq;
    struct dicey_client *client;
    dicey_client_on_reply_fn *cb;
    void *data;
    struct dicey_packet packet;

    uv_timespec64_t expires_at;
};

int32_t add_wrapping(int32_t *a, const int32_t b, const int32_t max) {
    // return the remainder

    int32_t sum;
    if (!dutl_checked_add(&sum, *a, b)) {
        abort(); // no way out here
    }

    const imaxdiv_t div = imaxdiv(sum, max);

    *a = (int32_t) div.rem;

    return (int32_t) div.quot;
}

uv_timespec64_t now_plus(const int32_t ms) {
    uv_timespec64_t now = { 0 };
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);

    const int32_t nsec = (ms % 1000) * 1000000;

    const int32_t sec = ms / 1000 + add_wrapping(&now.tv_nsec, nsec, 1000000000);

    now.tv_sec += sec;

    return now;
}

_Thread_local uv_once_t this_thread_sem_init = UV_ONCE_INIT;
_Thread_local uv_sem_t this_thread_sem;

static void this_thread_do_init(void) {
    const int res = uv_sem_init(&this_thread_sem, 0);
    if (res < 0) {
        abort(); // if we can't init a semaphore, this is the safest possible course of action
    }

    uv_sem_post(&this_thread_sem);
}

static void this_thread_init(void) {
    uv_once(&this_thread_sem_init, &this_thread_do_init);
}

struct dicey_client {
    uv_pipe_t pipe;

    uv_thread_t thread;

    dicey_client_event_fn *on_event;
    dicey_client_inspect_fn *inspect_func;

    _Atomic enum client_state state;
    uint32_t seq_cnt; // client-initiated packets are even, server-initiated are odd

    uv_loop_t loop;
    uv_async_t async;
    uv_timer_t timeout_timer;

    struct dicey_chunk *chunk;

    struct dicey_queue queue;
    struct dicey_pending_list *pending;

    void *data;
};

static bool client_process_event(
    struct dicey_client_event *const ev,
    struct dicey_client *const client,
    const int event,
    ...
);

static bool client_process_event_va(
    struct dicey_client_event *const ev,
    struct dicey_client *const client,
    const enum dicey_client_event_type event,
    va_list args
) {
    assert(ev && client);

    *ev = (struct dicey_client_event) { .type = event };

    switch (ev->type) {
    case DICEY_CLIENT_EVENT_CONNECT:
        assert(client->state == CLIENT_STATE_HELLO_SENT);

        client->state = CLIENT_STATE_RUNNING;

        break;

    case DICEY_CLIENT_EVENT_DISCONNECT:
        assert(client->state >= CLIENT_STATE_HELLO_SENT);

        client->state = CLIENT_STATE_DEAD; // TODO: support reconnections

        break;

    case DICEY_CLIENT_EVENT_ERROR:
        {
            ev->error.err = va_arg(args, enum dicey_error);
            const char *const fmt = va_arg(args, const char *);

            if (vasprintf(&ev->error.msg, fmt, args) < 0) {
                abort(); // no idea on how to recover from out of memory in this very spot
            }

            client->state = client->state <= CLIENT_STATE_CONNECTING ? CLIENT_STATE_CONN_FAIL : CLIENT_STATE_DEAD;

            break;
        }

    case DICEY_CLIENT_EVENT_HANDSHAKE_START:
        assert(client->state == CLIENT_STATE_INIT);

        ev->version = va_arg(args, struct dicey_version);

        client->state = CLIENT_STATE_CONNECTING;

        break;

    case DICEY_CLIENT_EVENT_HANDSHAKE_WAITING:
        assert(client->state == CLIENT_STATE_CONNECTING);

        break;

    case DICEY_CLIENT_EVENT_INIT:
        assert(client->state == CLIENT_STATE_UNINIT);

        client->state = CLIENT_STATE_INIT;

        break;

    case DICEY_CLIENT_EVENT_MESSAGE_RECEIVING:
        // messages should never be sent or received in the wrong state
        if (client->state != CLIENT_STATE_RUNNING) {
            // raise another state transition
            return client_process_event(
                ev,
                client,
                DICEY_CLIENT_EVENT_ERROR,
                DICEY_EINVAL,
                "invalid state for message, server has violated protocol"
            );
        } else {
            ev->packet = va_arg(args, struct dicey_packet *);
        }

        break;

    case DICEY_CLIENT_EVENT_MESSAGE_SENDING:
        if (client->state != CLIENT_STATE_RUNNING) {
            return client_process_event(
                ev,
                client,
                DICEY_CLIENT_EVENT_ERROR,
                DICEY_EINVAL,
                "invalid state for message, not connected to server yet"
            );
        } else {
            ev->packet = va_arg(args, struct dicey_packet *);
        }

        break;

    case DICEY_CLIENT_EVENT_SERVER_BYE:
        {
            assert(client->state >= CLIENT_STATE_CONNECTING);

            const enum dicey_bye_reason bye_reason = va_arg(args, enum dicey_bye_reason);
            if (bye_reason == DICEY_BYE_REASON_ERROR) {
                return client_process_event(ev, client, DICEY_CLIENT_EVENT_ERROR, "kicked by server");
            } else {
                // raise the event
                client->state = CLIENT_STATE_DEAD;
            }

            break;
        }

    default:
        abort(); // unreachable, dicey_client_event_type is an enum
    }

    va_end(args);

    return true;
}

static bool client_process_event(
    struct dicey_client_event *const ev,
    struct dicey_client *const client,
    const int event,
    ...
) {
    va_list args;
    va_start(args, event);

    const bool res = client_process_event_va(ev, client, event, args);

    va_end(args);

    return res;
}

static bool client_event(struct dicey_client *const client, const int event, ...) {
    if (!client->inspect_func) {
        return false;
    }

    va_list args;
    va_start(args, event);

    struct dicey_client_event ev = { 0 };
    const bool res = client_process_event_va(&ev, client, event, args);

    va_end(args);

    if (res) {
        client->inspect_func(client, dicey_client_get_context(client), ev);
    }

    return res;
}

static void on_close(uv_handle_t *const handle) {
    struct dicey_client *const client = (struct dicey_client *) handle;

    client_event(client, DICEY_CLIENT_EVENT_DISCONNECT);

    uv_stop(&client->loop);
}

static void client_quit(struct dicey_client *const client) {
    assert(client);

    uv_timer_stop(&client->timeout_timer);
    uv_close((uv_handle_t *) &client->pipe, &on_close);
}

static void on_write(uv_write_t *const req, const int status) {
    assert(req && req->data);

    struct send_request *const send_req = req->data;
    struct dicey_client *const client = send_req->client;

    assert(client);

    free(req);

    if (status < 0) {
        client_event(
            client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(status), "uv_write: %s", uv_strerror(status)
        );
        client_quit(client);

        goto cleanup;
    }

    switch (dicey_packet_get_kind(send_req->packet)) {
    case DICEY_PACKET_KIND_BYE:
        client_quit(client);
        return;

    case DICEY_PACKET_KIND_HELLO:
        assert(client->state == CLIENT_STATE_CONNECTING);
        client->state = CLIENT_STATE_HELLO_SENT;

        break;

    default:
        break;
    }

    dicey_pending_list_append(
        &client->pending,
        &(struct dicey_pending_reply) {
            .seq = send_req->seq,
            .callback = send_req->cb,
            .data = send_req->data,
            .expires_at = send_req->expires_at,
        }
    );

cleanup:
    dicey_packet_deinit(&send_req->packet);
    free(send_req);
}

static void check_timeout(uv_timer_t *const timer) {
    struct dicey_client *const client = (struct dicey_client *) timer->data;

    assert(client);

    if (client->state != CLIENT_STATE_RUNNING) {
        return;
    }

    dicey_pending_list_prune(client->pending, client);
}

#if HAS_ABSTRACT_SOCKETS
static void connstr_fixup(char *connstr) {
    assert(connstr);

    if (*connstr == '@') {
        *connstr = '\0';
    }
}
#endif

static enum dicey_error client_do_send(struct dicey_client *const client, struct send_request *req) {
    assert(client && req && dicey_packet_is_valid(req->packet));

    uv_write_t *const write_req = malloc(sizeof *write_req);
    if (!write_req) {
        dicey_packet_deinit(&req->packet);

        return DICEY_ENOMEM;
    }

    if (req->packet.nbytes > UINT_MAX) {
        client_event(
            client,
            DICEY_CLIENT_EVENT_ERROR,
            DICEY_EOVERFLOW,
            "attempting to send an abnormally sized packet of %" PRIu64 " bytes",
            req->packet.nbytes
        );

        free(write_req);
        dicey_packet_deinit(&req->packet);

        return DICEY_EOVERFLOW;
    }

    uv_buf_t buf = uv_buf_init(req->packet.payload, (unsigned int) req->packet.nbytes);

    write_req->data = req;

    client_event(client, DICEY_CLIENT_EVENT_MESSAGE_SENDING, &req->packet);

    const int err = uv_write(write_req, (uv_stream_t *) &client->pipe, &buf, 1U, &on_write);
    if (err) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(err), "uv_write: %s", uv_strerror(err));

        free(write_req);
        free(req);
        dicey_packet_deinit(&req->packet);

        client_quit(client);
    }

    return DICEY_OK;
}

static void data_available(uv_async_t *const async) {
    assert(async && async->data);

    struct dicey_client *const client = async->data;

    assert(client);

    void *req_ptr = NULL;
    while (dicey_queue_pop(&client->queue, &req_ptr, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(req_ptr);

        struct send_request *const req = req_ptr;

        const enum dicey_error err = client_do_send(client, req);

        if (err) {
            client_quit(client);
        }
    }
}

static enum dicey_error client_queue_out_packet(
    struct dicey_client *const client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *const cb,
    void *const data,
    const uint32_t timeout
) {
    struct send_request *const req = malloc(sizeof *req);
    if (!req) {
        dicey_packet_deinit(&packet);

        return DICEY_ENOMEM;
    }

    uint32_t seq = 0U;
    const enum dicey_error err = dicey_packet_get_seq(packet, &seq);
    if (err) {
        dicey_packet_deinit(&packet);
        free(req);
    }

    // seq 0 is reserved for the first packet, which is always a hello
    // also, we use even numbers for client-initiated packets, so check for that
    assert(seq && !(seq % 2U));

    *req = (struct send_request) {
        .seq = seq,
        .client = client,
        .cb = cb,
        .data = data,
        .packet = packet,
        .expires_at = now_plus(timeout),
    };

    if (!dicey_queue_push(&client->queue, req, DICEY_LOCKING_POLICY_BLOCKING)) {
        dicey_packet_deinit(&packet);

        return DICEY_ENOMEM;
    }

    return dicey_error_from_uv(uv_async_send(&client->async));
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

    struct send_request *const req = malloc(sizeof *req);
    if (!req) {
        return DICEY_ENOMEM;
    }

    *req = (struct send_request) {
        .seq = client_data_next_seq(client),
        .client = client,
        .cb = NULL,
        .data = NULL,
        .expires_at = now_plus(0),
    };

    enum dicey_error err = dicey_packet_bye(&req->packet, client_data_next_seq(client), reason);
    if (!err) {
        return err;
    }

    return client_do_send(client, req);
}

static void alloc_buffer(uv_handle_t *const handle, const size_t suggested_size, uv_buf_t *const buf) {
    (void) suggested_size; // useless, always 65k (max UDP packet size)

    struct dicey_client *const client = (struct dicey_client *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static enum dicey_error client_match_response(struct dicey_client *client, const struct dicey_packet packet) {
    assert(client && dicey_packet_is_valid(packet));

    uint32_t seq = 0U;
    DICEY_ASSUME(dicey_packet_get_seq(packet, &seq));

    struct dicey_pending_reply it = { 0 };
    if (!dicey_pending_list_search_and_remove(client->pending, seq, &it)) {
        return DICEY_EINVAL;
    }

    if (it.callback) {
        it.callback(client, DICEY_OK, packet, it.data);
    }

    return DICEY_OK;
}

static enum dicey_error client_got_bye(struct dicey_client *client, const struct dicey_packet packet) {
    struct dicey_bye bye = { 0 };
    DICEY_ASSUME(dicey_packet_as_bye(packet, &bye));

    assert(client && client->state >= CLIENT_STATE_CONNECTING);

    client_event(client, DICEY_CLIENT_EVENT_SERVER_BYE, bye.reason);

    client_quit(client);

    return DICEY_OK;
}

static enum dicey_error client_got_hello(struct dicey_client *client, const struct dicey_packet packet) {
    (void) packet;

    assert(client);

    if (client->state != CLIENT_STATE_HELLO_SENT) {
        return DICEY_EINVAL;
    }

    // all server versions are considered acceptable for now
    client_event(client, DICEY_CLIENT_EVENT_CONNECT);

    assert(client->pending && client->pending->len == 1U);

    // note: we must find a listener for this connect, otherwise the client is broken
    DICEY_ASSUME(client_match_response(client, packet));

    return DICEY_OK;
}

static enum dicey_error client_got_message(struct dicey_client *client, const struct dicey_packet packet) {
    assert(client);

    if (client->state != CLIENT_STATE_RUNNING) {
        return DICEY_EINVAL;
    }

    struct dicey_message msg = { 0 };
    DICEY_ASSUME(dicey_packet_as_message(packet, &msg));

    if (!is_server_msg(msg.type)) {
        client_event(
            client,
            DICEY_CLIENT_EVENT_ERROR,
            DICEY_EINVAL,
            "invalid message received from server, type = %s",
            dicey_op_to_string(msg.type)
        );

        return DICEY_EINVAL;
    }

    client_event(client, DICEY_CLIENT_EVENT_MESSAGE_RECEIVING, &packet);

    switch (msg.type) {
    case DICEY_OP_RESPONSE:
        {
            const enum dicey_error err = client_match_response(client, packet);
            if (err) {
                return err;
            }

            break;
        }

    case DICEY_OP_EVENT:
        if (client->on_event) {
            client->on_event(client, client->data, packet);
        }

        break;

    default:
        abort(); // unreachable, we've checked is_server_msg above
    }

    return DICEY_OK;
}

static enum dicey_error client_got_packet(struct dicey_client *client, struct dicey_packet packet) {
    assert(client && dicey_packet_is_valid(packet));

    enum dicey_error err = DICEY_OK;

    const enum dicey_packet_kind kind = dicey_packet_get_kind(packet);

    switch (kind) {
    case DICEY_PACKET_KIND_HELLO:
        err = client_got_hello(client, packet);
        break;

    case DICEY_PACKET_KIND_BYE:
        err = client_got_bye(client, packet);
        break;

    case DICEY_PACKET_KIND_MESSAGE:
        err = client_got_message(client, packet);
        break;

    default:
        abort(); // unreachable, dicey_packet_is_valid guarantees a valid packet
    }

    return err ? client_signal_quit(client, DICEY_BYE_REASON_ERROR) : DICEY_OK;
}

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    (void) buf; // unused

    struct dicey_client *const client = (struct dicey_client *) stream;
    assert(client && client->chunk);

    if (nread < 0) {
        if (nread != UV_EOF) {
            const int uverr = (int) nread;

            client_event(
                client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(uverr), "uv_read: %s", uv_strerror(uverr)
            );
        }

        client_quit(client);

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
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err, "invalid packet received");

        client_signal_quit(client, DICEY_BYE_REASON_ERROR);

        break;
    }
}

struct connect_args {
    uv_connect_t conn;

    struct dicey_client *client;

    dicey_client_on_connect_fn *on_connect_cb;
    void *data;

    size_t addr_len;
    char addr[];
};

void on_server_hello_reply(
    struct dicey_client *client,
    enum dicey_error error,
    struct dicey_packet packet,
    void *data
) {
    (void) packet; // unused with NDEBUG

    assert(client && data);
    assert(dicey_packet_is_valid(packet) && dicey_packet_get_kind(packet) == DICEY_PACKET_KIND_HELLO);

    struct connect_args *const connect_data = data;

    assert(connect_data->client == client && connect_data->on_connect_cb);

    connect_data->on_connect_cb(client, connect_data->data, error);

    free(connect_data);
}

static enum dicey_error client_quick_send_first_hello(struct connect_args *const connect_data) {
    assert(connect_data);

    struct dicey_client *const client = connect_data->client;

    struct send_request *const req = malloc(sizeof *req);
    if (!req) {
        return DICEY_ENOMEM;
    }

    *req = (struct send_request) {
        .seq = client_data_next_seq(client),
        .client = client,
        .cb = &on_server_hello_reply,
        .data = connect_data,
        .expires_at = now_plus(0),
    };

    assert(!req->seq); // first packet MUST have seq 0

    enum dicey_error err = dicey_packet_hello(&req->packet, req->seq, DICEY_PROTO_VERSION_CURRENT);
    if (err) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err, "failed to create hello packet");
        client_quit(client);

        return err;
    }

    err = client_do_send(client, req);
    if (err) {
        dicey_packet_deinit(&req->packet);
        free(req);
        free(connect_data);

        client_quit(client);
    }

    return DICEY_OK;
}

static void on_connect(uv_connect_t *const conn, const int status) {
    assert(conn);

    struct connect_args *const args = (struct connect_args *) conn;
    struct dicey_client *const client = args->client;

    assert(client);

    free(conn);

    if (status < 0) {
        client_event(
            client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(status), "uv_connect: %s", uv_strerror(status)
        );

        client_quit(client);

        return;
    }

    client_event(client, DICEY_CLIENT_EVENT_HANDSHAKE_START, DICEY_PROTO_VERSION_CURRENT);

    enum dicey_error err = client_quick_send_first_hello(args);
    if (err) {
        client_quit(client);

        return;
    }

    err = uv_read_start((uv_stream_t *) &client->pipe, &alloc_buffer, &on_read);
    if (err < 0) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(err), "uv_read_start: %s", uv_strerror(err));

        client_quit(client);
    }
}

static void connect_and_loop(void *const arg) {
    assert(arg);

    struct connect_args *const args = arg;
    struct dicey_client *const client = args->client;
    const char *const addr = args->addr;
    const size_t addr_len = args->addr_len;
    dicey_client_on_connect_fn *const on_connect_cb = args->on_connect_cb;

    assert(on_connect_cb);

    assert(client && on_connect_cb);

    enum dicey_error err = DICEY_OK;

    uv_connect_t *const conn = malloc(sizeof *conn);
    if (!conn) {
        err = DICEY_ENOMEM;

        goto fail;
    }

    *conn = (uv_connect_t) { .data = client };

    client->state = CLIENT_STATE_CONNECTING;

    int uverr = uv_pipe_connect2(conn, &client->pipe, addr, addr_len, 0, &on_connect);
    if (uverr < 0) {
        free(conn);

        err = dicey_error_from_uv(err);

        goto fail;
    }

    uverr = uv_timer_start(&client->timeout_timer, &check_timeout, 0, 1000);
    if (uverr < 0) {
        free(conn);

        err = dicey_error_from_uv(uverr);

        goto fail;
    }

    uverr = uv_run(&client->loop, UV_RUN_DEFAULT);
    if (err < 0) {
        err = dicey_error_from_uv(uverr);

        goto fail;
    }

    return;

fail:
    on_connect_cb(client, client->data, err);
}

struct sync_conn_data {
    uv_sem_t *sem;
    enum dicey_error err;
};

static void wait_for_connect(struct dicey_client *const client, void *const data, const enum dicey_error err) {
    (void) client;

    assert(data);

    struct sync_conn_data *const sync_data = data;

    assert(sync_data->sem);

    sync_data->err = err;

    uv_sem_post(sync_data->sem); // unlock the waiting thread
}

struct sync_reply_data {
    uv_sem_t *sem;
    enum dicey_error err;
    struct dicey_packet packet;
};

static void wait_for_reply(
    struct dicey_client *client,
    const enum dicey_error err,
    struct dicey_packet packet,
    void *data
) {
    (void) client;

    assert(data);

    struct sync_reply_data *const sync_data = data;

    assert(sync_data->sem);

    sync_data->err = err;
    sync_data->packet = packet;

    uv_sem_post(sync_data->sem); // unlock the waiting thread
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

    this_thread_init();

    struct sync_conn_data data = { .sem = &this_thread_sem, .err = DICEY_OK };
    enum dicey_error conn_err = dicey_client_connect_async(client, addr, &wait_for_connect, &data);
    if (conn_err) {
        return conn_err;
    }

    uv_sem_wait(&this_thread_sem);

    return data.err;
}

enum dicey_error dicey_client_connect_async(
    struct dicey_client *const client,
    const struct dicey_addr addr,
    dicey_client_on_connect_fn *const on_connect_cb,
    void *const data
) {
    assert(client && addr.addr);

    if (client->state != CLIENT_STATE_INIT) {
        return DICEY_EINVAL;
    }

    struct connect_args *const args = malloc(sizeof *args + addr.len + 1U);
    if (!args) {
        return DICEY_ENOMEM;
    }

    *args = (struct connect_args) {
        .client = client,

        .on_connect_cb = on_connect_cb,
        .data = data,

        .addr_len = addr.len,
    };

    memcpy(args->addr, addr.addr, addr.len + 1U);

#if HAS_ABSTRACT_SOCKETS
    connstr_fixup(args->addr);
#endif

    const int thread_res = uv_thread_create(&client->thread, &connect_and_loop, args);
    if (thread_res < 0) {
        free(args);

        return dicey_error_from_uv(thread_res);
    }

    return DICEY_OK;
}

void dicey_client_delete(struct dicey_client *const client) {
    if (client) {
        uv_close((uv_handle_t *) &client->pipe, NULL);

        uv_loop_close(&client->loop);

        free(client);
    }
}

void *dicey_client_get_context(const struct dicey_client *client) {
    return client ? client->data : NULL;
}

enum dicey_error dicey_client_new(struct dicey_client **const dest, const struct dicey_client_args *const args) {
    assert(dest);

    struct dicey_client *const client = malloc(sizeof *client);
    if (!client) {
        return DICEY_ENOMEM;
    }

    *client = (struct dicey_client) {
        .state = CLIENT_STATE_INIT,
    };

    if (args) {
        client->on_event = args->on_event;
        client->inspect_func = args->inspect_func;
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

    uverr = uv_timer_init(&client->loop, &client->timeout_timer);
    if (uverr) {
        err = dicey_error_from_uv(uverr);

        goto free_pipe;
    }

    *dest = client;

    return DICEY_OK;

free_pipe:
    uv_close((uv_handle_t *) &client->pipe, NULL);

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

enum dicey_error dicey_client_request(
    struct dicey_client *const client,
    const struct dicey_packet packet,
    struct dicey_packet *const reply,
    const uint32_t timeout
) {
    assert(client);

    this_thread_init();

    struct sync_reply_data data = { .sem = &this_thread_sem, .err = DICEY_OK };
    enum dicey_error req_err = dicey_client_request_async(client, packet, &wait_for_reply, &data, timeout);
    if (req_err) {
        return req_err;
    }

    uv_sem_wait(&this_thread_sem);

    if (reply) {
        *reply = data.packet;
    }

    return data.err;
}

enum dicey_error dicey_client_request_async(
    struct dicey_client *const client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *cb,
    void *data,
    const uint32_t timeout
) {
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

    return client_queue_out_packet(client, packet, cb, data, timeout);
}

void *dicey_client_set_context(struct dicey_client *const client, void *const data) {
    if (!client) {
        return NULL;
    }

    void *const old_data = client->data;

    client->data = data;

    return old_data;
}
