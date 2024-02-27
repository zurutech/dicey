// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>

#include <uv.h>

#include "pending-list.h"

#define BASE_CAP 128U

static bool pending_list_grow_if_needed(struct dicey_pending_list **const list_ptr) {
    assert(list_ptr);

    struct dicey_pending_list *list = *list_ptr;

    const size_t len = list ? list->len : 0U;
    const size_t old_cap = list ? list->cap : 0U;

    if (len < old_cap) {
        return true;
    }

    const size_t new_cap = old_cap ? old_cap * 3U / 2U : BASE_CAP;

    if (new_cap < old_cap) { // overflow
        return false;
    }

    const size_t new_size = sizeof *list + new_cap * sizeof *list->waiting;

    list = list ? realloc(list, new_size) : calloc(1U, new_size);
    if (!list) {
        free(*list_ptr);

        return false;
    }

    list->cap = new_cap;

    *list_ptr = list;

    return true;
}

static int timespec_cmp(const uv_timespec64_t a, const uv_timespec64_t b) {
    int sec_cmp = (a.tv_sec > b.tv_sec) - (a.tv_sec < b.tv_sec);

    return sec_cmp ? sec_cmp : (a.tv_nsec > b.tv_nsec) - (a.tv_nsec < b.tv_nsec);
}

bool dicey_pending_list_append(struct dicey_pending_list **const list_ptr, struct dicey_pending_reply *const reply) {
    assert(list_ptr && reply);

    if (!pending_list_grow_if_needed(list_ptr)) {
        return false;
    }

    struct dicey_pending_list *const list = *list_ptr;

    list->waiting[list->len++] = *reply;

    return true;
}

const struct dicey_pending_reply *dicey_pending_list_begin(const struct dicey_pending_list *const list) {
    return list ? list->waiting : NULL;
}

const struct dicey_pending_reply *dicey_pending_list_end(const struct dicey_pending_list *list) {
    return list ? list->waiting + list->len : NULL;
}

void dicey_pending_list_erase(struct dicey_pending_list *const list, const size_t entry) {
    assert(list && entry < list->len && list->len);

    if (entry + 1 < list->len) {
        const ptrdiff_t len_after = list->len - entry - 1;

        memmove(&list->waiting[entry], &list->waiting[entry + 1], len_after * sizeof *list->waiting);
    }

    --list->len;
}

void dicey_pending_list_prune(struct dicey_pending_list *const pending, struct dicey_client *const client) {
    // note: for the time being this is being done only once in the entire function, in order not to penalise the last
    // items if the previous callbacks were very slow
    uv_timespec64_t now = { 0 };
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);

    if (!pending) {
        return;
    }

    size_t index = 0;
    for (;;) {
        if (index >= pending->len) {
            break;
        }

        struct dicey_pending_reply *const item = &pending->waiting[index];

        if (timespec_cmp(item->expires_at, now) < 0) {
            assert(item->callback);

            item->callback(client, DICEY_ETIMEDOUT, (struct dicey_packet) { 0 }, item->data);

            dicey_pending_list_erase(pending, index);

            // note: keep the index at the current value. The rest of the array has been shifted down by one.
            // Len has been decreased by one.
            return;
        } else {
            // move to the next position. All items checked so far are still valid.
            ++index;
        }
    }
}

bool dicey_pending_list_search_and_remove(
    struct dicey_pending_list *const list,
    const uint32_t seq,
    struct dicey_pending_reply *const found
) {
    assert(list && found);

    for (size_t i = 0; i < list->len; ++i) {
        if (list->waiting[i].seq == seq) {
            *found = list->waiting[i];

            dicey_pending_list_erase(list, i);

            return true;
        }
    }

    return false;
}
