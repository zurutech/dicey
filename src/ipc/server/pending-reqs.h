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

#if !defined(VCCPEAPUDR_PENDING_REQS_H)
#define VCCPEAPUDR_PENDING_REQS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>

struct dicey_pending_request {
    uint32_t packet_seq;

    enum dicey_op op;

    const char *path;
    struct dicey_selector sel;

    const char *signature;
};

typedef bool dicey_pending_request_prune_fn(const struct dicey_pending_request *req, void *ctx);

struct dicey_pending_requests;

enum dicey_error dicey_pending_requests_add(
    struct dicey_pending_requests **reqs_ptr,
    const struct dicey_pending_request *req
);

enum dicey_error dicey_pending_requests_complete(
    struct dicey_pending_requests *reqs,
    uint32_t seq,
    struct dicey_pending_request *req
);

const struct dicey_pending_request *dicey_pending_requests_get(struct dicey_pending_requests *reqs, uint32_t seq);
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
