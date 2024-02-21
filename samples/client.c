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

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/getopt.h"
#include "util/packet-dump.h"
#include "util/packet-json.h"
#include "util/packet-xml.h"
#include "util/uvtools.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#if defined(__linux__)
#define HAS_ABSTRACT_SOCKETS 1
#else
#define HAS_ABSTRACT_SOCKETS 0
#endif

enum load_mode {
    LOAD_MODE_INVALID,
    LOAD_MODE_JSON,
    LOAD_MODE_XML,
};

enum client_state {
    CLIENT_STATE_UNINIT,
    CLIENT_STATE_INIT,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_HELLO_SENT,
    CLIENT_STATE_RUNNING,

    CLIENT_STATE_DEAD,
    CLIENT_STATE_CONN_FAIL,
};

struct send_request {
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

struct dicey_client;

typedef void at_exit_fn(struct dicey_client *client);
typedef void on_connect_fn(struct dicey_client *client);
typedef void on_disconnect_fn(struct dicey_client *client);
typedef void on_error_fn(const struct dicey_client *client, enum dicey_error err, const char *msg, ...);
typedef void on_message_recv_fn(struct dicey_client *client, struct dicey_packet packet);
typedef void on_message_sent_fn(struct dicey_client *client, struct dicey_packet packet);

static bool is_server_msg(const enum dicey_op op) {
    switch (op) {
    case DICEY_OP_RESPONSE:
    case DICEY_OP_EVENT:
        return true;

    default:
        return false;
    }
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

    return uv_buf_init(cnk->bytes + cnk->len, avail > UINT_MAX ? UINT_MAX : (unsigned int) avail);
}

static void chunk_clear(struct growable_chunk *const buffer) {
    assert(buffer);

    buffer->len = 0;
}

struct dicey_client_args {
    at_exit_fn *at_exit;
    on_connect_fn *on_connect;
    on_disconnect_fn *on_disconnect;
    on_error_fn *on_error;
    on_message_recv_fn *on_message_recv;
    on_message_sent_fn *on_message_sent;
};

struct dicey_client {
    uv_pipe_t pipe;

    uv_thread_t thread;

    at_exit_fn *at_exit;
    on_connect_fn *on_connect;
    on_disconnect_fn *on_disconnect;
    on_error_fn *on_error;
    on_message_recv_fn *on_message_recv;
    on_message_sent_fn *on_message_sent;

    enum client_state state;
    uint32_t seq_cnt; // client-initiated packets are even, server-initiated are odd

    uv_loop_t loop;
    uv_async_t async;

    struct growable_chunk *chunk;

    struct request_queue queue;

    void *data;
};

static void dicey_client_delete(struct dicey_client *const client) {
    if (client) {
        uv_close((uv_handle_t *) &client->pipe, NULL);

        uv_loop_close(&client->loop);

        free(client);
    }
}

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

struct write_request {
    uv_write_t req;

    struct dicey_client *client;
    struct dicey_packet packet;
};

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
        client_quit(client, util_uverr_to_dicey(status));
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

    struct send_request req;
    while (request_queue_pop(&client->queue, &req, LOCKING_POLICY_NONBLOCKING)) {
        struct write_request *const write_req = malloc(sizeof *write_req);
        if (!write_req) {
            dicey_packet_deinit(&req.packet);

            continue;
        }

        if (req.packet.nbytes > UINT_MAX) {
            client_quit(client, DICEY_EOVERFLOW);

            free(write_req);
            dicey_packet_deinit(&req.packet);

            continue;
        }

        uv_buf_t buf = uv_buf_init(req.packet.payload, (unsigned int) req.packet.nbytes);

        *write_req = (struct write_request) {
            .client = client,
            .packet = req.packet,
        };

        const int err = uv_write((uv_write_t *) &write_req, (uv_stream_t *) &client->pipe, &buf, 1, &on_write);
        if (err) {
            client->on_error(client, util_uverr_to_dicey(err), "uv_write: %s", uv_strerror(err));

            free(write_req);
            dicey_packet_deinit(&req.packet);
        }
    }
}

static enum dicey_error dicey_client_new(struct dicey_client **const dest, const struct dicey_client_args *const args) {
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
        err = util_uverr_to_dicey(uverr);

        goto free_struct;
    }

    uverr = request_queue_init(&client->queue);
    if (uverr < 0) {
        err = util_uverr_to_dicey(uverr);

        goto free_loop;
    }

    uverr = uv_async_init(&client->loop, &client->async, &data_available);
    if (uverr < 0) {
        err = util_uverr_to_dicey(uverr);

        goto free_queue;
    }

    client->async.data = client;

    uverr = uv_pipe_init(&client->loop, &client->pipe, 0);
    if (uverr) {
        err = util_uverr_to_dicey(uverr);

        goto free_async;
    }

    *dest = client;

    return DICEY_OK;

free_async:
    uv_close((uv_handle_t *) &client->async, NULL);

