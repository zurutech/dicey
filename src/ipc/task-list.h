// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ORNYMSAZGZ_TASK_LIST_H)
#define ORNYMSAZGZ_TASK_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

struct dicey_task_entry {
    uv_timespec64_t expires_at;

    void *data;
};

struct dicey_task_list {
    size_t len, cap;
    struct dicey_task_entry waiting[];
};

bool dicey_task_list_append(struct dicey_task_list **list_ptr, void *entry_data, const uint32_t delay_ms);
const struct dicey_task_entry *dicey_task_list_begin(const struct dicey_task_list *list);
const struct dicey_task_entry *dicey_task_list_end(const struct dicey_task_list *list);
bool dicey_task_list_erase(struct dicey_task_list *list, const void *entry_data);
void dicey_task_list_erase_at(struct dicey_task_list *list, size_t entry);

typedef void dicey_task_list_expired_fn(void *ctx, void *expired_item);
void dicey_task_list_prune(struct dicey_task_list *list, dicey_task_list_expired_fn *expired_cb, void *ctx);

#endif // ORNYMSAZGZ_TASK_LIST_H
