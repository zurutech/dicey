/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
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
#define _XOPEN_SOURCE 700

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
#include <dicey/core/hashset.h>
#include <dicey/core/packet.h>
#include <dicey/core/typedescr.h>
#include <dicey/core/value.h>
#include <dicey/ipc/address.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/request.h>
#include <dicey/ipc/server-api.h>
#include <dicey/ipc/server.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"
#include "sup/util.h"
#include "sup/uvtools.h"

#include "ipc/chunk.h"
#include "ipc/elemdescr.h"
#include "ipc/queue.h"

#include "builtins/builtins.h"

#include "client-data.h"
#include "pending-reqs.h"
#include "server-clients.h"
#include "server-internal.h"
#include "server-loopreq.h"
#include "shared-packet.h"

#include "dicey_config.h"

#define DICEY_SET_RESPONSE_SIG                                                                                         \
    (const char[]) { (char) DICEY_TYPE_UNIT, '\0' }

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

    case DICEY_OP_SIGNAL:
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

    case DICEY_OP_SIGNAL:
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

    case DICEY_OP_SIGNAL:
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

    case DICEY_OP_SIGNAL:
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

    case DICEY_OP_SIGNAL:
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

static void loop_request_delete(void *const ctx, void *const ptr) {
    DICEY_UNUSED(ctx);

    struct dicey_server_loop_request *const req = ptr;
    if (req) {
        if (req->sem) {
            req->err = DICEY_ECANCELLED;

            uv_sem_post(req->sem);
        } else {
            assert(req->cb);

            req->cb(NULL, NULL, req->payload);
            free(req);
        }
    }
}

static void on_write(uv_write_t *req, int status);

