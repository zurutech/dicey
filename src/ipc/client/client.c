// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/address.h>
#include <dicey/ipc/client.h>

#include "sup/asprintf.h"

#include "ipc/chunk.h"

#include "ipc/tasks/io.h"
#include "ipc/tasks/list.h"
#include "ipc/tasks/loop.h"
#include "ipc/uvtools.h"

#include "waiting-list.h"

#define DEFAULT_TIMEOUT ((int32_t) 1000U)
#define READ_MINBUF 256U // 256B

enum client_state {
    CLIENT_STATE_UNINIT,
    CLIENT_STATE_INIT,
    CLIENT_STATE_CONNECT_START,
    CLIENT_STATE_RUNNING,

    CLIENT_STATE_DEAD,

    CLIENT_STATE_CLOSING,
    CLIENT_STATE_CLOSED,
};

struct dicey_client {
    _Atomic enum client_state state;

    uv_pipe_t pipe;

    struct dicey_task_loop *tloop;

    dicey_client_inspect_fn *inspect_func;
    dicey_client_event_fn *on_event;

    struct dicey_waiting_list *waiting_tasks;
    struct dicey_chunk *recv_chunk;

    uint32_t next_seq;

    void *ctx;
};

struct sync_conn_data {
    uv_sem_t sem;
    enum dicey_error err;
};

struct sync_disconn_data {
    uv_sem_t sem;
    enum dicey_error err;
};

struct sync_req_data {
    uv_sem_t sem;

    struct dicey_packet response;
    enum dicey_error err;
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

static void client_reset_seq(struct dicey_client *const client) {
    assert(client);

    client->next_seq = 2U; // Do not restart from 0 - ever
}

static uint32_t client_next_seq(struct dicey_client *const client) {
    assert(client && !(client->next_seq % 2U));

    const uint32_t next = client->next_seq;

    client->next_seq += 2U;

    if (!client->next_seq) { // overflow
        client_reset_seq(client);
    }

    return next;
}

static bool client_event(struct dicey_client *client, int event, ...);

static struct dicey_task_error *client_task_send_oneshot(
    struct dicey_client *const client,
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_packet packet
) {
    if (packet.nbytes >= UINT_MAX) {
        return dicey_task_error_new(DICEY_EINVAL, "packet size too large");
    }

    return dicey_task_op_write(
        tloop, id, (uv_stream_t *) &client->pipe, uv_buf_init(packet.payload, (unsigned) packet.nbytes)
    );
}

static struct dicey_task_error *client_task_send_and_queue(
    struct dicey_client *const client,
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_packet packet
) {
    assert(packet.payload && packet.nbytes < UINT_MAX);

    struct dicey_task_error *const task_err = dicey_task_op_write_and_wait(
        tloop, id, (uv_stream_t *) &client->pipe, uv_buf_init(packet.payload, (unsigned) packet.nbytes)
    );

    if (task_err) {
        return task_err;
    }

    // register that we expect a response on this task for sequence number 0
    if (!dicey_waiting_list_append(&client->waiting_tasks, 0U, id)) {
        return dicey_task_error_new(DICEY_ENOMEM, "failed to register hello packet for response");
    }

    return NULL;
}

struct disconnect_context {
    struct dicey_client *client;

    struct dicey_packet bye;

    dicey_client_on_disconnect_fn *cb;
    void *cb_data;
};

static struct dicey_task_result send_bye(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) input;

    struct disconnect_context *const disconn_ctx = data;
    assert(disconn_ctx && disconn_ctx->client);

    struct dicey_client *const client = disconn_ctx->client;
    assert(uv_is_active((uv_handle_t *) &client->pipe) && client->state >= CLIENT_STATE_CONNECT_START);

    // craft a bye packet
    enum dicey_error craft_fail =
        dicey_packet_bye(&disconn_ctx->bye, client_next_seq(client), DICEY_BYE_REASON_SHUTDOWN);
    if (craft_fail) {
        return dicey_task_fail(craft_fail, "failed to craft bye packet");
    }

    struct dicey_task_error *const err = client_task_send_oneshot(client, tloop, id, disconn_ctx->bye);
    if (err) {
        return dicey_task_fail_with(err);
    }

    return dicey_task_continue();
}

