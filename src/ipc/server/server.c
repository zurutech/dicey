/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _CRT_NONSTDC_NO_DEPRECATE 1

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

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/typedescr.h>
#include <dicey/core/value.h>
#include <dicey/ipc/address.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/server.h>
#include <dicey/ipc/traits.h>

#include "sup/assume.h"
#include "sup/trace.h"

#include "ipc/chunk.h"
#include "ipc/elemdescr.h"
#include "ipc/queue.h"
#include "ipc/uvtools.h"

#include "builtins/builtins.h"

#include "client-data.h"
#include "pending-reqs.h"
#include "shared-packet.h"

#define DICEY_SET_RESPONSE_SIG                                                                                         \
    (const char[]) { (char) DICEY_TYPE_UNIT, '\0' }

enum loop_request_kind {
    LOOP_REQ_ADD_OBJECT,
    LOOP_REQ_ADD_TRAIT,
    LOOP_REQ_DEL_OBJECT,
    LOOP_REQ_RAISE_EVENT,
    LOOP_REQ_SEND_RESPONSE,
    LOOP_REQ_KICK_CLIENT,
    LOOP_REQ_STOP_SERVER,
};

struct loop_request {
    enum loop_request_kind kind;

    ptrdiff_t target;

    uv_sem_t *sem;
    enum dicey_error err;

    union {
        enum dicey_bye_reason kick_reason;

        struct {
            const char *name;
            struct dicey_hashset *traits;
        } object_info;

        struct dicey_trait *trait;

        struct dicey_packet packet;
    };
};

enum server_state {
    SERVER_STATE_UNINIT,
    SERVER_STATE_INIT,
    SERVER_STATE_RUNNING,
    SERVER_STATE_QUITTING,
};

struct outbound_packet {
    enum dicey_op kind;

    union {
        struct dicey_shared_packet *shared;
        struct dicey_packet single;
    };
};

static struct dicey_packet outbound_packet_borrow(const struct outbound_packet packet) {
    switch (packet.kind) {
    case DICEY_OP_RESPONSE:
        return packet.single;

    case DICEY_OP_EVENT:
        return dicey_shared_packet_borrow(packet.shared);

    default:
        assert(false);

        return (struct dicey_packet) { 0 };
    }
}

// utility function that either deallocates or decrements the refcount of the packet, depending if it's shared or not
static void outbound_packet_cleanup(struct outbound_packet *const packet) {
    assert(packet);

    switch (packet->kind) {
    case DICEY_OP_RESPONSE:
        dicey_packet_deinit(&packet->single);

        break;

    case DICEY_OP_EVENT:
        dicey_shared_packet_unref(packet->shared);

        break;

    default:
        assert(false);

        break;
    }

    *packet = (struct outbound_packet) { 0 };
}

#if !defined(NDEBUG)
static bool outbound_packet_is_valid(const struct outbound_packet packet) {
    switch (packet.kind) {
    case DICEY_OP_RESPONSE:
        return dicey_packet_is_valid(packet.single);

    case DICEY_OP_EVENT:
        return dicey_shared_packet_is_valid(packet.shared);

    default:
        assert(false);

        return false;
    }
}
#endif

static void *outbound_packet_payload(const struct outbound_packet packet) {
    switch (packet.kind) {
    case DICEY_OP_RESPONSE:
        return packet.single.payload;

    case DICEY_OP_EVENT:
        return dicey_shared_packet_borrow(packet.shared).payload;

    default:
        assert(false);

        return NULL;
    }
}

static size_t outbound_packet_size(const struct outbound_packet packet) {
    switch (packet.kind) {
    case DICEY_OP_RESPONSE:
        return packet.single.nbytes;

    case DICEY_OP_EVENT:
        return dicey_shared_packet_size(packet.shared);

    default:
        assert(false);

        return 0U;
    }
}

struct write_request {
    uv_write_t req;
    struct dicey_server *server;

    ptrdiff_t client_id;

    enum dicey_packet_kind kind;

    struct outbound_packet packet;
};

struct dicey_server {
    // first member is the uv_pipe_t to allow for type punning
    uv_pipe_t pipe;

    _Atomic enum server_state state;

    uint32_t seq_cnt; // used for ALL server-initiated packets. Starts with 1, and will roll over after UINT32_MAX.

    // super ugly way to unlock the callers of dicey_server_stop when the server is actually stopped
    // avoiding this would probably require to use the same task system the client uses - which is way more complex than
    // whatever the server needs
    uv_sem_t *shutdown_hook;

    uv_loop_t loop;
    uv_async_t async;

    struct dicey_queue queue;

    dicey_server_on_connect_fn *on_connect;
    dicey_server_on_disconnect_fn *on_disconnect;
    dicey_server_on_error_fn *on_error;
    dicey_server_on_request_fn *on_request;

    struct dicey_client_list *clients;
    struct dicey_registry registry;

    struct dicey_view_mut scratchpad;

    void *ctx;
};

static void loop_request_delete(void *const ctx, void *const ptr) {
    (void) ctx;

    struct loop_request *const req = ptr;
    if (req) {
        dicey_packet_deinit(&req->packet);
        free(req);
    }
}

static void on_write(uv_write_t *req, int status);

static bool is_event_msg(const struct dicey_packet pkt) {
    struct dicey_message msg = { 0 };
    if (dicey_packet_as_message(pkt, &msg) != DICEY_OK) {
        return false;
    }

    return msg.type == DICEY_OP_EVENT;
}