static bool is_event_msg(const struct dicey_packet pkt) {
    struct dicey_message msg = { 0 };
    if (dicey_packet_as_message(pkt, &msg) != DICEY_OK) {
        return false;
    }

    return msg.type == DICEY_OP_SIGNAL;
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

        if (elem.flags & DICEY_ELEMENT_READONLY) {
            return DICEY_EPROPERTY_READ_ONLY;
        }

        break;

    case DICEY_OP_EXEC:
        if (elem.type != DICEY_ELEMENT_TYPE_OPERATION) {
            return DICEY_EINVAL;
        }

        break;

    case DICEY_OP_SIGNAL:
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
    case DICEY_OP_SIGNAL:
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

static enum dicey_error registry_add_aliases(
    struct dicey_registry *const registry,
    const char *const path,
    const struct dicey_hashset *const aliases
) {
    assert(registry && path);

    if (!aliases || !dicey_hashset_size(aliases)) {
        return DICEY_OK; // no aliases to add
    }

    struct dicey_hashset_iter iter = dicey_hashset_iter_start(aliases);
    const char *alias = NULL;
    enum dicey_error err = DICEY_OK;

    while (dicey_hashset_iter_next(&iter, &alias)) {
        err = dicey_registry_alias_object(registry, path, alias);

        // if the alias already exists, we can ignore the error
        if (err && err != DICEY_EEXIST) {
            break;
        }
    }

    if (err && err != DICEY_EEXIST) {
        struct dicey_hashset_iter iter = dicey_hashset_iter_start(aliases);
        const char *alias = NULL;

        while (dicey_hashset_iter_next(&iter, &alias)) {
            // remove the alias from the registry, best effort
            (void) dicey_registry_unalias_object(registry, path, alias);
        }
    }

    return DICEY_OK;
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

    // TODO: simplify this code, which is a bit redundant with `dicey_request`

    enum dicey_error err = DICEY_OK;

    // match the packet back with the request
    struct dicey_request req = { 0 };

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
    dicey_request_deinit(&req); // always cleanup, this is noop if the request is empty

    if (err) {
        dicey_packet_deinit(&packet);
    }

    return err;
}

#define READ_MINBUF 256U // 256B

static ptrdiff_t server_new_peer(struct dicey_server *const server, struct dicey_client_data **const dest) {
    struct dicey_client_data **client_bucket = NULL;
    size_t id = 0U;

    enum dicey_error err = dicey_server_reserve_id(server, &client_bucket, &id);
    if (err) {
        return err;
    }

    struct dicey_client_data *const client = *client_bucket = dicey_client_data_new(server, id);
    if (!client) {
        // release the id
        DICEY_UNUSED(dicey_server_release_id(server, id));

        return TRACE(DICEY_ENOMEM);
    }

    if (uv_pipe_init(&server->loop, &client->pipe, 0)) {
        // release the id and free the client data struct
        err = dicey_server_cleanup_id(server, id);
        if (err) {
            return err;
        }

        return TRACE(DICEY_EUV_UNKNOWN);
    }

    *dest = client;

    return client->info.id;
}

static void server_shutdown_at_end(uv_handle_t *const handle) {
    assert(handle);

    struct dicey_server *const server = handle->data;
    assert(server);

    uv_stop(&server->loop);

    if (server->shutdown_hook) {
        // clean the shutdown hook before posting
        uv_sem_t *const sem = server->shutdown_hook;

        server->shutdown_hook = NULL;

        uv_sem_post(sem);
    }
}

static void server_close_prepare(uv_handle_t *const handle) {
    assert(handle);

    struct dicey_server *const server = handle->data; // the async handle, which has the server as data
    assert(server);

    uv_close((uv_handle_t *) &server->startup_prepare, &server_shutdown_at_end);
}

static void server_close_pipe(uv_handle_t *const handle) {
    struct dicey_server *const server = handle->data; // the async handle, which has the server as data

    uv_close((uv_handle_t *) &server->pipe, &server_close_prepare);
}

static enum dicey_error server_finalize_shutdown(struct dicey_server *const server) {
    assert(server && server->state == SERVER_STATE_QUITTING);

    dicey_queue_deinit(&server->queue, &loop_request_delete, NULL);

    uv_close((uv_handle_t *) &server->async, &server_close_pipe);

    return DICEY_OK;
}

#define server_report_startup(SERVERPTR, ERRC)                                                                         \
    do {                                                                                                               \
        struct dicey_server *const _srvptr = (SERVERPTR);                                                              \
        assert(_srvptr);                                                                                               \
                                                                                                                       \
        if (_srvptr->on_startup) {                                                                                     \
            _srvptr->on_startup(_srvptr, (ERRC));                                                                      \
        }                                                                                                              \
    } while (0)

static void server_init_notify_startup(uv_prepare_t *const startup_prepare) {
    assert(startup_prepare);

    struct dicey_server *const server = startup_prepare->data;
    assert(server);

    server_report_startup(server, DICEY_OK); // successful startup

    // it always returns 0 anyway
    (void) uv_prepare_stop(startup_prepare);
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

static bool request_should_prune_if_matching(const struct dicey_request *const req, void *const ctx) {
    const struct prune_ctx *const pctx = ctx;

    assert(req && pctx && pctx->server);

    const char *const main_path = dicey_registry_get_main_path(&pctx->server->registry, pctx->path_to_prune);
    assert(main_path); // the object must exist, we should have catched it earlier

    // check if the request is for the object we are removing
    if (!strcmp(main_path, req->real_path)) {
        const struct dicey_message *const msg = dicey_request_get_message(req);
        assert(msg);

        struct outbound_packet packet = { .kind = DICEY_OP_RESPONSE };
        enum dicey_error err =
            make_error(&packet.single, req->packet_seq, msg->path, msg->selector, DICEY_EPATH_DELETED);
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

        dicey_pending_requests_prune(client->pending, &request_should_prune_if_matching, &ctx);
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

    // avoid deadlocks: if there are no clients, we can finalize the shutdown immediately without waiting for all byes
    // to be sent
    return empty ? server_finalize_shutdown(server) : err;
}

static ptrdiff_t client_got_bye(struct dicey_client_data *client, const struct dicey_bye bye) {
    DICEY_UNUSED(bye); // unused

    assert(client);

    const enum dicey_client_data_state current_state = dicey_client_data_get_state(client);

    // if a client is in a quitting state, we assume it still has stuff to do (i.e., it's a plugin for instance)
    // we keep it around and hope the server will collect this when the time is right.
    // in this case BYE will be assumed not as the last ever communication received from the client, but as a step in
    // a longer controlled process that will eventually lead to the client being removed from the server
    if (current_state == CLIENT_DATA_STATE_QUITTING) {
        return CLIENT_DATA_STATE_QUITTING;
    }

    dicey_server_remove_client(client->parent, client->info.id);
    return CLIENT_DATA_STATE_DEAD;
}

static ptrdiff_t client_got_hello(
    struct dicey_client_data *client,
    const uint32_t seq,
    const struct dicey_hello hello
) {
    assert(client);

    struct dicey_server *const server = client->parent;
    assert(server);

    if (dicey_client_data_get_state(client) != CLIENT_DATA_STATE_CONNECTED) {
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

    return CLIENT_DATA_STATE_RUNNING;
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

    if (dicey_client_data_get_state(client) != CLIENT_DATA_STATE_RUNNING) {
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
        return repl_err ? repl_err : CLIENT_DATA_STATE_RUNNING;
    }

    struct dicey_object_element_entry object_entry = { 0 };

    if (!dicey_registry_get_element_entry_from_sel(&server->registry, message.path, message.selector, &object_entry)) {
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
        return repl_err ? repl_err : CLIENT_DATA_STATE_RUNNING;
    }

    const enum dicey_error op_err = is_message_acceptable_for(*object_entry.element, &message);
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
        return repl_err ? repl_err : CLIENT_DATA_STATE_RUNNING;
    }

    const struct dicey_element_entry elem_entry = dicey_object_element_entry_to_element_entry(&object_entry);

    struct dicey_registry_builtin_info binfo = { 0 };
    if (dicey_registry_get_builtin_info_for(elem_entry, &binfo)) {
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

        struct dicey_builtin_request request = {
            .opcode = binfo.opcode,
            .client = client,
            .path = message.path,
            .entry = &elem_entry,
            .source = &packet,
            .value = &message.value,
        };

        const ptrdiff_t builtin_res = binfo.handler(&context, &request, &response.single);
        if (builtin_res < 0) {
            const enum dicey_error repl_err =
                server_report_error(server, client, packet, (enum dicey_error) builtin_res);

            dicey_packet_deinit(&packet);

            // shortcircuit: the introspection failed and we've already sent an error response
            return repl_err ? repl_err : CLIENT_DATA_STATE_RUNNING;
        }

        enum dicey_client_data_state new_state = (enum dicey_client_data_state) builtin_res;

        // if the builtin code didn't do this already, get rid of the request - we don't need it anymore
        if (dicey_packet_is_valid(packet)) {
            dicey_packet_deinit(&packet);
        }

        struct dicey_packet rpkt = outbound_packet_borrow(response);

        if (!dicey_packet_is_valid(rpkt)) {
            return new_state; // no response needed for this builtin
        }
        // set the seq number of the response to match the seq number of the request
        const enum dicey_error set_err = dicey_packet_set_seq(rpkt, seq);
        if (set_err) {
            outbound_packet_cleanup(&response);

            return set_err;
        }

        const enum dicey_error send_err = server_sendpkt(server, client, response);

        if (send_err) {
            outbound_packet_cleanup(&response);
        }

        return send_err ? (ptrdiff_t) send_err : (ptrdiff_t) new_state;
    }

    if (server->on_request) {
        struct dicey_request request = { 0 };

        const enum dicey_error err = dicey_server_request_for(server, &client->info, packet, &request);
        if (err) {
            dicey_packet_deinit(&packet);

            return err;
        }

        const struct dicey_pending_request_result accept_res = dicey_pending_requests_add(&client->pending, &request);
        if (accept_res.error) {
            dicey_request_deinit(&request);

            // the client has violated the protocol somehow and will be promply kicked out
            return accept_res.error;
        }

        // request has been copied to the pending requests struct. Use accept_res.value to access it from now on
        struct dicey_request *const pending_req = accept_res.value;
        assert(pending_req);

        server->on_request(server, pending_req);

        // The user code has control over the lifecycle of the request. This means that it has to consume it, either
        // by sending a response or by attempting to use it and trigger a failure.
        // This if handles the latter case; the user attempted to construct a response, but it failed for any reason, so
        // we're pruning the request and sending an error response to the client (best effort)
        if (pending_req->state == DICEY_REQUEST_STATE_ABORTED) {
            // reply to the server with a generic error. Can't do much if this also fails

            (void) server_report_error(server, client, packet, DICEY_EAGAIN);

            // get rid of the request. We don't need to get it retrieved, we already have it
            // Again, if this fails, we can't do much about it
            (void) dicey_pending_requests_complete(client->pending, seq, NULL);

            dicey_request_deinit(&request);
        }
    } else {
        dicey_packet_deinit(&packet);
    }

    return CLIENT_DATA_STATE_RUNNING;
}

static enum dicey_error client_got_packet(struct dicey_client_data *const client, struct dicey_packet packet) {
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
        return dicey_server_client_raised_error(client->parent, client, err);
    } else {
        dicey_client_data_set_state(client, (enum dicey_client_data_state) err);

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
        const enum dicey_error err = dicey_server_remove_client(write_req->server, write_req->client_id);
        if (err) {
            server->on_error(server, err, info, "dicey_server_remove_client: %s\n", dicey_error_name(err));
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
    DICEY_UNUSED(suggested_size); // useless, always 65k (max UDP packet size)

    struct dicey_client_data *const client = (struct dicey_client_data *) handle;
    assert(client);

    *buf = dicey_chunk_get_buf(&client->chunk, READ_MINBUF);

    assert(buf->base && buf->len && buf->len >= READ_MINBUF && client->chunk);
}

static void on_read(uv_stream_t *const stream, const ssize_t nread, const uv_buf_t *const buf) {
    DICEY_UNUSED(buf);

    struct dicey_client_data *const client = (struct dicey_client_data *) stream;
    assert(client && client->parent);

    struct dicey_server *const server = client->parent;

    if (nread < 0) {
        if (nread != UV_EOF) {
            const int uverr = (int) nread;

            server->on_error(server, dicey_error_from_uv(uverr), &client->info, "Read error %s\n", uv_strerror(uverr));
        }

        // if the client is known to be quitting in a non trivial way (i.e. it's a plugin), it is expected for its pipe
        // to shut down at some point. Don't kick it out yet, let the server collect it when the time is right
        if (client->state != CLIENT_DATA_STATE_QUITTING) {
            const enum dicey_error remove_err = dicey_server_remove_client(client->parent, client->info.id);
            if (remove_err) {
                server->on_error(
                    server, remove_err, &client->info, "dicey_server_remove_client: %s\n", dicey_error_name(remove_err)
                );
            }
        }

        return;
    }

    if (server->state != SERVER_STATE_RUNNING) {
        // ignore inbound packets while shutting down
        return;
    }

    struct dicey_chunk *const chunk = client->chunk;
    assert(chunk); // if we got to this point, we must have a chunk

    // mark the first nread bytes of the chunk as taken
    chunk->len += (size_t) nread;

    const void *base = chunk->bytes;
    size_t remainder = chunk->len;

    // attempt parsing a packet
    struct dicey_packet packet = { 0 };
    const enum dicey_error err = dicey_packet_load(&packet, &base, &remainder);
    switch (err) {
    case DICEY_OK:
        DICEY_UNUSED(client_got_packet(client, packet));

        dicey_chunk_clear(chunk);

        break;

    case DICEY_EAGAIN:
        break; // not enough data to parse a packet

    default:
        DICEY_UNUSED(dicey_server_client_raised_error(client->parent, client, err));

        break;
    }
}

struct object_info {
    const char *const name;
    struct dicey_hashset *traits;
};

static enum dicey_error loop_request_add_object(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);

    struct object_info nfo = { 0 };

    memcpy(&nfo, payload, sizeof nfo);
    assert(nfo.name && nfo.traits);

    enum dicey_error err = DICEY_OK;
    if (server) {
        err = dicey_registry_add_object_with_trait_set(&server->registry, nfo.name, nfo.traits);
    } else {
        // the request was aborted. Free the traits. We don't care about err; the caller will receive ECANCELLED anyway
        dicey_hashset_delete(nfo.traits);
    }

    // free the name we strdup'd earlier
    free((char *) nfo.name);

    return err;
}

struct aliases_info {
    const char *path;
    struct dicey_hashset *aliases;
};

static enum dicey_error loop_request_add_aliases(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);

    struct aliases_info nfo = { 0 };

    memcpy(&nfo, payload, sizeof nfo);
    assert(nfo.path && nfo.aliases);

    enum dicey_error err = DICEY_OK;

    if (server) {
        err = registry_add_aliases(&server->registry, nfo.path, nfo.aliases);
    }

    // free the strings we strdup'd earlier
    free((char *) nfo.path);
    dicey_hashset_delete(nfo.aliases);

    return err;
}

static enum dicey_error loop_request_add_trait(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);

    struct dicey_trait *trait = NULL;

    memcpy(&trait, payload, sizeof trait);
    assert(trait);

    if (!server) {
        // the request was aborted. Free the trait. We don't care about err; the caller will receive ECANCELLED anyway
        dicey_trait_delete(trait);

        return DICEY_ECANCELLED;
    }

    return dicey_registry_add_trait(&server->registry, trait);
}

static enum dicey_error loop_request_del_object(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);

    const char *path = payload;
    assert(path);

    enum dicey_error err = DICEY_OK;
    if (server) {
        err = remove_object(server, path);
    }

    return err;
}

