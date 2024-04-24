// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/core/errors.h>

#include "sup/trace.h"

#include "pending-reqs.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#define FIRST_SEQ ((uint32_t) 2U)
#define STARTING_CAP 12U

struct dicey_pending_requests {
    uint32_t last_seq;

    size_t start, end, len, cap;
    struct dicey_pending_request reqs[];
};

static size_t index_of(const struct dicey_pending_requests *const reqs, const size_t i) {
    return (reqs->start + i) % reqs->cap;
}

static bool is_hole(const struct dicey_pending_request *const req) {
    return !req->path; // just any value - if the path is NULL, the request is empty
}

static size_t next_index(const struct dicey_pending_requests *const reqs, const size_t i) {
    assert(reqs);

    return (i + 1) % reqs->cap;
}

static uint32_t next_seq(const uint32_t cur_seq) {
    return cur_seq < UINT32_MAX - 2U ? cur_seq + 2U : FIRST_SEQ;
}

static void pending_request_invalidate(struct dicey_pending_request *const req) {
    assert(req);

    req->op = DICEY_OP_INVALID;
    req->path = NULL;
    req->sel = (struct dicey_selector) { 0 };

    // leave the seq be, it's useful for binary search
}

static bool pending_request_is_valid(const struct dicey_pending_request *const req) {
    return req && req->op != DICEY_OP_INVALID && req->path && dicey_selector_is_valid(req->sel);
}

static struct dicey_pending_request *request_at(struct dicey_pending_requests *const reqs, const size_t i) {
    assert(reqs);

    return &reqs->reqs[index_of(reqs, i)];
}

static struct dicey_pending_request *search_seq(struct dicey_pending_requests *const reqs, const uint32_t seq) {
    if (!reqs->len) {
        return NULL;
    }

    size_t l = 0, r = reqs->len - 1;

    while (l <= r) {
        size_t m = (l + r) / 2;

        struct dicey_pending_request *const req = request_at(reqs, m);
        assert(req); // the value _must_ be valid, it comes from the list and we know m is in range

        if (req->packet_seq < seq) {
            l = m + 1;
        } else if (req->packet_seq > seq) {
            r = m - 1;
        } else {
            return pending_request_is_valid(req) ? req : NULL;
        }
    }

    return NULL;
}

static enum dicey_error pending_requests_make_room(struct dicey_pending_requests **reqs_ptr) {
    assert(reqs_ptr && *reqs_ptr);

    struct dicey_pending_requests *reqs = *reqs_ptr;
    assert(reqs->len <= reqs->cap);

    // if len == cap, then there are no holes in the list and we really must reallocate
    // also reallocate if occupation is above 80%, in order to minimise how many time compacting is needed
    // TODO: find a way to go around possible intereger overflows
    const size_t occupation = reqs->len * 100U / reqs->cap;

    const struct dicey_pending_requests *const old_reqs = reqs;
    if (occupation > 80U) {
        const size_t new_cap = reqs->cap * 3U / 2U;
        reqs = calloc(1U, sizeof *reqs + new_cap * sizeof *reqs->reqs);
        if (!reqs) {
            return TRACE(DICEY_ENOMEM);
        }

        // copy the fixed part of the struct
        *reqs = *old_reqs;

        reqs->cap = new_cap;

        // reset the indeces - we are operating on a new circular memory block
        reqs->start = 0;
        reqs->end = reqs->len;
    }

    *reqs_ptr = reqs;

    if (!old_reqs->len) {
        // nothing to copy, we are done

        return DICEY_OK;
    }

    // compact the list

    // we know we have len elements and 0 or more holes. Iterate the circular buffer(s) until we get len elements
    // this works the same either with a new or the same buffer - i goes faster than o and thus we can compact the list
    // in-place
    size_t i = old_reqs->start, o = reqs->start;
    do {
        const struct dicey_pending_request *const req = &old_reqs->reqs[i];
        if (!is_hole(req)) {
            reqs->reqs[o] = *req;

            o = next_index(reqs, o);
        }

        i = next_index(old_reqs, i);
    } while (i != old_reqs->end);