static struct dicey_task_result issue_close(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) input;

    struct disconnect_context *const disconn_ctx = data;
    assert(disconn_ctx && disconn_ctx->client);

    struct dicey_client *const client = disconn_ctx->client;

    // note: this function can work standalone. Therefore, we must assert twice that the client is in a valid state
    assert(uv_is_active((uv_handle_t *) &client->pipe) && client->state >= CLIENT_STATE_CONNECT_START);

    struct dicey_task_error *const err = dicey_task_op_close(tloop, id, (uv_handle_t *) &client->pipe);
    if (err) {
        return dicey_task_fail_with(err);
    }

    return dicey_task_continue();
}

static void disconnect_end(const int64_t id, struct dicey_task_error *const err, void *const ctx) {
    (void) id;

    struct disconnect_context *const disconn_ctx = ctx;
    assert(disconn_ctx && disconn_ctx->client);

    struct dicey_client *const client = disconn_ctx->client;

    const enum dicey_error errcode = err ? err->error : DICEY_OK;

    if (errcode) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err->error, "%s", err->message);
    }

    client_event(client, DICEY_CLIENT_EVENT_QUIT);

    if (disconn_ctx->cb) {
        disconn_ctx->cb(client, disconn_ctx->cb_data, errcode);
    }

    free(disconn_ctx);
}

static const struct dicey_task_request disconnect_sequence = {
    .work = (dicey_task_loop_do_work_fn *[]) {&send_bye, issue_close, NULL},
    .at_end = &disconnect_end,
};

static enum dicey_error client_issue_disconnect(
    struct dicey_client *const client,
    dicey_client_on_disconnect_fn *const cb,
    void *const data
) {
    struct disconnect_context *const ctx = malloc(sizeof *ctx);
    if (!ctx) {
        return DICEY_ENOMEM;
    }

    *ctx = (struct disconnect_context) {
        .client = client,
        .cb = cb,
        .cb_data = data,
    };

    struct dicey_task_request *const disconnect_req = malloc(sizeof *disconnect_req);
    if (!disconnect_req) {
        free(ctx);

        return DICEY_ENOMEM;
    }

    *disconnect_req = disconnect_sequence;

    disconnect_req->ctx = ctx;
    disconnect_req->timeout_ms = DEFAULT_TIMEOUT;

    enum dicey_error err = dicey_task_loop_submit(client->tloop, disconnect_req);
    if (err) {
        free(disconnect_req);
        free(ctx);

        return err;
    }

    return DICEY_OK;
}

static void client_alloc_buffer(uv_handle_t *const handle, const size_t suggested_size, uv_buf_t *const buf) {
    (void) suggested_size; // useless, always 65k (max UDP packet size)

    struct dicey_client *const client = (struct dicey_client *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->recv_chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->recv_chunk);
}

static void client_got_packet(struct dicey_client *const client, struct dicey_packet packet) {
    assert(client && packet.payload && packet.nbytes);

    uint32_t seq_no = UINT32_MAX;
    enum dicey_error err = dicey_packet_get_seq(packet, &seq_no);
    if (err) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err, "failed to get sequence number from packet");

        goto cleanup;
    }

    struct dicey_bye bye;
    if (dicey_packet_as_bye(packet, &bye) == DICEY_OK) {
        client_event(client, DICEY_CLIENT_EVENT_SERVER_BYE, bye.reason);

        goto cleanup;
    }

    bool is_event = false;
    struct dicey_message msg;
    if (dicey_packet_as_message(packet, &msg) == DICEY_OK) {
        if (!is_server_msg(msg.type)) {
            client_event(
                client,
                DICEY_CLIENT_EVENT_ERROR,
                DICEY_EINVAL,
                "invalid message type sent by server: %s",
                dicey_op_to_string(msg.type)
            );

            goto cleanup;
        }

        client_event(client, DICEY_CLIENT_EVENT_MESSAGE_RECEIVING, packet);

        is_event = msg.type == DICEY_OP_EVENT;
    }

    if (is_event) {
        assert(client->on_event);

        client->on_event(client, dicey_client_get_context(client), packet);
    } else {
        // the packet is a response or hello, so it must match with something in our waiting list. If it doesn't, it may
        // have timed out
        uint64_t task_id = UINT64_MAX;
        if (dicey_waiting_list_remove_seq(client->waiting_tasks, seq_no, &task_id)) {
            // unlock the task

            dicey_task_loop_advance(client->tloop, task_id, &packet);

            goto cleanup;
        }
    }

cleanup:
    dicey_packet_deinit(&packet);
}