static enum dicey_error loop_request_kick_client(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    enum dicey_bye_reason reason = DICEY_BYE_REASON_INVALID;

    memcpy(&reason, payload, sizeof reason);
    assert(reason != DICEY_BYE_REASON_INVALID);

    if (!server) {
        return DICEY_ECANCELLED;
    }

    return server_kick_client(server, client, reason);
}

static enum dicey_error loop_request_raise_signal(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);

    struct dicey_packet packet = { 0 };

    memcpy(&packet, payload, sizeof packet);
    assert(dicey_packet_is_valid(packet));

    if (!server) {
        dicey_packet_deinit(&packet);

        return DICEY_ECANCELLED;
    }

    return dicey_server_raise_internal(server, packet);
}

static enum dicey_error loop_request_send_response(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    struct dicey_packet packet = { 0 };

    memcpy(&packet, payload, sizeof packet);
    assert(dicey_packet_is_valid(packet));

    enum dicey_error err = DICEY_OK;
    if (!server) {
        err = DICEY_ECANCELLED;

        goto quit;
    }

    if (client) {
        struct dicey_message msg = { 0 };

        err = dicey_packet_as_message(packet, &msg);
        if (err) {
            goto quit;
        }

        // TODO: validate that we are sending a valid response
        err = client_send_response(server, client, packet, &msg);

        // the packet has been consumed by client_send_response, se we reset it to avoid double freeing
        packet = DICEY_EMPTY_PACKET;
    } else {
        err = DICEY_EPEER_NOT_FOUND;
    }

quit:
    dicey_packet_deinit(&packet);

    return err;
}