static bool can_send_as_event(const struct dicey_packet packet) {
    if (!dicey_packet_is_valid(packet)) {
        return false;
    }

    // all seqs will do - we will set a new one anyway
    return is_event_msg(packet);
}

static enum dicey_error is_message_acceptable_for(
    const struct dicey_element elem,
    const struct dicey_message *const msg
) {
    assert(msg);

    switch (msg->type) {
    case DICEY_OP_GET:
        if (elem.type != DICEY_ELEMENT_TYPE_PROPERTY) {
            return DICEY_EINVAL;
        }

        // for GET, skip signature validation
        return DICEY_OK;

    case DICEY_OP_SET:
        if (elem.type != DICEY_ELEMENT_TYPE_PROPERTY) {
            return DICEY_EINVAL;
        }

        if (elem.readonly) {
            return DICEY_EPROPERTY_READ_ONLY;
        }

        break;

    case DICEY_OP_EXEC:
        if (elem.type != DICEY_ELEMENT_TYPE_OPERATION) {
            return DICEY_EINVAL;
        }

        break;

    case DICEY_OP_EVENT:
        if (elem.type != DICEY_ELEMENT_TYPE_SIGNAL) {
            return DICEY_EINVAL;
        }

        break;

    case DICEY_OP_RESPONSE:
        return false; // never ok, only server can send responses

    default:
        assert(false);

        return DICEY_EINVAL;
    }

    return dicey_value_is_compatible_with(&msg->value, elem.signature) ? DICEY_OK : DICEY_ESIGNATURE_MISMATCH;
}

static bool is_server_op(const enum dicey_op op) {
    switch (op) {
    case DICEY_OP_RESPONSE:
    case DICEY_OP_EVENT:
        return true;

    default:
        return false;
    }
}

static bool is_response_msg(const struct dicey_packet pkt) {
    struct dicey_message msg = { 0 };
    if (dicey_packet_as_message(pkt, &msg) != DICEY_OK) {
        return false;
    }

    return msg.type == DICEY_OP_RESPONSE;
}

static bool can_send_as_response(const struct dicey_packet packet) {
    if (!dicey_packet_is_valid(packet)) {
        return false;
    }

    uint32_t seq = UINT32_MAX;
    const enum dicey_error err = dicey_packet_get_seq(packet, &seq);

    // disallow sending packets with seq number 0
    return !err && seq && is_response_msg(packet);
}

static enum dicey_error make_error(
    struct dicey_packet *const dest,
    uint32_t seq,
    const char *const path,
    const struct dicey_selector sel,
    const enum dicey_error msg_err
) {
    assert(dest && path && dicey_selector_is_valid(sel) && msg_err);

    return dicey_packet_message(dest, seq, DICEY_OP_RESPONSE, path, sel, (struct dicey_arg) {
        .type = DICEY_TYPE_ERROR,
        .error = {
            .code = (uint16_t) msg_err,
            .message = dicey_error_msg(msg_err),
        },
    });
}