static void client_on_read(uv_stream_t *stream, const ssize_t nread, const struct uv_buf_t *const buf) {
    (void) buf; // unused

    struct dicey_client *const client = (struct dicey_client *) stream;
    assert(client);

    if (nread < 0) {
        if (nread != UV_EOF && client->state != CLIENT_STATE_DEAD) {
            const int uverr = (int) nread;

            client_event(
                client, DICEY_CLIENT_EVENT_ERROR, dicey_error_from_uv(uverr), "uv_read: %s", uv_strerror(uverr)
            );
        }

        return;
    }

    struct dicey_chunk *const chunk = client->recv_chunk;

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

        break;
    }
}

static enum dicey_error client_start_read(struct dicey_client *const client) {
    assert(client);

    assert(client->state == CLIENT_STATE_INIT);

    return dicey_error_from_uv(uv_read_start((uv_stream_t *) &client->pipe, &client_alloc_buffer, &client_on_read));
}

struct connect_context {
    struct dicey_client *client;
    struct dicey_addr addr;
    struct dicey_packet hello;

    dicey_client_on_connect_fn *cb;
    void *cb_data;
};

static struct dicey_task_result issue_connect(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) input;
    assert(tloop && !input); // no input expected

    struct connect_context *const ctx = data;
    assert(ctx && ctx->client && ctx->addr.addr && ctx->addr.len);

    struct dicey_client *const client = ctx->client;

    assert(client->state == CLIENT_STATE_INIT && !uv_is_active((uv_handle_t *) &client->pipe));

    struct dicey_task_error *const err = dicey_task_op_connect_pipe(tloop, id, &client->pipe, ctx->addr);

    return err ? dicey_task_fail_with(err) : dicey_task_continue();
}

static struct dicey_task_result send_first_hello(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) input;
    assert(tloop && !input); // no input expected

    struct connect_context *const ctx = data;
    assert(ctx && ctx->client);

    struct dicey_client *const client = ctx->client;

    // first, startup the read task, or nothing will ever work
    enum dicey_error err = client_start_read(client);
    if (err) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err, "failed to start read task");

        return dicey_task_fail(err, "failed to start read task");
    }

    // the hello packet always has a sequence number of 0
    err = dicey_packet_hello(&ctx->hello, 0U, DICEY_PROTO_VERSION_CURRENT);
    if (err) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err, "failed to create hello packet");

        return dicey_task_fail(err, "failed to create hello packet");
    }

    client_reset_seq(client);

    struct dicey_task_error *const queue_err = client_task_send_and_queue(client, tloop, id, ctx->hello);
    if (queue_err) {
        // packet is cleared by finalizer, don't worry about it

        return dicey_task_fail_with(queue_err);
    }

    client_event(client, DICEY_CLIENT_EVENT_HANDSHAKE_START);

    return dicey_task_continue();
}

static struct dicey_task_result verify_and_finish_connect(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) tloop;
    (void) id;
    assert(tloop && input);

    struct connect_context *const ctx = data;
    assert(ctx && ctx->client);

    struct dicey_packet packet = *(struct dicey_packet *) input;
    free(input);

    uint32_t seq_no = UINT32_MAX;
    enum dicey_error err = dicey_packet_get_seq(packet, &seq_no);
    if (err) {
        dicey_packet_deinit(&packet);

        return dicey_task_fail(err, "failed to get sequence number from packet");
    }

    // We currently don't concern ourself with packet versions.
    const bool is_hello = dicey_packet_get_kind(packet) == DICEY_PACKET_KIND_HELLO;
    dicey_packet_deinit(&packet);

    if (seq_no) {
        return dicey_task_fail(DICEY_EINVAL, "expected sequence number 0 from hello packet");
    }

    if (!is_hello) {
        return dicey_task_fail(DICEY_EINVAL, "expected hello packet");
    }

    struct dicey_client *const client = ctx->client;

    if (client->state != CLIENT_STATE_CONNECT_START) {
        return dicey_task_fail(DICEY_EINVAL, "invalid state for connect verification");
    }

    client_event(client, DICEY_CLIENT_EVENT_CONNECT);

    return dicey_task_continue();
}

static void connect_end(const int64_t id, struct dicey_task_error *const err, void *const ctx) {
    (void) id;
    struct connect_context *const connect_ctx = ctx;
    assert(connect_ctx && connect_ctx->client && connect_ctx->cb);

    struct dicey_client *const client = connect_ctx->client;

    const enum dicey_error errcode = err ? err->error : DICEY_OK;
    const char *const errmsg = err ? err->message : NULL;

    if (errcode) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err->error, "%s", errmsg);
    }

    connect_ctx->cb(client, connect_ctx->cb_data, errcode, errmsg);

    dicey_addr_deinit(&connect_ctx->addr);
    dicey_packet_deinit(&connect_ctx->hello);
    free(connect_ctx);
}