// ew! this function is not a real handler, but its address is used as a tag to identify the shutdown request
// shutting the loop down is a special case, because due to the loop shutting down we can't send a response
static enum dicey_error loop_request_shutdown_phony_handler(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const payload
) {
    DICEY_UNUSED(client);
    DICEY_UNUSED(payload);

    return server ? DICEY_EINVAL : DICEY_ECANCELLED;
}

#define LOOP_REQUEST_SHUTDOWN                                                                                          \
    (struct dicey_server_loop_request) {                                                                               \
        .cb = &loop_request_shutdown_phony_handler,                                                                    \
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,                                                                     \
    };

static void loop_request_inbound(uv_async_t *const async) {
    assert(async && async->data);

    struct dicey_server *const server = async->data;

    assert(server);

    void *item = NULL;
    struct dicey_client_data *client = NULL;
    while (dicey_queue_pop(&server->queue, &item, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(item);

        struct dicey_server_loop_request *const req = item;

        // special case: handle server shutdown. This is a special case because there may be a semaphore waiting,
        // but it shouldn't be signaled until the server has actually stopped
        if (req->cb == &loop_request_shutdown_phony_handler) {
            req->err = server_shutdown(server);

            if (!req->err) { // the request went through. We should quit early after cleaning up
                server->shutdown_hook = req->sem;

                // do not unlock anything here - it will be done later, when an actual shutdown happens
                if (!req->sem) {
                    // there's nobody waiting for this, so we must delete it
                    free(req);
                }

                return;
            }

            // if the request did not go through, go forward, unlock the caller if locked and report the error as usual
        } else {
            // retrieve client data, if any, and call the callback
            if (req->target >= 0) {
                client = dicey_client_list_get_client(server->clients, req->target);
            }

            assert(req->cb);
            req->err = req->cb(server, client, req->payload);
        }

        if (req->sem) {
            // signal the waiting thread that the request has been processed
            // this is only necessary with STOP_SERVER, unless an early error happens - we want the request to return
            // only when the server _actually_ stops. That's why the case above has a return statement

            uv_sem_post(req->sem);

            // don't free the request here, it's the caller's responsibility if they are waiting
        } else {
            if (req->err && client) {
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

    const ptrdiff_t result = server_new_peer(server, &client);
    if (result < 0) {
        server->on_error(server, result, NULL, "server_add_client: %s", dicey_error_name(result));

        return;
    }

    assert(client);

    const size_t id = (size_t) result;

    const int accept_err = uv_accept(stream, (uv_stream_t *) client);
    if (accept_err) {
        server->on_error(server, dicey_error_from_uv(accept_err), NULL, "uv_accept: %s", uv_strerror(accept_err));

        dicey_server_remove_client(server, id);
    }

    if (server->on_connect && !server->on_connect(server, id, &client->info.user_data)) {
        server->on_error(server, DICEY_ECONNREFUSED, &client->info, "connection refused by user code");

        dicey_server_remove_client(server, (size_t) id);

        return;
    }

    const enum dicey_error err = dicey_server_start_reading_from_client_internal(server, id);
    if (err) {
        server->on_error(server, dicey_error_from_uv(err), &client->info, "read_start fail: %s", dicey_error_msg(err));

        dicey_server_remove_client(server, id);
    }
}

static void dummy_error_handler(
    struct dicey_server *const state,
    const enum dicey_error err,
    const struct dicey_client_info *const cln,
    const char *const msg,
    ...
) {
    DICEY_UNUSED(state);
    DICEY_UNUSED(err);
    DICEY_UNUSED(cln);
    DICEY_UNUSED(msg);
}

static void close_all_handles(uv_handle_t *const handle, void *const ctx) {
    DICEY_UNUSED(ctx);

    // issue a close and pray
    uv_close(handle, NULL);
}

void dicey_server_delete(struct dicey_server *const server) {
    if (!server) {
        return;
    }

    if (server->state == SERVER_STATE_RUNNING) {
        DICEY_UNUSED(dicey_server_stop_and_wait(server));
    }

    int uverr = uv_loop_close(&server->loop);
    if (uverr == UV_EBUSY) {
        // hail mary attempt at closing any handles left. This is 99% likely only triggered whenever the loop was never
        // run at all, so there are only empty handles to free up

        uv_walk(&server->loop, &close_all_handles, NULL);

        uv_run(&server->loop, UV_RUN_DEFAULT); // should return whenever all uv_close calls are done

        uverr = uv_loop_close(&server->loop); // just quit, we don't care what happens
        assert(!uverr);
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
        server->on_startup = args->on_startup;

#if DICEY_HAS_PLUGINS
        server->on_plugin_event = args->on_plugin_event;

        server->plugin_startup_timeout = args->plugin_startup_timeout;
#endif

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

    server->pipe.data = server;

    uv_prepare_t *const prepare = &server->startup_prepare;

    uverr = uv_prepare_init(&server->loop, prepare);
    if (uverr) {
        goto free_pipe;
    }

    server->startup_prepare.data = server;

    *dest = server;

    return DICEY_OK;

free_pipe:
    uv_close((uv_handle_t *) &server->pipe, NULL);

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
    switch ((enum dicey_server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_add_object_with_trait_set(registry, path, trait_names);
        }

    case SERVER_STATE_RUNNING:
        {
            struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct object_info);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            char *const path_copy = strdup(path); // the path is NOT owned by the request
            if (!path_copy) {
                free(req);

                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct dicey_server_loop_request) {
                .cb = &loop_request_add_object,
                .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
            };

            struct object_info object_info = {
                .name = path_copy,
                .traits = trait_names,
            };

            DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct object_info, &object_info);

            return dicey_server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

enum dicey_error dicey_server_add_object_with(struct dicey_server *const server, const char *const path, ...) {
    assert(server && path);
    va_list args;

    va_start(args, path);

    struct dicey_hashset *traits = NULL;
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

enum dicey_error dicey_server_add_object_alias(
    struct dicey_server *const server,
    const char *const path,
    const char *const alias
) {
    assert(server && path && alias);

    struct dicey_hashset *aliases = NULL;
    if (dicey_hashset_add(&aliases, alias) == DICEY_HASH_SET_FAILED) {
        return TRACE(DICEY_ENOMEM);
    }

    return dicey_server_add_object_aliases(server, path, aliases);
}

enum dicey_error dicey_server_add_object_aliases(
    struct dicey_server *const server,
    const char *const path,
    struct dicey_hashset *aliases
) {
    assert(server && path && aliases);

    switch ((enum dicey_server_state) server->state) {
    // directly add the aliases to the registry if the server is not running
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            const enum dicey_error err = registry_add_aliases(registry, path, aliases);

            dicey_hashset_delete(aliases);

            return err;
        }

    // submit a request to the server loop if the server is running
    case SERVER_STATE_RUNNING:
        {
            struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct aliases_info);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            char *const path_copy = strdup(path);
            if (!path_copy) {
                free(req);
                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct dicey_server_loop_request) {
                .cb = &loop_request_add_aliases,
                .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
            };

            struct aliases_info aliases_info = {
                .path = path_copy,
                .aliases = aliases,
            };

            DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct aliases_info, &aliases_info);

            return dicey_server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

enum dicey_error dicey_server_add_trait(struct dicey_server *const server, struct dicey_trait *const trait) {
    assert(server && trait);

    switch ((enum dicey_server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_add_trait(registry, trait);
        }

    case SERVER_STATE_RUNNING:
        {
            struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct dicey_trait *);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct dicey_server_loop_request) {
                .cb = &loop_request_add_trait,
                .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
            };

            DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct dicey_trait *, &trait);

            return dicey_server_submit_request(server, req);
        }

    default:
        return TRACE(DICEY_EINVAL);
    }
}

enum dicey_error dicey_server_delete_object(struct dicey_server *const server, const char *const path) {
    assert(server && path);

    // TODO: analyse the thread safety of this approach
    switch ((enum dicey_server_state) server->state) {
    case SERVER_STATE_UNINIT:
    case SERVER_STATE_INIT:
        {
            struct dicey_registry *const registry = dicey_server_get_registry(server);
            assert(registry);

            return dicey_registry_delete_object(registry, path);
        }

    case SERVER_STATE_RUNNING:
        {
            const size_t path_size = dutl_zstring_size(path);
            struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(path_size);
            if (!req) {
                return TRACE(DICEY_ENOMEM);
            }

            *req = (struct dicey_server_loop_request) {
                .cb = &loop_request_del_object,
                .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
            };

            struct dicey_view_mut payload = DICEY_SERVER_LOOP_REQ_GET_PAYLOAD_AS_VIEW_MUT(*req, path_size);
            const ptrdiff_t result = dicey_view_mut_write_zstring(&payload, path);
            if (result < 0) {
                free(req);

                return (enum dicey_error) result;
            }

            assert((size_t) result == path_size);

            return dicey_server_submit_request(server, req);
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

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(enum dicey_bye_reason);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &loop_request_kick_client,
        .target = id,
    };

    const enum dicey_bye_reason reason = DICEY_BYE_REASON_KICKED;

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, enum dicey_bye_reason, &reason);

    return dicey_server_blocking_request(server, req);
}

enum dicey_error dicey_server_raise(struct dicey_server *const server, const struct dicey_packet packet) {
    assert(server && dicey_packet_is_valid(packet));

    if (!can_send_as_event(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct dicey_packet);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &loop_request_raise_signal,
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
    };

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct dicey_packet, &packet);

    return dicey_server_submit_request(server, req);
}

enum dicey_error dicey_server_raise_and_wait(struct dicey_server *const server, const struct dicey_packet packet) {
    assert(server && dicey_packet_is_valid(packet));

    if (!can_send_as_event(packet)) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct dicey_packet);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &loop_request_raise_signal,
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
    };

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct dicey_packet, &packet);

    return dicey_server_blocking_request(server, req);
}

