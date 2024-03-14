// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ORNYMSAZGZ_TASK_LIST_H)
#define ORNYMSAZGZ_TASK_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#define WAIT_FOREVER ((int32_t) -1)

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

struct dicey_task_entry {
    int64_t id;
    uv_timespec64_t expires_at;

    void *data;
};

struct dicey_task_list {
    int64_t next_id;
    size_t len, cap;
    struct dicey_task_entry waiting[];
};

int64_t dicey_task_list_append(struct dicey_task_list **list_ptr, void *entry_data, const int32_t delay_ms);
const struct dicey_task_entry *dicey_task_list_begin(const struct dicey_task_list *list);
const struct dicey_task_entry *dicey_task_list_end(const struct dicey_task_list *list);
bool dicey_task_list_erase(struct dicey_task_list *list, int64_t id);
const struct dicey_task_entry *dicey_task_list_find(const struct dicey_task_list *list, int64_t id);

typedef void dicey_task_list_expired_fn(void *ctx, int64_t id, void *expired_item);
void dicey_task_list_prune(struct dicey_task_list *list, dicey_task_list_expired_fn *expired_cb, void *ctx);

#endif // ORNYMSAZGZ_TASK_LIST_H