static const struct dicey_task_request connect_sequence = {
    .work = (dicey_task_loop_do_work_fn *[]) {&issue_connect, &send_first_hello, &verify_and_finish_connect, NULL},
    .at_end = &connect_end,
};

enum dicey_error client_issue_connect(
    struct dicey_client *const client,
    const struct dicey_addr addr,
    dicey_client_on_connect_fn *const cb,
    void *const data
) {
    struct connect_context *const ctx = malloc(sizeof *ctx);
    if (!ctx) {
        return DICEY_ENOMEM;
    }

    *ctx = (struct connect_context) {
        .client = client,
        .addr = addr,
        .cb = cb,
        .cb_data = data,
    };

    struct dicey_task_request *const connect_req = malloc(sizeof *connect_req);
    if (!connect_req) {
        free(ctx);

        return DICEY_ENOMEM;
    }

    *connect_req = connect_sequence;

    connect_req->ctx = ctx;
    connect_req->timeout_ms = DEFAULT_TIMEOUT;

    enum dicey_error err = dicey_task_loop_submit(client->tloop, connect_req);
    if (err) {
        free(connect_req);
        free(ctx);

        return err;
    }

    return DICEY_OK;
}

struct request_context {
    struct dicey_client *client;
    struct dicey_packet request, response;

    dicey_client_on_reply_fn *cb;
    void *cb_data;
};

static struct dicey_task_result issue_request(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) input;
    assert(tloop && !input); // no input expected, we get the packet from the context

    struct request_context *const ctx = data;
    assert(ctx && ctx->client);

    struct dicey_client *const client = ctx->client;
    assert(client->state == CLIENT_STATE_RUNNING);

    const struct dicey_packet packet = ctx->request;
    assert(dicey_packet_is_valid(packet));

    const uint32_t seq_no = client_next_seq(client);

    enum dicey_error seq_err = dicey_packet_set_seq(packet, seq_no);
    if (seq_err) {
        return dicey_task_fail(seq_err, "failed to set sequence number on request packet");
    }

    struct dicey_task_error *const err = client_task_send_and_queue(client, tloop, id, packet);

    return err ? dicey_task_fail_with(err) : dicey_task_continue();
}

static struct dicey_task_result check_response(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const data,
    void *const input
) {
    (void) tloop;
    (void) id;
    assert(tloop && input);

    struct request_context *const ctx = data;
    assert(ctx && ctx->client);

    ctx->response = *(struct dicey_packet *) input;
    assert(dicey_packet_is_valid(ctx->response));

    return dicey_task_continue();
}

static void request_end(const int64_t id, struct dicey_task_error *const err, void *const ctx) {
    (void) id;

    struct request_context *const req_ctx = ctx;
    assert(req_ctx && req_ctx->client);

    struct dicey_client *const client = req_ctx->client;

    const enum dicey_error errcode = err ? err->error : DICEY_OK;

    if (errcode) {
        client_event(client, DICEY_CLIENT_EVENT_ERROR, err->error, "%s", err->message);
    }

    assert(req_ctx->cb);

    req_ctx->cb(client, req_ctx->cb_data, errcode, req_ctx->response);

    dicey_packet_deinit(&req_ctx->request);
    dicey_packet_deinit(&req_ctx->response);
    free(req_ctx);
}

static const struct dicey_task_request request_sequence = {
    .work = (dicey_task_loop_do_work_fn *[]) {&issue_request, &check_response, NULL},
    .at_end = &request_end,
};

static enum dicey_error client_issue_request(
    struct dicey_client *const client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *const cb,
    void *const data,
    uint32_t timeout
) {
    struct dicey_task_request *const req = malloc(sizeof *req);
    if (!req) {
        return DICEY_ENOMEM;
    }

    *req = request_sequence;

    struct request_context *const ctx = malloc(sizeof *ctx);
    if (!ctx) {
        free(req);

        return DICEY_ENOMEM;
    }

    *ctx = (struct request_context) {
        .client = client,
        .request = packet,
        .cb = cb,
        .cb_data = data,
    };

    req->ctx = ctx;
    req->timeout_ms = timeout;

    enum dicey_error err = dicey_task_loop_submit(client->tloop, req);
    if (err) {
        free(req);
        free(ctx);

        return err;
    }

    return DICEY_OK;
}

static bool client_process_event(
    struct dicey_client_event *const ev,
    struct dicey_client *const client,
    const int event,
    ...
);

