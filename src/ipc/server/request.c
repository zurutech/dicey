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
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/message.h>

#include <dicey/ipc/registry.h>
#include <dicey/ipc/request.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"

#include "wirefmt/packet-args.h"

#include "pending-reqs.h"
#include "server-internal.h"

enum reply_policy {
    REPLY_POLICY_ASYNC,    // reply asynchronously, without waiting for the response to be sent
    REPLY_POLICY_BLOCKING, // reply synchronously, waiting for the response to be sent
};

static enum dicey_error send_reply(
    struct dicey_request *const req,
    const enum reply_policy policy,
    const struct dicey_arg arg
) {
    assert(req);

    struct dicey_value_builder builder = { 0 };

    enum dicey_error err = dicey_request_response_start(req, &builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(&builder, arg);
    if (err) {
        return dicey_request_response_reset(req, &builder);
    }

    return policy == REPLY_POLICY_ASYNC ? dicey_request_response_send(req, &builder)
                                        : dicey_request_response_send_and_wait(req, &builder);
}

static enum dicey_error reply_with_existing(
    struct dicey_request *const req,
    const enum reply_policy policy,
    const struct dicey_value *const value
) {
    assert(req && value);

    struct dicey_arg arg = { 0 };

    const enum dicey_error err = dicey_arg_from_borrowed_value(&arg, value);
    if (err) {
        return err;
    }

    const enum dicey_error send_err = send_reply(req, policy, arg);

    dicey_arg_free_contents(&arg);

    return send_err;
}

enum dicey_error finalize_request(
    struct dicey_request *const req,
    const enum reply_policy policy,
    struct dicey_value_builder *const builder
) {
    assert(req && builder);

    if (req->state != DICEY_REQUEST_STATE_CONSTRUCTING || !dicey_value_builder_is_pending(builder)) {
        return TRACE(DICEY_EINVAL);
    }

    assert(dicey_message_builder_is_pending(&req->resp_builder));

    struct dicey_packet reply = { 0 };

    enum dicey_error err = dicey_message_builder_build(&req->resp_builder, &reply);
    if (err) {
        req->state = DICEY_REQUEST_STATE_ABORTED;

        return err;
    }

    err = policy == REPLY_POLICY_ASYNC ? dicey_server_send_response(req->server, req->cln.id, reply)
                                       : dicey_server_send_response_and_wait(req->server, req->cln.id, reply);

    if (err) {
        // if the response could not be sent, reset the request state to failed
        req->state = DICEY_REQUEST_STATE_ABORTED;

        // deinitialize the packet
        dicey_packet_deinit(&reply);

        return err;
    }

    req->state = DICEY_REQUEST_STATE_COMPLETED;

    return DICEY_OK;
}

void dicey_request_deinit(struct dicey_request *const req) {
    if (req) {
        // deinitialize the response builder
        dicey_message_builder_discard(&req->resp_builder);

        // free the packet if it was allocated
        dicey_packet_deinit(&req->packet);

        // zero out the request
        *req = (struct dicey_request) { 0 };
    }
}

enum dicey_error dicey_request_fail(struct dicey_request *const req, const uint16_t code, const char *const msg) {
    assert(req);

    // set the error code and message in the response builder
    return dicey_request_reply(req, (struct dicey_arg) {
        .type = DICEY_TYPE_ERROR,
        .error = {
            .code = code,
            .message = msg, // null is fine here, it will be ignored
        },
    });
}

enum dicey_error dicey_request_fail_and_wait(
    struct dicey_request *const req,
    const uint16_t code,
    const char *const msg
) {
    assert(req);

    // set the error code and message in the response builder
    return dicey_request_reply_and_wait(req, (struct dicey_arg) {
        .type = DICEY_TYPE_ERROR,
        .error = {
            .code = code,
            .message = msg, // null is fine here, it will be ignored
        },
    });
}

const struct dicey_client_info *dicey_request_get_client_info(const struct dicey_request *const req) {
    return req ? &req->cln : NULL;
}

const struct dicey_message *dicey_request_get_message(const struct dicey_request *const req) {
    return req ? &req->message : NULL;
}

const char *dicey_request_get_real_path(const struct dicey_request *const req) {
    return req ? req->real_path : NULL;
}

uint32_t dicey_request_get_seq(const struct dicey_request *const req) {
    assert(req);

    return req->packet_seq;
}

enum dicey_error dicey_request_reply(struct dicey_request *const req, const struct dicey_arg arg) {
    return send_reply(req, REPLY_POLICY_ASYNC, arg);
}

enum dicey_error dicey_request_reply_and_wait(struct dicey_request *const req, const struct dicey_arg arg) {
    return send_reply(req, REPLY_POLICY_BLOCKING, arg);
}

enum dicey_error dicey_request_reply_with_existing(
    struct dicey_request *const req,
    const struct dicey_value *const value
) {
    return reply_with_existing(req, REPLY_POLICY_ASYNC, value);
}

enum dicey_error dicey_request_reply_with_existing_and_wait(
    struct dicey_request *const req,
    const struct dicey_value *const value
) {
    return reply_with_existing(req, REPLY_POLICY_BLOCKING, value);
}

enum dicey_error dicey_request_response_send(
    struct dicey_request *const req,
    struct dicey_value_builder *const builder
) {
    return finalize_request(req, REPLY_POLICY_ASYNC, builder);
}

enum dicey_error dicey_request_response_send_and_wait(
    struct dicey_request *const req,
    struct dicey_value_builder *const builder
) {
    return finalize_request(req, REPLY_POLICY_BLOCKING, builder);
}

enum dicey_error dicey_request_response_reset(
    struct dicey_request *const req,
    struct dicey_value_builder *const builder
) {
    assert(req && builder);

    if (req->state != DICEY_REQUEST_STATE_CONSTRUCTING) {
        return TRACE(DICEY_EINVAL);
    }

    assert(dicey_message_builder_is_pending(&req->resp_builder));

    const enum dicey_error err = dicey_message_builder_value_end(&req->resp_builder, builder);
    if (err) {
        return err;
    }

    req->state = DICEY_REQUEST_STATE_PENDING;

    return DICEY_OK;
}

enum dicey_error dicey_request_response_start(struct dicey_request *req, struct dicey_value_builder *builder) {
    assert(req && builder);

    if (req->state != DICEY_REQUEST_STATE_PENDING || dicey_value_builder_is_pending(builder)) {
        return TRACE(DICEY_EINVAL);
    }

    assert(dicey_message_builder_is_pending(&req->resp_builder));

    enum dicey_error err = dicey_message_builder_value_start(&req->resp_builder, builder);
    if (err) {
        return err;
    }

    // set the request state to constructing
    req->state = DICEY_REQUEST_STATE_CONSTRUCTING;

    return DICEY_OK;
}

enum dicey_error dicey_server_request_for(
    struct dicey_server *const server,
    struct dicey_client_info *const cln,
    const struct dicey_packet packet,
    struct dicey_request *const dest
) {
    assert(server && dicey_packet_is_valid(packet) && dest);

    enum dicey_error err = dicey_packet_as_message(packet, &dest->message);
    if (err) {
        goto fail;
    }

    err = dicey_packet_get_seq(packet, &dest->packet_seq);
    if (err) {
        goto fail;
    }

    const struct dicey_element *elem = dicey_registry_get_element(
        &server->registry, dest->message.path, dest->message.selector.trait, dest->message.selector.elem
    );

    if (!elem) {
        err = TRACE(DICEY_EELEMENT_NOT_FOUND);
        goto fail;
    }

    assert(elem->signature);

    err = dicey_message_builder_begin(&dest->resp_builder, DICEY_OP_RESPONSE);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_seq(&dest->resp_builder, dest->packet_seq);
    if (err) {
        goto deinit_all;
    }

    err = dicey_message_builder_set_path(&dest->resp_builder, dest->message.path);
    if (err) {
        goto deinit_all;
    }

    err = dicey_message_builder_set_selector(&dest->resp_builder, dest->message.selector);
    if (err) {
        goto deinit_all;
    }

    dest->real_path = dest->message.path;
    dest->packet = packet;
    dest->cln = *cln; // copy the client info, it's just a few bytes
    dest->op = dest->message.type;
    dest->state = DICEY_REQUEST_STATE_PENDING;
    dest->signature = elem->signature;
    dest->server = server;

    // hide the message path from the user-facing code
    dest->message.path = dicey_registry_get_main_path(&server->registry, dest->message.path);

    return DICEY_OK;

deinit_all:
    dicey_message_builder_discard(&dest->resp_builder);

fail:
    *dest = (struct dicey_request) { 0 };

    return err;
}
