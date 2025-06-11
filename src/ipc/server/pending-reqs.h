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

#if !defined(VCCPEAPUDR_PENDING_REQS_H)
#define VCCPEAPUDR_PENDING_REQS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/message.h>
#include <dicey/core/packet.h>

#include <dicey/ipc/request.h>
#include <dicey/ipc/server-api.h>
#include <dicey/ipc/server.h>

enum dicey_request_state {
    DICEY_REQUEST_STATE_PENDING,      // the request is pending
    DICEY_REQUEST_STATE_CONSTRUCTING, // the request is being constructed
    DICEY_REQUEST_STATE_COMPLETED,    // the request is ready to be sent

    DICEY_REQUEST_STATE_ABORTED, // the request failed to be constructed or couldn't be sent
};

struct dicey_request {
    uint32_t packet_seq;
    enum dicey_op op;

    struct dicey_client_info cln;

    enum dicey_request_state state; // the current state of the request
    struct dicey_packet packet;     // the request packet
    const char *real_path; // the real path target by packet (only differs from message.path if the target is an alias)
    struct dicey_message message;              // the packet above, extracted as a message (except the path)
    struct dicey_message_builder resp_builder; // the response builder

    const char *signature;

    struct dicey_server *server; // the server that this request comes from
};

void dicey_request_deinit(struct dicey_request *req);

enum dicey_error dicey_server_request_for(
    struct dicey_server *server,
    struct dicey_client_info *cln,
    struct dicey_packet packet,
    struct dicey_request *dest
);

typedef bool dicey_pending_request_prune_fn(const struct dicey_request *req, void *ctx);

struct dicey_pending_requests;
struct dicey_pending_request_result {
    enum dicey_error error;
    struct dicey_request *value;
};

struct dicey_pending_request_result dicey_pending_requests_add(
    struct dicey_pending_requests **reqs_ptr,
    const struct dicey_request *req
);

enum dicey_error dicey_pending_requests_complete(
    struct dicey_pending_requests *reqs,
    uint32_t seq,
    struct dicey_request *req
);

const struct dicey_request *dicey_pending_requests_get(struct dicey_pending_requests *reqs, uint32_t seq);
bool dicey_pending_requests_is_pending(struct dicey_pending_requests *reqs, uint32_t seq);
void dicey_pending_requests_prune(
    struct dicey_pending_requests *reqs,
    dicey_pending_request_prune_fn *prune_fn,
    void *ctx
);

// skip a sequence number.
// This is required when a request is handled internally by the server; the pending_requests struct must remain in sync
// with the client's view of the world, even if there's no request pending at all.
enum dicey_error dicey_pending_request_skip(struct dicey_pending_requests **reqs_ptr, uint32_t seq);

#endif // VCCPEAPUDR_PENDING_REQS_H