static void client_set_state(struct dicey_client *const client, const enum client_state state) {
    assert(client && state >= CLIENT_STATE_UNINIT && state <= CLIENT_STATE_CLOSED);

    assert(client->state <= state); // state transitions must be forward

    client->state = state;
}

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
        assert(client->state == CLIENT_STATE_CONNECT_START);

        client_set_state(client, CLIENT_STATE_RUNNING);

        break;

    case DICEY_CLIENT_EVENT_ERROR:
        {
            ev->error.err = va_arg(args, enum dicey_error);
            const char *const fmt = va_arg(args, const char *);

            (void) vasprintf(&ev->error.msg, fmt, args);

            client_set_state(client, CLIENT_STATE_DEAD);

            break;
        }

    case DICEY_CLIENT_EVENT_HANDSHAKE_START:
        assert(client->state == CLIENT_STATE_INIT);

        ev->version = va_arg(args, struct dicey_version);

        client_set_state(client, CLIENT_STATE_CONNECT_START);

        break;

    case DICEY_CLIENT_EVENT_INIT:
        assert(client->state == CLIENT_STATE_UNINIT);

        client_set_state(client, CLIENT_STATE_INIT);

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
            ev->packet = va_arg(args, struct dicey_packet);
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
            ev->packet = va_arg(args, struct dicey_packet);
        }

        break;

    case DICEY_CLIENT_EVENT_SERVER_BYE:
        {
            assert(client->state >= CLIENT_STATE_CONNECT_START);

            const enum dicey_bye_reason bye_reason = va_arg(args, enum dicey_bye_reason);
            if (bye_reason == DICEY_BYE_REASON_ERROR) {
                return client_process_event(ev, client, DICEY_CLIENT_EVENT_ERROR, "kicked by server");
            } else {
                // raise the event
                client_set_state(client, CLIENT_STATE_DEAD);
            }

            break;
        }

    case DICEY_CLIENT_EVENT_QUITTING:
        {
            assert(client->state <= CLIENT_STATE_CLOSING);

            client_set_state(client, CLIENT_STATE_CLOSING);

            break;
        }

    case DICEY_CLIENT_EVENT_QUIT:
        {
            assert(client->state == CLIENT_STATE_CLOSING);

            client_set_state(client, CLIENT_STATE_CLOSED);

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

    if (ev.type == DICEY_CLIENT_EVENT_ERROR) {
        free(ev.error.msg);
    }

    return res;
}

void dicey_client_delete(struct dicey_client *const client) {
    if (client) {
        dicey_task_loop_delete(client->tloop);

        free(client);
    }
}

static void unlock_after_connect(
    struct dicey_client *const client,
    void *const data,
    const enum dicey_error err,
    const char *const errmsg
) {
    (void) client;
    (void) errmsg;

    assert(data);

    struct sync_conn_data *const sync_data = data;

    sync_data->err = err;

    uv_sem_post(&sync_data->sem); // unlock the waiting thread
}

static void unlock_after_disconnect(struct dicey_client *const client, void *const data, const enum dicey_error err) {
    (void) client;

    assert(data);

    struct sync_disconn_data *const sync_data = data;

    sync_data->err = err;

    uv_sem_post(&sync_data->sem); // unlock the waiting thread
}

static void unlock_after_request(
    struct dicey_client *const client,
    void *const data,
    const enum dicey_error err,
    const struct dicey_packet response
) {
    (void) client;

    assert(data && dicey_packet_is_valid(response));

    struct sync_req_data *const sync_data = data;

    sync_data->err = err;
    sync_data->response = response;

    uv_sem_post(&sync_data->sem); // unlock the waiting thread
}

static void clean_up_task(void *const ctx, const int64_t id, struct dicey_task_error *const err) {
    (void) err;

    assert(ctx);

    struct dicey_client *const client = ctx;

    // remove the completed task from the waiting list (if any)
    // todo: do this only when timing out
    (void) dicey_waiting_list_remove_task(client->waiting_tasks, id, NULL);
}

static void reset_state(void *const ctx) {
    struct dicey_client *const client = ctx;
    assert(client && uv_is_closing((uv_handle_t *) &client->pipe) && !dicey_task_loop_is_running(client->tloop));

    client_set_state(client, CLIENT_STATE_DEAD);

    client->next_seq = 0U;
    client->pipe = (uv_pipe_t) { 0 };

    dicey_waiting_list_clear(client->waiting_tasks);
    dicey_chunk_clear(client->recv_chunk);

    dicey_task_loop_delete(client->tloop);
    client->tloop = NULL;
}