free_queue:
    request_queue_deinit(&client->queue);

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

    return util_uverr_to_dicey(uv_write((uv_write_t *) req, (uv_stream_t *) client, &buf, 1, &on_write));

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

    *buf = chunk_get_buf(&client->chunk, READ_MINBUF);

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
        client_quit(client, nread == UV_EOF ? DICEY_OK : util_uverr_to_dicey((int) nread));

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

static void on_connect(uv_connect_t *const conn, const int status) {
    assert(conn && conn->data);

    struct dicey_client *const client = conn->data;
    free(conn);

    if (status < 0) {
        client->on_error(client, util_uverr_to_dicey(status), "uv_connect: %s", uv_strerror(status));

        client->state = CLIENT_STATE_CONN_FAIL;

        uv_close((uv_handle_t *) client, &on_close);

        return;
    }

    client->state = CLIENT_STATE_CONNECTING;

    const int read_err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    if (read_err < 0) {
        client_quit(client, util_uverr_to_dicey(status));

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

        client->on_error(client, util_uverr_to_dicey(err), "uv_pipe_connect2: %s\n", uv_strerror(err));
    }

    err = uv_run(&client->loop, UV_RUN_DEFAULT);
    if (err < 0) {
        client->on_error(client, util_uverr_to_dicey(err), "uv_run: %s\n", uv_strerror(err));
    }

    free(addr);

    if (client->at_exit) {
        client->at_exit(client);
    }
}

struct dicey_addr {
    const char *addr;
    size_t len;
};

static struct dicey_addr dicey_addr_from_str(const char *const str) {
    assert(str);

    return (struct dicey_addr) {
        .addr = str,
        .len = strlen(str),
    };
}

static enum dicey_error dicey_client_connect(struct dicey_client *const client, const struct dicey_addr addr) {
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
    char *const addr_copy = strdup(addr);
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

        return util_uverr_to_dicey(thread_res);
    }

    return DICEY_OK;
}

static enum dicey_error dicey_client_send(struct dicey_client *const client, struct dicey_packet packet) {
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

    (void) request_queue_push(
        &client->queue,
        (struct send_request) {
            .packet = packet,
        },
        LOCKING_POLICY_BLOCKING
    );

    return util_uverr_to_dicey(uv_async_send(&client->async));
}

struct recv_data {
    uint32_t seq;

    uv_sem_t sem;
};

static struct recv_data *make_data(void) {
    struct recv_data *const data = calloc(1U, sizeof *data);
    if (!data) {
        return NULL;
    }

    const int err = uv_sem_init(&data->sem, 0);
    if (err < 0) {
        free(data);

        return NULL;
    }

    return data;
}

static void at_client_exit(struct dicey_client *const client) {
    struct recv_data *const data = client->data;
    if (data) {
        uv_sem_post(&data->sem);
    }
}

static void on_client_connect(struct dicey_client *const client) {
    (void) client;

    puts("info: client connected");
}

static void on_client_disconnect(struct dicey_client *const cln) {
    (void) cln;

    puts("info: client disconnected");
}

static void on_client_error(
    const struct dicey_client *const client,
    const enum dicey_error err,
    const char *const msg,
    ...
) {
    (void) client;

    va_list args;
    va_start(args, msg);

    fprintf(stderr, "%sError (%s):", dicey_error_name(err), dicey_error_msg(err));

    vfprintf(stderr, msg, args);
    fputc('\n', stderr);

    va_end(args);
}

static void on_message_received(struct dicey_client *const cln, struct dicey_packet packet) {
    (void) cln;

    puts("info: received packet from server");

    struct util_dumper dumper = util_dumper_for(stdout);

    util_dumper_dump_packet(&dumper, packet);

    uint32_t seq = 0U;
    const enum dicey_error err = dicey_packet_get_seq(packet, &seq);
    if (err) {
        abort(); // unreachable, dicey_packet_is_valid guarantees a valid packet
    }

    struct recv_data *const data = cln->data;
    assert(data);

    if (seq == data->seq) {
        // unlock main thread
        uv_sem_post(&data->sem);
    }

    dicey_packet_deinit(&packet);
}

static void on_message_sent(struct dicey_client *const cln, struct dicey_packet packet) {
    (void) cln;

    uint32_t seq = 0U;

    const enum dicey_error err = dicey_packet_get_seq(packet, &seq);
    if (err) {
        abort(); // unreachable
    }

    struct recv_data *const data = cln->data;
    assert(data);

    data->seq = seq;

    puts("info: sent packet to server");

    struct util_dumper dumper = util_dumper_for(stdout);

    util_dumper_dump_packet(&dumper, packet);
}