static enum dicey_error server_sendpkt(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    struct outbound_packet packet
) {
    assert(server && client && outbound_packet_is_valid(packet));

    const size_t nbytes = outbound_packet_size(packet);
    if (nbytes > UINT_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    enum dicey_error err = DICEY_OK;

    struct write_request *const req = malloc(sizeof *req);
    if (!req) {
        err = TRACE(DICEY_ENOMEM);

        goto fail;
    }

    *req = (struct write_request) {
        .server = server,
        .client_id = client->info.id,
        .packet = packet,
    };

    void *const payload = outbound_packet_payload(packet);
    assert(payload);

    uv_buf_t buf = uv_buf_init(payload, (unsigned int) nbytes);

    return dicey_error_from_uv(uv_write((uv_write_t *) req, (uv_stream_t *) client, &buf, 1, &on_write));

fail:
    free(req);

    return err;
}

static enum dicey_error client_send_response(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    struct dicey_packet packet,
    const struct dicey_message *const msg // packet, parsed as msg
) {
    assert(server && client && msg);

    enum dicey_error err = DICEY_OK;

    if (msg->type != DICEY_OP_RESPONSE) {
        dicey_packet_deinit(&packet);

        err = TRACE(DICEY_EINVAL);

        goto quit;
    }

    uint32_t seq = 0U;

    err = dicey_packet_get_seq(packet, &seq);
    if (err) {
        goto quit;
    }

    // match the packet back with the request
    struct dicey_pending_request req = { 0 };

    err = dicey_pending_requests_complete(client->pending, seq, &req);
    if (err) {
        goto quit;
    }

    // if the request was a set, the response must have a unit signature, while in all other cases, the response
    // must have the same signature as the request
    const char *sig = req.op == DICEY_OP_SET ? DICEY_SET_RESPONSE_SIG : req.signature;

    if (!dicey_value_can_be_returned_from(&msg->value, sig)) {
        err = TRACE(DICEY_ESIGNATURE_MISMATCH);

        goto quit;
    }

    err = server_sendpkt(
        server,
        client,
        (struct outbound_packet) {
            .kind = DICEY_OP_RESPONSE,
            .single = packet, // responses are always single packets
        }
    );

quit:
    if (err) {
        dicey_packet_deinit(&packet);
    }

    return err;
}

#define READ_MINBUF 256U // 256B

static void on_client_end(uv_handle_t *const handle) {
    struct dicey_client_data *const client = (struct dicey_client_data *) handle;

    if (client->parent->on_disconnect) {
        client->parent->on_disconnect(client->parent, &client->info);
    }

    dicey_client_data_delete(client);
}

static ptrdiff_t server_add_client(struct dicey_server *const server, struct dicey_client_data **const dest) {
    struct dicey_client_data **client_bucket = NULL;
    size_t id = 0U;

    if (!dicey_client_list_new_bucket(&server->clients, &client_bucket, &id)) {
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_client_data *const client = *client_bucket = dicey_client_data_new(server, id);
    if (!client) {
        return TRACE(DICEY_ENOMEM);
    }

    if (uv_pipe_init(&server->loop, &client->pipe, 0)) {
        free(client);

        return TRACE(DICEY_EUV_UNKNOWN);
    }

    *dest = client;

    return client->info.id;
}

static enum dicey_error server_submit_request(struct dicey_server *const server, struct loop_request *const req) {
    assert(server && req);

    const bool success = dicey_queue_push(&server->queue, req, DICEY_LOCKING_POLICY_BLOCKING);

    assert(success);
    (void) success; // suppress unused variable warning with NDEBUG and MSVC

    return dicey_error_from_uv(uv_async_send(&server->async));
}

static enum dicey_error server_blocking_request(
    struct dicey_server *const server,
    struct loop_request *const req // must be malloc'd, and will be freed by this function
) {
    assert(server && req);

    uv_sem_t sem = { 0 };
    uv_sem_init(&sem, 0);

    req->sem = &sem;

    enum dicey_error err = server_submit_request(server, req);

    // there is no way async send can fail, honestly, and it if does, there is no possible way to recover
    assert(!err);
    (void) err; // suppress unused variable warning with NDEBUG and MSVC

    uv_sem_wait(&sem);

    uv_sem_destroy(&sem);

    err = req->err;
    free(req);

    return err;
}

static void server_shutdown_at_end(uv_handle_t *const handle) {
    struct dicey_server *const server = (struct dicey_server *) handle;

    uv_stop(&server->loop);

    if (server->shutdown_hook) {
        // clean the shutdown hook before posting
        uv_sem_t *const sem = server->shutdown_hook;

        server->shutdown_hook = NULL;

        uv_sem_post(sem);
    }
}

static void server_close_pipe(uv_handle_t *const handle) {
    struct dicey_server *const server = handle->data; // the async handle, which has the server as data

    uv_close((uv_handle_t *) &server->pipe, &server_shutdown_at_end);
}

static enum dicey_error server_finalize_shutdown(struct dicey_server *const server) {
    assert(server && server->state == SERVER_STATE_QUITTING);

    dicey_queue_deinit(&server->queue, &loop_request_delete, NULL);

    uv_close((uv_handle_t *) &server->async, &server_close_pipe);

    return DICEY_OK;
}

static uint32_t server_next_seq(struct dicey_server *const server) {
    assert(server);

    const uint32_t seq = server->seq_cnt;

    server->seq_cnt += 2U; // will roll over after UINT32_MAX

    return seq;
}

static enum dicey_error server_kick_client(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    const enum dicey_bye_reason reason
) {
    assert(server);

    struct outbound_packet packet = { .kind = DICEY_OP_RESPONSE };

    enum dicey_error err = dicey_packet_bye(&packet.single, server_next_seq(server), reason);
    if (err) {
        return err;
    }

    err = server_sendpkt(server, client, packet);

    if (err) {
        outbound_packet_cleanup(&packet);
    }

    return err;
}

static enum dicey_error server_remove_client(struct dicey_server *const server, const size_t index) {
    if (!server) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_client_data *const bucket = dicey_client_list_drop_client(server->clients, index);

    if (!bucket) {
        return TRACE(DICEY_EINVAL);
    }

    uv_close((uv_handle_t *) bucket, &on_client_end);

    return DICEY_OK;
}

static enum dicey_error server_report_error(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    const struct dicey_packet req,
    const enum dicey_error report_err
) {
    assert(server && client);

    struct outbound_packet packet = { .kind = DICEY_OP_RESPONSE };

    uint32_t seq = UINT32_MAX;
    enum dicey_error err = dicey_packet_get_seq(req, &seq);
    if (err) {
        return err;
    }

    struct dicey_message msg = { 0 };
    err = dicey_packet_as_message(req, &msg);
    if (err) {
        return err;
    }

    err = make_error(&packet.single, seq, msg.path, msg.selector, report_err);
    if (err) {
        return err;
    }

    err = server_sendpkt(server, client, packet);

    if (err) {
        outbound_packet_cleanup(&packet);
    }

    return err;
}

struct prune_ctx {
    struct dicey_server *server;
    struct dicey_client_data *client;
    const char *path_to_prune;
};

static enum dicey_error raise_event(
    struct dicey_server *const server,
    struct dicey_packet packet,
    const struct dicey_message *const msg
) {
    assert(server && msg);

    // start with 1, because if the first send fails, we risk prematurely freeing the packet
    struct dicey_shared_packet *shared_pkt = dicey_shared_packet_from(packet, 1);
    if (!shared_pkt) {
        dicey_packet_deinit(&packet);

        return TRACE(DICEY_ENOMEM);
    }

    const char *const elemdescr = dicey_element_descriptor_format_to(&server->scratchpad, msg->path, msg->selector);
    if (!elemdescr) {
        dicey_shared_packet_unref(shared_pkt);

        return TRACE(DICEY_ENOMEM);
    }

    enum dicey_error err = dicey_packet_set_seq(dicey_shared_packet_borrow(shared_pkt), server_next_seq(server));
    if (err) {
        dicey_shared_packet_unref(shared_pkt);

        return err;
    }

    // iterate all clients and check if they should receive the event
    struct dicey_client_data *const *const end = dicey_client_list_end(server->clients);
    for (struct dicey_client_data *const *client = dicey_client_list_begin(server->clients); client < end; ++client) {
        if (!*client) {
            continue;
        }

        if (!dicey_client_data_is_subscribed(*client, elemdescr)) {
            continue;
        }

        // hold the packet. We know the refcount will be equal to the number of events sent (because we hold the thread)
        // + 1 (because this function is holding it too for now)
        dicey_shared_packet_ref(shared_pkt);

        struct outbound_packet event = { .kind = DICEY_OP_EVENT, .shared = shared_pkt };

        err = server_sendpkt(server, *client, event);
        if (err) {
            // deref, we failed this send
            dicey_shared_packet_unref(shared_pkt);
        }
    }

    // deref, we are done with the packet and its refcount has increased by the number of interested clients
    dicey_shared_packet_unref(shared_pkt);

    return DICEY_OK;
}

static bool request_targets_path(const struct dicey_pending_request *const req, void *const ctx) {
    assert(req && ctx);

    const struct prune_ctx *const pctx = ctx;

    if (!strcmp(req->path, pctx->path_to_prune)) {
        struct outbound_packet packet = { .kind = DICEY_OP_RESPONSE };
        enum dicey_error err = make_error(&packet.single, req->packet_seq, req->path, req->sel, DICEY_EPATH_DELETED);
        if (!err) {
            // if err is true, the client will timeout, but we are clearly OOM, so we can't do anything about it

            // best effort send - we can't really do anything else
            err = server_sendpkt(pctx->server, pctx->client, packet);

            if (err) {
                outbound_packet_cleanup(&packet);
            }
        }

        return true;
    }

    return false;
}

static enum dicey_error remove_object(struct dicey_server *server, const char *const path) {
    // before removing an object from the registry, we must prune all pending requests to it from all clients
    struct dicey_client_data *const *const end = dicey_client_list_end(server->clients);

    for (struct dicey_client_data *const *cur = dicey_client_list_begin(server->clients); cur < end; ++cur) {
        struct dicey_client_data *client = *cur;

        if (!client) {
            continue;
        }

        struct prune_ctx ctx = {
            .server = server,
            .client = client,
            .path_to_prune = path,
        };

        dicey_pending_requests_prune(client->pending, &request_targets_path, &ctx);
    }

    return dicey_registry_delete_object(&server->registry, path);
}

static enum dicey_error server_shutdown(struct dicey_server *const server) {
    assert(server && server->state == SERVER_STATE_RUNNING);

    server->state = SERVER_STATE_QUITTING;

    struct dicey_client_data *const *const end = dicey_client_list_end(server->clients);

    enum dicey_error err = DICEY_OK;

    bool empty = true;

    for (struct dicey_client_data *const *client = dicey_client_list_begin(server->clients); client < end; ++client) {
        if (*client) {
            empty = false;

            const enum dicey_error kick_err = server_kick_client(server, *client, DICEY_BYE_REASON_SHUTDOWN);

            err = kick_err ? kick_err : err;
        }
    }

    // avoid deadlocks: if there are no clients, we can finalize the shutdown immediately without wating for all byes to
    // be sent
    return empty ? server_finalize_shutdown(server) : err;
}

static ptrdiff_t client_got_bye(struct dicey_client_data *client, const struct dicey_bye bye) {
    (void) bye; // unused

    assert(client);

    server_remove_client(client->parent, client->info.id);

    return CLIENT_STATE_DEAD;
}

static ptrdiff_t client_got_hello(
    struct dicey_client_data *client,
    const uint32_t seq,
    const struct dicey_hello hello
) {
    assert(client);

    struct dicey_server *const server = client->parent;
    assert(server);

    if (client->state != CLIENT_STATE_CONNECTED) {
        return TRACE(DICEY_EINVAL);
    }

    if (seq) {
        server->on_error(
            server, DICEY_EINVAL, &client->info, "unexpected seq number %" PRIu32 "in hello packet, must be 0", seq
        );

        return TRACE(DICEY_EINVAL);
    }

    if (dicey_version_cmp(hello.version, DICEY_PROTO_VERSION_CURRENT) < 0) {
        return TRACE(DICEY_ECLIENT_TOO_OLD);
    }

    struct outbound_packet hello_repl = { .kind = DICEY_OP_RESPONSE };

    // reply with the same seq
    enum dicey_error err = dicey_packet_hello(&hello_repl.single, seq, DICEY_PROTO_VERSION_CURRENT);
    if (err) {
        return err;
    }

    err = server_sendpkt(server, client, hello_repl);

    if (err) {
        outbound_packet_cleanup(&hello_repl);
        return err;
    }

    return CLIENT_STATE_RUNNING;
}

static ptrdiff_t client_got_message(struct dicey_client_data *const client, struct dicey_packet packet) {
    assert(client);

    struct dicey_server *const server = client->parent;
    assert(server);

    uint32_t seq = UINT32_MAX;
    if (dicey_packet_get_seq(packet, &seq) != DICEY_OK) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_message message = { 0 };
    if (dicey_packet_as_message(packet, &message) != DICEY_OK || is_server_op(message.type)) {
        return TRACE(DICEY_EINVAL);
    }

    if (client->state != CLIENT_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_object_entry obj_entry = { 0 };

    if (!dicey_registry_get_object_entry(&server->registry, message.path, &obj_entry)) {
        // not a fatal error: skip the seq and send an error response
        const enum dicey_error skip_err = dicey_pending_request_skip(&client->pending, seq);
        if (skip_err) {
            dicey_packet_deinit(&packet);

            return skip_err;
        }

        const enum dicey_error repl_err = server_report_error(server, client, packet, DICEY_EPATH_NOT_FOUND);

        // get rid of packet
        dicey_packet_deinit(&packet);

        // shortcircuit: the object was not found, so we've already sent an error response
        return repl_err ? repl_err : CLIENT_STATE_RUNNING;
    }

    struct dicey_element_entry elem_entry = { 0 };

    if (!dicey_registry_get_element_entry_from_sel(&server->registry, message.path, message.selector, &elem_entry)) {
        // not a fatal error: skip the seq and send an error response
        const enum dicey_error skip_err = dicey_pending_request_skip(&client->pending, seq);
        if (skip_err) {
            dicey_packet_deinit(&packet);

            return skip_err;
        }

        const enum dicey_error repl_err = server_report_error(server, client, packet, DICEY_EELEMENT_NOT_FOUND);

        // get rid of packet
        dicey_packet_deinit(&packet);

        // shortcircuit: the element was not found, so we've already sent an error response
        return repl_err ? repl_err : CLIENT_STATE_RUNNING;
    }

    const enum dicey_error op_err = is_message_acceptable_for(*elem_entry.element, &message);
    if (op_err) {
        // not a fatal error: skip the seq and send an error response
        const enum dicey_error skip_err = dicey_pending_request_skip(&client->pending, seq);
        if (skip_err) {
            dicey_packet_deinit(&packet);

            return skip_err;
        }

        const enum dicey_error repl_err = server_report_error(server, client, packet, op_err);

        // get rid of packet
        dicey_packet_deinit(&packet);

        // shortcircuit: the message has been rejected and we've already sent an error response
        return repl_err ? repl_err : CLIENT_STATE_RUNNING;
    }

    struct dicey_registry_builtin_info binfo = { 0 };
    if (dicey_registry_get_builtin_info_for(&elem_entry, &binfo)) {
        assert(binfo.handler);

        // we hit on a builtin
        // validate and skip the seq - otherwise the client state will misalign with the server
        const enum dicey_error skip_err = dicey_pending_request_skip(&client->pending, seq);
        if (skip_err) {
            dicey_packet_deinit(&packet);

            return skip_err;
        }

        struct outbound_packet response = { .kind = DICEY_OP_RESPONSE };

        struct dicey_builtin_context context = {
            .registry = &server->registry,
            .scratchpad = &server->scratchpad,
        };

        const enum dicey_error builtin_err =
            binfo.handler(&context, binfo.opcode, client, message.path, &elem_entry, &message.value, &response.single);

        if (builtin_err) {
            const enum dicey_error repl_err = server_report_error(server, client, packet, builtin_err);

            dicey_packet_deinit(&packet);

            // shortcircuit: the introspection failed and we've already sent an error response
            return repl_err ? repl_err : CLIENT_STATE_RUNNING;
        }

        // get rid of the request, we don't need it anymore
        dicey_packet_deinit(&packet);

        // set the seq number of the response to match the seq number of the request
        const enum dicey_error set_err = dicey_packet_set_seq(response.single, seq);
        if (set_err) {
            outbound_packet_cleanup(&response);

            return set_err;
        }

        const enum dicey_error send_err = server_sendpkt(server, client, response);

        if (send_err) {
            outbound_packet_cleanup(&response);
        }

        return send_err ? send_err : CLIENT_STATE_RUNNING;
    }

    if (server->on_request) {
        const enum dicey_error accept_err = dicey_pending_requests_add(
            &client->pending,
            &(struct dicey_pending_request) {
                .packet_seq = seq,
                .op = message.type,
                .path = obj_entry.path, // lifetime tied to the object
                .sel = elem_entry.sel,  // lifetime tied to the trait
                .signature = elem_entry.element->signature,
            }
        );

        if (accept_err) {
            dicey_packet_deinit(&packet);

            // the client has violated the protocol somehow and will be promply kicked out
            return accept_err;
        }

        // move ownership of the packet to the callback
        server->on_request(server, &client->info, seq, packet);
    } else {
        dicey_packet_deinit(&packet);
    }

    return CLIENT_STATE_RUNNING;
}

static enum dicey_error client_raised_error(struct dicey_client_data *client, const enum dicey_error err) {
    assert(client);

    struct dicey_server *const server = client->parent;
    assert(server);

    client->state = CLIENT_STATE_DEAD;

    server->on_error(server, err, &client->info, "client error: %s", dicey_error_name(err));

    return server_kick_client(server, client, DICEY_BYE_REASON_ERROR);
}

static enum dicey_error client_got_packet(struct dicey_client_data *client, struct dicey_packet packet) {
    assert(client && dicey_packet_is_valid(packet));

    ptrdiff_t err = DICEY_OK;

    switch (dicey_packet_get_kind(packet)) {
    case DICEY_PACKET_KIND_HELLO:
        {
            uint32_t seq = 0U;
            err = dicey_packet_get_seq(packet, &seq);
            if (err) {
                break;
            }

            struct dicey_hello hello = { 0 };

            DICEY_ASSUME(dicey_packet_as_hello(packet, &hello));

            err = client_got_hello(client, seq, hello);

            dicey_packet_deinit(&packet);

            break;
        }

    case DICEY_PACKET_KIND_BYE:
        {
            struct dicey_bye bye = { 0 };

            DICEY_ASSUME(dicey_packet_as_bye(packet, &bye));

            err = client_got_bye(client, bye);

            dicey_packet_deinit(&packet);

            break;
        }

    case DICEY_PACKET_KIND_MESSAGE:
        err = client_got_message(client, packet);
        break;

    default:
        abort(); // unreachable, dicey_packet_is_valid guarantees a valid packet
    }

    if (err < 0) {
        return client_raised_error(client, err);
    } else {
        client->state = (enum dicey_client_state) err;
        return DICEY_OK;
    }
}

static void on_write(uv_write_t *const req, const int status) {
    assert(req);

    struct write_request *const write_req = (struct write_request *) req;
    struct dicey_server *const server = write_req->server;

    assert(server);

    const struct dicey_client_data *const client = dicey_client_list_get_client(server->clients, write_req->client_id);
    assert(client);

    const struct dicey_client_info *const info = &client->info;

    if (status < 0) {
        server->on_error(server, dicey_error_from_uv(status), info, "write error %s\n", uv_strerror(status));
    }

    // temporarily borrow the packet
    const struct dicey_packet packet = outbound_packet_borrow(write_req->packet);

    if (dicey_packet_get_kind(packet) == DICEY_PACKET_KIND_BYE) {
        const enum dicey_error err = server_remove_client(write_req->server, write_req->client_id);
        if (err) {
            server->on_error(server, err, info, "server_remove_client: %s\n", dicey_error_name(err));
        }
    }

    // either cleans up the packet or decrements the refcount
    outbound_packet_cleanup(&write_req->packet);
    free(write_req);

    if (server->state == SERVER_STATE_QUITTING && dicey_client_list_is_empty(server->clients)) {
        // all clients have been freed. We can now close the server
        const enum dicey_error err = server_finalize_shutdown(server);

        if (err) {
            server->on_error(server, err, NULL, "server_finalize_shutdown: %s\n", dicey_error_name(err));

            if (server->shutdown_hook) {
                uv_sem_post(server->shutdown_hook);
            }
        }
    }
}

static void alloc_buffer(uv_handle_t *const handle, const size_t suggested_size, uv_buf_t *const buf) {
    (void) suggested_size; // useless, always 65k (max UDP packet size)

    struct dicey_client_data *const client = (struct dicey_client_data *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    (void) buf;

    struct dicey_client_data *const client = (struct dicey_client_data *) stream;
    assert(client && client->parent && client->chunk);

    struct dicey_server *const server = client->parent;

    if (nread < 0) {
        if (nread != UV_EOF) {
            const int uverr = (int) nread;

            server->on_error(server, dicey_error_from_uv(uverr), &client->info, "Read error %s\n", uv_strerror(uverr));
        }

        const enum dicey_error remove_err = server_remove_client(client->parent, client->info.id);
        if (remove_err) {
            server->on_error(
                server, remove_err, &client->info, "server_remove_client: %s\n", dicey_error_name(remove_err)
            );
        }

        return;
    }

    if (server->state != SERVER_STATE_RUNNING) {
        // ignore inbound packets while shutting down
        return;
    }

    struct dicey_chunk *const chunk = client->chunk;

    // mark the first nread bytes of the chunk as taken
    chunk->len += (size_t) nread;

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

static void loop_request_inbound(uv_async_t *const async) {
    assert(async && async->data);

    struct dicey_server *const server = async->data;

    assert(server);

    void *item = NULL;
    while (dicey_queue_pop(&server->queue, &item, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(item);

        struct loop_request *const req = item;

        req->err = DICEY_OK;

        struct dicey_client_data *const client =
            req->kind != LOOP_REQ_STOP_SERVER ? dicey_client_list_get_client(server->clients, req->target) : NULL;

        switch (req->kind) {
        case LOOP_REQ_ADD_OBJECT:
            assert(req->object_info.name && req->object_info.traits);

            req->err = dicey_registry_add_object_with_trait_set(
                &server->registry, req->object_info.name, req->object_info.traits
            );

            // free the name we strdup'd earlier
            free((char *) req->object_info.name);

            break;

        case LOOP_REQ_ADD_TRAIT:
            assert(req->trait);

            req->err = dicey_registry_add_trait(&server->registry, req->trait);

            break;

        case LOOP_REQ_DEL_OBJECT:
            assert(req->object_info.name);

            req->err = remove_object(server, req->object_info.name);

            // free the name we strdup'd earlier
            free((char *) req->object_info.name);
            break;

        case LOOP_REQ_RAISE_EVENT:
            {
                assert(dicey_packet_is_valid(req->packet));

                struct dicey_message msg = { 0 };
                req->err = dicey_packet_as_message(req->packet, &msg);
                if (req->err) {
                    break;
                }

                req->err = raise_event(server, req->packet, &msg);

                break;
            }

        case LOOP_REQ_SEND_RESPONSE:
            assert(dicey_packet_is_valid(req->packet));

            if (client) {
                struct dicey_message msg = { 0 };

                req->err = dicey_packet_as_message(req->packet, &msg);
                if (req->err) {
                    break;
                }

                // TODO: validate that we are sending a valid response
                req->err = client_send_response(server, client, req->packet, &msg);
            } else {
                req->err = DICEY_EPEER_NOT_FOUND;
            }

            break;

        case LOOP_REQ_KICK_CLIENT:
            req->err = server_kick_client(server, client, req->kick_reason);

            break;

        case LOOP_REQ_STOP_SERVER:
            server->shutdown_hook = req->sem;
            req->err = server_shutdown(server);

            if (!req->err) {
                // do not unlock anything here - it will be done later, when an actual shutdown happens
                if (!req->sem) {
                    // there's nobody wating for this, so we must delete it
                    free(req);
                }

                return;
            }

            // if the request did not go through, go forward and unlock the caller
            break;
        }

        if (req->sem) {
            // signal the waiting thread that the request has been processed
            // this is only necessary with STOP_SERVER, unless an early error happens - we want the request to return
            // only when the server _actually_ stops. That's why the case above has a return statement

            uv_sem_post(req->sem);

            // don't free the request here, it's the caller's responsibility if they are waiting
        } else {
            if (req->err) {
                // if the request is not blocking, and an error happened, we must report it otherwise it will be lost
                server->on_error(
                    server, req->err, &client->info, "loop_request_inbound: %s", dicey_error_name(req->err)
                );
            }

            // the request is not blocking, so we must free it
            // the packet, if any, will be freed in on_write
            free(req);
        }
    }
}

static void on_connect(uv_stream_t *const stream, const int status) {
    assert(stream);

    struct dicey_server *const server = (struct dicey_server *) stream;

    if (status < 0) {
        server->on_error(server, dicey_error_from_uv(status), NULL, "New connection error %s", uv_strerror(status));

        return;
    }

    struct dicey_client_data *client = NULL;

    const ptrdiff_t id = server_add_client(server, &client);
    if (id < 0) {
        server->on_error(server, id, NULL, "server_add_client: %s", dicey_error_name(id));

        return;
    }

    assert(client);

    const int accept_err = uv_accept(stream, (uv_stream_t *) client);
    if (accept_err) {
        server->on_error(server, dicey_error_from_uv(accept_err), NULL, "uv_accept: %s", uv_strerror(accept_err));

        server_remove_client(server, (size_t) id);
    }

    if (server->on_connect && !server->on_connect(server, (size_t) id, &client->info.user_data)) {
        server->on_error(server, DICEY_ECONNREFUSED, &client->info, "connection refused by user code");

        server_remove_client(server, (size_t) id);

        return;
    }

    const int err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    if (err < 0) {
        server->on_error(server, dicey_error_from_uv(err), &client->info, "read_start fail: %s", uv_strerror(err));

        server_remove_client(server, (size_t) id);
    }
}

static void dummy_error_handler(
    struct dicey_server *const state,
    const enum dicey_error err,
    const struct dicey_client_info *const cln,
    const char *const msg,
    ...
) {
    (void) state;
    (void) err;
    (void) cln;
    (void) msg;
}

static void close_all_handles(uv_handle_t *const handle, void *const ctx) {
    (void) ctx;

    // issue a close and pray
    uv_close(handle, NULL);
}

void dicey_server_delete(struct dicey_server *const server) {
    if (!server) {
        return;
    }

    if (server->state == SERVER_STATE_RUNNING) {
        (void) dicey_server_stop_and_wait(server);
    }

    const int uverr = uv_loop_close(&server->loop);
    if (uverr == UV_EBUSY) {
        // hail mary attempt at closing any handles left. This is 99% likely only triggered whenever the loop was never
        // run at all, so there are only empty handles to free up

        uv_walk(&server->loop, &close_all_handles, NULL);

        uv_run(&server->loop, UV_RUN_DEFAULT); // should return whenever all uv_close calls are done
    }

    dicey_registry_deinit(&server->registry);

    free(server->clients);
    free(server->scratchpad.data);
    free(server);
}

enum dicey_error dicey_server_new(struct dicey_server **const dest, const struct dicey_server_args *const args) {
    assert(dest);

    struct dicey_server *const server = malloc(sizeof *server);
    if (!server) {
        return TRACE(DICEY_ENOMEM);
    }

    *server = (struct dicey_server) {
        .on_error = &dummy_error_handler,

        .seq_cnt = 1U, // server-initiated seq numbers are always odd

        .clients = NULL,
    };

    enum dicey_error err = dicey_registry_init(&server->registry);
    if (err) {
        free(server);

        return err;
    }

    if (args) {
        server->on_connect = args->on_connect;
        server->on_disconnect = args->on_disconnect;
        server->on_request = args->on_request;

        if (args->on_error) {
            server->on_error = args->on_error;
        }
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

    uverr = uv_async_init(&server->loop, &server->async, &loop_request_inbound);
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
    dicey_queue_deinit(&server->queue, &loop_request_delete, NULL);

free_loop:
    uv_loop_close(&server->loop);

free_clients:
    free(server->clients);

    dicey_registry_deinit(&server->registry);
    free(server);

    return err;
}

enum dicey_error dicey_server_add_object(
    struct dicey_server *const server,
    const char *const path,
    struct dicey_hashset *const trait_names
) {
    assert(server && path && trait_names);

    // TODO: analyse the thread safety of this approach
    switch ((enum server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_add_object_with_trait_set(registry, path, trait_names);
        }

    case SERVER_STATE_RUNNING:
        {
            struct loop_request *const req = malloc(sizeof *req);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            char *const path_copy = strdup(path); // the path is NOT owned by the request
            if (!path_copy) {
                free(req);

                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct loop_request) {
                .kind = LOOP_REQ_ADD_OBJECT,
                .object_info = {
                    .name = path_copy,
                    .traits = trait_names,
                },
            };

            return server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

enum dicey_error dicey_server_add_object_with(struct dicey_server *const server, const char *const path, ...) {
    assert(server && path);
    va_list args;

    struct dicey_hashset *traits = dicey_hashset_new();
    if (!traits) {
        return TRACE(DICEY_ENOMEM);
    }

    va_start(args, path);

    enum dicey_error err = DICEY_OK;

    for (;;) {
        const char *const trait = va_arg(args, const char *);
        if (!trait) {
            break;
        }

        switch (dicey_hashset_add(&traits, trait)) {
        case DICEY_HASH_SET_FAILED:
            err = TRACE(DICEY_ENOMEM);

            goto out;

        case DICEY_HASH_SET_UPDATED:
            err = TRACE(DICEY_EEXIST);

            goto out;

        default:
            break;
        }
    }

out:
    va_end(args);

    if (err) {
        dicey_hashset_delete(traits);

        return err;
    }

    err = dicey_server_add_object(server, path, traits);
    if (err) {
        dicey_hashset_delete(traits);
    }

    return err;
}

enum dicey_error dicey_server_add_trait(struct dicey_server *const server, struct dicey_trait *const trait) {
    assert(server && trait);

    switch ((enum server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_add_trait(registry, trait);
        }

    case SERVER_STATE_RUNNING:
        {
            struct loop_request *const req = malloc(sizeof *req);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct loop_request) {
                .kind = LOOP_REQ_ADD_TRAIT,
                .trait = trait,
            };

            return server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

enum dicey_error dicey_server_delete_object(struct dicey_server *const server, const char *const path) {
    assert(server && path);

    // TODO: analyse the thread safety of this approach
    switch ((enum server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_delete_object(registry, path);
        }

    case SERVER_STATE_RUNNING:
        {
            struct loop_request *const req = malloc(sizeof *req);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            char *const path_copy = strdup(path); // the path is NOT owned by the request
            if (!path_copy) {
                free(req);

                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct loop_request) {
                .kind = LOOP_REQ_DEL_OBJECT,
                .object_info = {
                    .name = path_copy,
                },
            };

            return server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

void *dicey_server_get_context(struct dicey_server *const server) {
    return server ? server->ctx : NULL;
}

struct dicey_registry *dicey_server_get_registry(struct dicey_server *const server) {
    assert(server && server->state <= SERVER_STATE_INIT);

    // only allow this function to be called before the server is started
    return server->state <= SERVER_STATE_INIT ? &server->registry : NULL;
}

enum dicey_error dicey_server_kick(struct dicey_server *const server, const size_t id) {
    assert(server);

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_KICK_CLIENT,
        .target = id,
    };

    return server_blocking_request(server, req);
}

enum dicey_error dicey_server_raise(struct dicey_server *const server, const struct dicey_packet packet) {
    assert(server && dicey_packet_is_valid(packet));

    if (!can_send_as_event(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_RAISE_EVENT,
        .target = -1,
        .packet = packet,
    };

    return server_submit_request(server, req);
}

enum dicey_error dicey_server_raise_and_wait(struct dicey_server *const server, const struct dicey_packet packet) {
    assert(server && dicey_packet_is_valid(packet));

    if (!can_send_as_event(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_RAISE_EVENT,
        .target = -1,
        .packet = packet,
    };

    return server_blocking_request(server, req);
}

enum dicey_error dicey_server_send_response(
    struct dicey_server *const server,
    const size_t id,
    const struct dicey_packet packet
) {
    assert(server);

    if (!can_send_as_response(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    if (id > (size_t) PTRDIFF_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_SEND_RESPONSE,
        .target = id,
        .packet = packet,
    };

    return server_submit_request(server, req);
}

enum dicey_error dicey_server_send_response_and_wait(
    struct dicey_server *const server,
    const size_t id,
    const struct dicey_packet packet
) {
    assert(server);

    if (!can_send_as_response(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    if (id > (size_t) PTRDIFF_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_SEND_RESPONSE,
        .target = id,
        .packet = packet,
    };

    return server_blocking_request(server, req);
}

void *dicey_server_set_context(struct dicey_server *const server, void *const new_context) {
    assert(server);

    void *const old_context = server->ctx;
    server->ctx = new_context;

    return old_context;
}

enum dicey_error dicey_server_start(struct dicey_server *const server, struct dicey_addr addr) {
    assert(server && addr.addr && addr.len);

    int err = uv_pipe_bind2(&server->pipe, addr.addr, addr.len, 0U);

    dicey_addr_deinit(&addr);

    if (err < 0) {
        goto quit;
    }

    err = uv_listen((uv_stream_t *) &server->pipe, 128, &on_connect);

    if (err < 0) {
        goto quit;
    }

    server->state = SERVER_STATE_RUNNING;

    err = uv_run(&server->loop, UV_RUN_DEFAULT);
    if (err < 0) {
        goto quit;
    }

    server->state = SERVER_STATE_INIT;

quit:
    return dicey_error_from_uv(err);
}

enum dicey_error dicey_server_stop(struct dicey_server *const server) {
    assert(server);

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_STOP_SERVER,
    };

    const enum dicey_error err = server_submit_request(server, req);
    if (err) {
        free(req);

        return err;
    }

    return DICEY_OK;
}

enum dicey_error dicey_server_stop_and_wait(struct dicey_server *const server) {
    assert(server);

    if (server->state != SERVER_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    struct loop_request *const req = malloc(sizeof *req);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct loop_request) {
        .kind = LOOP_REQ_STOP_SERVER,
    };

    return server_blocking_request(server, req);
}
