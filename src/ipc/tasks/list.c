// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include "list.h"

#define BASE_CAP 128U
#define FOREVER_TIME                                                                                                   \
    (uv_timespec64_t) { 0 }

static int32_t add_wrapping(int32_t *const a, const int32_t b, const int32_t max) {
    const imaxdiv_t a_div = imaxdiv(*a, max);
    const imaxdiv_t b_div = imaxdiv(b, max);

    *a = (int32_t) (a_div.rem + b_div.rem) % max;

    return (int32_t) (a_div.quot + b_div.quot);
}

static void task_list_erase_at(struct dicey_task_list *const list, const size_t entry) {
    assert(list);

    if (entry >= list->len) {
        return; // the node has been already removed somehow
    }

    if (entry + 1 < list->len) {
        const ptrdiff_t len_after = list->len - entry - 1;

        memmove(&list->waiting[entry], &list->waiting[entry + 1], len_after * sizeof *list->waiting);
    }

    --list->len;
}

static ptrdiff_t task_list_find_index(const struct dicey_task_list *const list, const int64_t id) {
    assert(list);

    const size_t len = list->len;
    assert(len <= (size_t) PTRDIFF_MAX);

    for (ptrdiff_t i = 0; i < (ptrdiff_t) len; ++i) {
        if (list->waiting[i].id == id) {
            return i;
        }
    }

    return -1;
}

static uv_timespec64_t now_plus(const int32_t ms) {
    uv_timespec64_t now = { 0 };
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);

    const int32_t nsec = (ms % 1000) * 1000000;

    const int32_t sec = ms / 1000 + add_wrapping(&now.tv_nsec, nsec, 1000000000);

    now.tv_sec += sec;

    return now;
}

static bool is_wait_forever(const uv_timespec64_t ts) {
    return !ts.tv_sec && !ts.tv_nsec;
}

static bool task_list_grow_if_needed(struct dicey_task_list **const list_ptr) {
    assert(list_ptr);

    struct dicey_task_list *list = *list_ptr;

    const size_t len = list ? list->len : 0U;
    if ((uintmax_t) len >= (uintmax_t) INT64_MAX) {
        return false; // ids go up to int64_t and are positive
    }

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

int64_t dicey_task_list_append(
    struct dicey_task_list **const list_ptr,
    void *const entry_data,
    const int32_t delay_ms
) {
    assert(list_ptr && entry_data);

    if (!task_list_grow_if_needed(list_ptr)) {
        return -1;
    }

    struct dicey_task_list *const list = *list_ptr;

    const int64_t id = list->next_id++;

    list->waiting[list->len++] = (struct dicey_task_entry) {
        .id = id,
        .expires_at = delay_ms == WAIT_FOREVER ? FOREVER_TIME : now_plus(delay_ms),
        .data = entry_data,
    };

    return id;
}

const struct dicey_task_entry *dicey_task_list_begin(const struct dicey_task_list *const list) {
    return list ? list->waiting : NULL;
}

const struct dicey_task_entry *dicey_task_list_end(const struct dicey_task_list *list) {
    return list ? list->waiting + list->len : NULL;
}

bool dicey_task_list_erase(struct dicey_task_list *const list, const int64_t id) {
    assert(list);

    const ptrdiff_t index = task_list_find_index(list, id);
    if (index < 0) {
        return false;
    }

    task_list_erase_at(list, index);

    return true;
}

const struct dicey_task_entry *dicey_task_list_find(const struct dicey_task_list *const list, const int64_t id) {
    assert(list);

    const ptrdiff_t index = task_list_find_index(list, id);

    return index >= 0 ? &list->waiting[index] : NULL;
}

void dicey_task_list_prune(
    struct dicey_task_list *const task,
    dicey_task_list_expired_fn *const expired_cb,
    void *const ctx
) {
    assert(expired_cb);

    // note: for the time being this is being done only once in the entire function, in order not to penalise the last
    // items if the previous callbacks were very slow
    uv_timespec64_t now = { 0 };
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &now);

    if (!task) {
        return;
    }

    size_t index = 0;
    for (;;) {
        if (index >= task->len) {
            break;
        }

        struct dicey_task_entry *const item = &task->waiting[index];

        const uv_timespec64_t expires_at = item->expires_at;

        if (!is_wait_forever(expires_at) && timespec_cmp(expires_at, now) < 0) {
            expired_cb(ctx, item->id, item->data);

            task_list_erase_at(task, index);

            // note: keep the index at the current value. The rest of the array has been shifted down by one.
            // Len has been decreased by one.
            return;
        } else {
            // move to the next position. All items checked so far are still valid.
            ++index;
        }
    }
}
