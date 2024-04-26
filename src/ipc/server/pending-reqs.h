// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

#endif // VCCPEAPUDR_PENDING_REQS_H
