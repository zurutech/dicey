// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "waiting-list.h"

#define BASE_CAP 128U

struct dicey_waiting_list {
    size_t len, cap;
    struct dicey_waiting_task waiting[];
};

static bool pending_list_grow_if_needed(struct dicey_waiting_list **const list_ptr) {
    assert(list_ptr);

    struct dicey_waiting_list *list = *list_ptr;

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

static void waiting_list_erase(struct dicey_waiting_list *const list, const size_t entry) {
    assert(list && entry < list->len && list->len);

    if (entry + 1 < list->len) {
        const ptrdiff_t len_after = list->len - entry - 1;

        memmove(&list->waiting[entry], &list->waiting[entry + 1], len_after * sizeof *list->waiting);
    }

    --list->len;
}

bool dicey_waiting_list_append(struct dicey_waiting_list **const list_ptr, const uint32_t seq, const uint64_t task_id) {
    assert(list_ptr);

    if (!pending_list_grow_if_needed(list_ptr)) {
        return false;
    }

    struct dicey_waiting_list *const list = *list_ptr;

    list->waiting[list->len++] = (struct dicey_waiting_task) { .packet_seq = seq, .task_id = task_id };

    return true;
}

const struct dicey_waiting_task *dicey_waiting_list_begin(const struct dicey_waiting_list *const list) {
    return list ? list->waiting : NULL;
}

void dicey_waiting_list_clear(struct dicey_waiting_list *const list) {
    if (list) {
        list->len = 0U;
    }
}

const struct dicey_waiting_task *dicey_waiting_list_end(const struct dicey_waiting_list *list) {
    return list ? list->waiting + list->len : NULL;
}

bool dicey_waiting_list_remove_seq(struct dicey_waiting_list *const list, const uint32_t seq, uint64_t *const task_id) {
    assert(list);

    for (size_t i = 0; i < list->len; ++i) {
        if (list->waiting[i].packet_seq == seq) {
            if (task_id) {
                *task_id = list->waiting[i].task_id;
            }

            waiting_list_erase(list, i);

            return true;
        }
    }

    return false;
}

bool dicey_waiting_list_remove_task(
    struct dicey_waiting_list *const list,
    const uint64_t task_id,
    uint32_t *const seq
) {
    if (!list) {
        return false;
    }

    for (size_t i = 0; i < list->len; ++i) {
        if (list->waiting[i].task_id == task_id) {
            if (seq) {
                *seq = list->waiting[i].packet_seq;
            }

            waiting_list_erase(list, i);

            return true;
        }
    }

    return false;
}