enum dicey_error dicey_server_client_raised_error(
    struct dicey_server *server,
    struct dicey_client_data *const client,
    const enum dicey_error err
) {
    assert(client && server);

    dicey_client_data_set_state(client, CLIENT_DATA_STATE_DEAD);

    server->on_error(server, err, &client->info, "client error: %s", dicey_error_name(err));

    return server_kick_client(server, client, DICEY_BYE_REASON_ERROR);
}

enum dicey_error dicey_server_raise_internal(struct dicey_server *const server, struct dicey_packet packet) {
    assert(server);

    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(packet, &msg);
    if (err) {
        return err;
    }

    // start with 1, because if the first send fails, we risk prematurely freeing the packet
    struct dicey_shared_packet *shared_pkt = dicey_shared_packet_from(packet, 1);
    if (!shared_pkt) {
        dicey_packet_deinit(&packet);

        return TRACE(DICEY_ENOMEM);
    }

    const char *const elemdescr = dicey_element_descriptor_format_to(&server->scratchpad, msg.path, msg.selector);
    if (!elemdescr) {
        dicey_shared_packet_unref(shared_pkt);

        return TRACE(DICEY_ENOMEM);
    }

    err = dicey_packet_set_seq(dicey_shared_packet_borrow(shared_pkt), server_next_seq(server));
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