    return DICEY_OK;
}

static bool room_available(const struct dicey_pending_requests *const reqs) {
    // the structure has room available if:
    // - it has a capacity (i.e. it's not empty)
    // - the start and end indeces are different (and if cap is not 0, it means we've ran the circular buffer back to
    // the start)
    return reqs->cap && reqs->start != reqs->end;
}

static bool validate_seqnum(
    const struct dicey_pending_requests *const reqs,
    const struct dicey_pending_request *const req
) {
    assert(req);

    if (!reqs) {
        // if the list is empty, we can accept the request. It must have seq == 2
        return req->packet_seq == FIRST_SEQ;
    }

    // otherwise, the request must come after the latest request with a seq number incremented by 2
    const uint32_t expected = next_seq(reqs->last_seq);
    if (req->packet_seq != expected) {
        return false;
    }

    return true;
}

enum dicey_error dicey_pending_requests_add(
    struct dicey_pending_requests **const reqs_ptr,
    const struct dicey_pending_request *const req
) {
    assert(reqs_ptr && pending_request_is_valid(req));

    struct dicey_pending_requests *reqs = *reqs_ptr;

    if (!validate_seqnum(reqs, req)) {
        return TRACE(DICEY_ESEQNUM_MISMATCH);
    }

    if (reqs) {
        // accept the new sequence number now - otherwise the client will get stuck or kicked
        reqs->last_seq = req->packet_seq;

        if (!room_available(reqs)) {
            const enum dicey_error err = pending_requests_make_room(&reqs);
            if (err != DICEY_OK) {
                return err;
            }

            // if this is true, the pointer has been reallocated. Clean up the old memory and update the pointer
            if (reqs != *reqs_ptr) {
                free(*reqs_ptr);

                *reqs_ptr = reqs;
            }
        }
    } else {
        *reqs_ptr = reqs = calloc(1U, sizeof *reqs + STARTING_CAP * sizeof *reqs->reqs);
        if (!reqs) {
            return TRACE(DICEY_ENOMEM);
        }

        *reqs = (struct dicey_pending_requests
        ) { .last_seq = req->packet_seq, .start = 0, .end = 0, .len = 0, .cap = STARTING_CAP };
    }

    reqs->reqs[reqs->end] = *req;
    reqs->end = next_index(reqs, reqs->end);

    ++reqs->len;

    return DICEY_OK;
}

enum dicey_error dicey_pending_requests_complete(
    struct dicey_pending_requests *const reqs,
    const uint32_t seq,
    struct dicey_pending_request *const req
) {
    if (!reqs || !reqs->len) {
        return TRACE(DICEY_ENOENT);
    }

    struct dicey_pending_request *const ext_req = search_seq(reqs, seq);
    if (!ext_req) {
        return TRACE(DICEY_ENOENT);
    }

    if (req) {
        *req = *ext_req;
    }

    pending_request_invalidate(ext_req);

    return DICEY_OK;
}

const struct dicey_pending_request *dicey_pending_requests_get(struct dicey_pending_requests *reqs, uint32_t seq) {
    if (!reqs || !reqs->len) {
        return false;
    }

    return search_seq(reqs, seq);
}

bool dicey_pending_requests_is_pending(struct dicey_pending_requests *const reqs, const uint32_t seq) {
    return dicey_pending_requests_get(reqs, seq);
}

void dicey_pending_requests_prune(
    struct dicey_pending_requests *const reqs,
    dicey_pending_request_prune_fn *const prune_fn,
    void *const ctx
) {
    assert(prune_fn);

    if (!reqs || !reqs->len) {
        return;
    }

    for (size_t i = 0; i < reqs->len; ++i) {
        struct dicey_pending_request *const req = request_at(reqs, i);
        assert(req);

        if (is_hole(req)) {
            continue;
        }

        if (prune_fn(req, ctx)) {
            pending_request_invalidate(req);
        }
    }
}