enum dicey_error dicey_client_new(struct dicey_client **const dest, const struct dicey_client_args *const args) {
    assert(dest);

    struct dicey_client *const client = calloc(1U, sizeof *client);
    if (!client) {
        return DICEY_ENOMEM;
    }

    if (args) {
        client->inspect_func = args->inspect_func;
        client->on_event = args->on_event;
    }

    client_event(client, DICEY_CLIENT_EVENT_INIT);

    *dest = client;

    return DICEY_OK;
}

enum dicey_error dicey_client_connect(struct dicey_client *const client, const struct dicey_addr addr) {
    assert(client && addr.addr);

    struct sync_conn_data data = { .err = DICEY_OK };
    uv_sem_init(&data.sem, 0);

    enum dicey_error conn_err = dicey_client_connect_async(client, addr, &unlock_after_connect, &data);
    if (conn_err) {
        uv_sem_destroy(&data.sem);
        return conn_err;
    }

    uv_sem_wait(&data.sem);
    uv_sem_destroy(&data.sem);

    return data.err;
}

enum dicey_error dicey_client_connect_async(
    struct dicey_client *const client,
    const struct dicey_addr addr,
    dicey_client_on_connect_fn *const cb,
    void *const data
) {
    assert(client && addr.addr);

    if (client->state != CLIENT_STATE_INIT) {
        return DICEY_EINVAL;
    }

    assert(!client->tloop);

    enum dicey_error err = dicey_task_loop_new(
        &client->tloop,
        &(struct dicey_task_loop_args) {
            .global_at_end = &clean_up_task,
            .global_stopped = &reset_state,
        }
    );

    if (err) {
        free(client);

        return err;
    }

    dicey_task_loop_set_context(client->tloop, client);

    err = dicey_task_loop_start(client->tloop);
    if (err) {
        return err;
    }

    return client_issue_connect(client, addr, cb, data);
}

void *dicey_client_get_context(const struct dicey_client *client) {
    assert(client);

    return client->ctx;
}

bool dicey_client_is_running(const struct dicey_client *const client) {
    assert(client);

    return client->state == CLIENT_STATE_RUNNING;
}

enum dicey_error dicey_client_disconnect(struct dicey_client *const client) {
    assert(client);

    struct sync_disconn_data data = { .err = DICEY_OK };
    uv_sem_init(&data.sem, 0);

    enum dicey_error disconn_err = dicey_client_disconnect_async(client, &unlock_after_disconnect, &data);
    if (disconn_err) {
        uv_sem_destroy(&data.sem);
        return disconn_err;
    }

    uv_sem_wait(&data.sem);
    uv_sem_destroy(&data.sem);

    return data.err;
}

enum dicey_error dicey_client_disconnect_async(
    struct dicey_client *const client,
    dicey_client_on_disconnect_fn *const cb,
    void *const data
) {
    assert(client);

    if (client->state != CLIENT_STATE_RUNNING) {
        return DICEY_EINVAL;
    }

    return client_issue_disconnect(client, cb, data);
}

enum dicey_error dicey_client_request(
    struct dicey_client *const client,
    const struct dicey_packet packet,
    struct dicey_packet *const response,
    const uint32_t timeout_ms
) {
    assert(client && response && dicey_packet_is_valid(packet));

    struct sync_req_data data = { .err = DICEY_OK };
    uv_sem_init(&data.sem, 0);

    enum dicey_error req_err = dicey_client_request_async(client, packet, &unlock_after_request, &data, timeout_ms);
    if (req_err) {
        uv_sem_destroy(&data.sem);
        return req_err;
    }

    uv_sem_wait(&data.sem);

    uv_sem_destroy(&data.sem);

    *response = data.response;

    return data.err;
}

enum dicey_error dicey_client_request_async(
    struct dicey_client *const client,
    const struct dicey_packet packet,
    dicey_client_on_reply_fn *const cb,
    void *const data,
    const uint32_t timeout
) {
    assert(client && dicey_packet_is_valid(packet) && cb);

    if (client->state != CLIENT_STATE_RUNNING) {
        return DICEY_EINVAL;
    }

    return client_issue_request(client, packet, cb, data, timeout);
}

void *dicey_client_set_context(struct dicey_client *const client, void *const data) {
    assert(client && client->state == CLIENT_STATE_INIT);

    void *const old = client->ctx;

    client->ctx = data;

    return old;
}