        struct outbound_packet event = { .kind = DICEY_OP_SIGNAL, .shared = shared_pkt };

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

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct dicey_packet);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &loop_request_send_response,
        .target = id,
    };

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct dicey_packet, &packet);

    return dicey_server_submit_request(server, req);
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

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct dicey_packet);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &loop_request_send_response,
        .target = id,
    };

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct dicey_packet, &packet);

    return dicey_server_blocking_request(server, req);
}

void *dicey_server_set_context(struct dicey_server *const server, void *const new_context) {
    assert(server);

    void *const old_context = server->ctx;
    server->ctx = new_context;

    return old_context;
}

enum dicey_error dicey_server_start(struct dicey_server *const server, struct dicey_addr addr) {
    assert(server && addr.addr && addr.len);

    int uverr = uv_pipe_bind2(&server->pipe, addr.addr, addr.len, 0U);

    dicey_addr_deinit(&addr);

    if (uverr < 0) {
        goto fail;
    }

    uverr = uv_prepare_start(&server->startup_prepare, &server_init_notify_startup);
    if (uverr) {
        goto fail;
    }

    uverr = uv_listen((uv_stream_t *) &server->pipe, 128, &on_connect);

    if (uverr < 0) {
        goto after_prepare;
    }

    server->state = SERVER_STATE_RUNNING;

    uverr = uv_run(&server->loop, UV_RUN_DEFAULT);
    if (uverr < 0) {
        goto after_prepare;
    }

    server->state = SERVER_STATE_INIT;

    return DICEY_OK;

after_prepare:
    uv_prepare_stop(&server->startup_prepare);

fail:
    { // declaration after label is only allowed in C23 and later
        const enum dicey_error err = dicey_error_from_uv(uverr);
        server_report_startup(server, err);

        return dicey_error_from_uv(err);
    }
}

enum dicey_error dicey_server_start_reading_from_client_internal(struct dicey_server *const server, const size_t id) {
    struct dicey_client_data *const client = dicey_client_list_get_client(server->clients, id);

    const int err = uv_read_start((uv_stream_t *) client, &alloc_buffer, &on_read);

    return dicey_error_from_uv(err);
}

enum dicey_error dicey_server_stop(struct dicey_server *const server) {
    assert(server);

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW_EMPTY();
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = LOOP_REQUEST_SHUTDOWN;

    const enum dicey_error err = dicey_server_submit_request(server, req);
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

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW_EMPTY();
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = LOOP_REQUEST_SHUTDOWN;

    return dicey_server_blocking_request(server, req);
}