static int do_send(char *addr, struct dicey_packet packet) {
    struct dicey_client *client = NULL;

    enum dicey_error err = dicey_client_new(
        &client,
        &(struct dicey_client_args) {
            .at_exit = &at_client_exit,
            .on_connect = &on_client_connect,
            .on_disconnect = &on_client_disconnect,
            .on_error = &on_client_error,
            .on_message_recv = &on_message_received,
            .on_message_sent = &on_message_sent,
        }
    );

    if (err) {
        return err;
    }

    client->data = make_data();
    err = dicey_client_connect(client, dicey_addr_from_str(addr));
    if (err) {
        free(client->data);
        dicey_client_delete(client);
        dicey_packet_deinit(&packet);

        return err;
    }

    err = dicey_client_send(client, packet);
    if (err) {
        free(client->data);
        dicey_client_delete(client);
        dicey_packet_deinit(&packet);
    }

    uv_sem_wait(&((struct recv_data *) client->data)->sem);

    return DICEY_OK;
}

static void print_xml_errors(const struct util_xml_errors *const errs) {
    if (!errs->errors) {
        return;
    }

    const struct util_xml_error *const end = *errs->errors + errs->nerrs;
    for (const struct util_xml_error *err = *errs->errors; err < end; ++err) {
        fputs("error in XML input:", stderr);

        if (err->line) {
            fprintf(stderr, "line %d: ", err->line);

            if (err->col) {
                fprintf(stderr, ", col %d: ", err->col);
            }
        } else {
            fputc(' ', stderr);
        }

        fprintf(stderr, "%s\n", err->message);
    }
}

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...] SOCKET [FILE]\n"                                                                           \
    "  -j  load FILE or stdin as a JSON-encoded packet\n"                                                              \
    "  -h  print this help message and exit\n"                                                                         \
    "  -x  load FILE or stdin as an XML-encoded packet\n"                                                              \
    "\n"                                                                                                               \
    "If not specified, FILE defaults to stdin. The extension is used to probe the contents of the file.\n"             \
    "Any SEQ parameter will be ignored.\n"

static enum load_mode file_probe(const char *const path) {
    const char *const ext = strrchr(path, '.');
    if (ext) {
        if (!strcmp(ext, ".json")) {
            return LOAD_MODE_JSON;
        }

        if (!strcmp(ext, ".xml")) {
            return LOAD_MODE_XML;
        }
    }

    return LOAD_MODE_INVALID;
}

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    const char *fin = NULL;
    char *socket = NULL;
    enum load_mode mode = LOAD_MODE_INVALID;

    int opt = 0;

    while ((opt = getopt(argc, argv, "jhx")) != -1) {
        switch (opt) {
        case 'j':
            mode = LOAD_MODE_JSON;
            break;

        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case 'x':
            mode = LOAD_MODE_XML;
            break;

        case '?':
            if (optopt == 'o') {
                fputs("error: -o requires an argument\n", stderr);
            } else {
                fprintf(stderr, "error: unknown option -%c\n", optopt);
            }

            print_help(progname, stderr);
            return EXIT_FAILURE;

        default:
            abort();
        }
    }

    switch (argc - optind) {
    case 0:
        fputs("error: missing socket or pipe name\n", stderr);
        return EXIT_FAILURE;

    case 2:
        fin = argv[optind + 1];

        if (mode == LOAD_MODE_INVALID) {
            mode = file_probe(fin);
        }

        // fallthrough

    case 1:
        socket = argv[optind];
        break;

    default:
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    if (mode == LOAD_MODE_INVALID) {
        fputs("error: no input mode specified and no file format can be determined from file name\n", stderr);

        return EXIT_FAILURE;
    }

    FILE *in = stdin;
    if (fin) {
        in = fopen(fin, "r");

        if (!in) {
            perror("fopen");
            return EXIT_FAILURE;
        }
    }

    uint8_t *dumped_bytes = NULL;
    size_t nbytes = 0, bcap = 0;

    while (!feof(in)) {
        uint8_t buf[4096];
        const size_t n = fread(buf, 1, sizeof buf, in);
        if (!n) {
            break;
        }

        const size_t new_len = nbytes + n;
        if (new_len > bcap) {
            bcap += sizeof buf;
            dumped_bytes = realloc(dumped_bytes, bcap);
            if (!dumped_bytes) {
                abort(); // this silences cppcheck, we don't care about safety in this dummy program
            }
        }

        memcpy(dumped_bytes + nbytes, buf, n);
        nbytes = new_len;
    }

    if (!nbytes || !dumped_bytes) {
        fputs("error: no input\n", stderr);
        return EXIT_FAILURE;
    }

    struct dicey_packet pkt = { 0 };

    enum dicey_error err = DICEY_OK;

    switch (mode) {
    case LOAD_MODE_JSON:
        err = util_json_to_dicey(&pkt, dumped_bytes, nbytes);
        break;

    case LOAD_MODE_XML:
        {
            struct util_xml_errors errs = util_xml_to_dicey(&pkt, dumped_bytes, nbytes);
            if (errs.nerrs) {
                print_xml_errors(&errs);
                util_xml_errors_deinit(&errs);

                err = DICEY_EINVAL; // kinda sucks?
            }
            break;
        }

    default:
        abort();
    }

    if (err) {
        goto quit;
    }

    err = do_send(socket, pkt);

    pkt = (struct dicey_packet) { 0 };

quit:
    dicey_packet_deinit(&pkt);
    free(dumped_bytes);
    fclose(in);

    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
