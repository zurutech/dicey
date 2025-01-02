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

#if !defined(ORNYMSAZGZ_TASK_LIST_H)
#define ORNYMSAZGZ_TASK_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include "dicey_config.h"

#define WAIT_FOREVER ((int32_t) -1)

#if defined(DICEY_CC_IS_MSVC)
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
