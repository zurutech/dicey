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

#if !defined(ORNYMSAZGZ_PENDING_LIST_H)
#define ORNYMSAZGZ_PENDING_LIST_H

#include <stdbool.h>
#include <stdint.h>

struct dicey_waiting_task {
    uint32_t packet_seq;
    uint64_t task_id;
};

struct dicey_waiting_list;

struct dicey_waiting_task *dicey_waiting_list_append(
    struct dicey_waiting_list **list_ptr,
    struct dicey_waiting_task *task
);

const struct dicey_waiting_task *dicey_waiting_list_cbegin(const struct dicey_waiting_list *list);
const struct dicey_waiting_task *dicey_waiting_list_cend(const struct dicey_waiting_list *list);

void dicey_waiting_list_clear(struct dicey_waiting_list *list);

bool dicey_waiting_list_remove_seq(struct dicey_waiting_list *list, uint32_t seq, uint64_t *task_id);

bool dicey_waiting_list_remove_task(struct dicey_waiting_list *list, uint64_t task_id, uint32_t *seq);

#endif // ORNYMSAZGZ_PENDING_LIST_H
