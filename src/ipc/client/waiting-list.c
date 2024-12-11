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

#define ARRAY_EXPORT
#define ARRAY_TYPE_NAME dicey_waiting_list
#define ARRAY_VALUE_TYPE struct dicey_waiting_task
#include "sup/array.inc"

bool dicey_waiting_list_remove_seq(struct dicey_waiting_list *const list, const uint32_t seq, uint64_t *const task_id) {
    assert(list);

    size_t i = 0;
    for (const struct dicey_waiting_task *it = dicey_waiting_list_begin(list),
                                         *const end = dicey_waiting_list_end(list);
         it < end;
         ++it, ++i) {
        if (it->packet_seq == seq) {
            if (task_id) {
                *task_id = it->task_id;
            }

            dicey_waiting_list_erase(list, i);

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
        if (list->data[i].task_id == task_id) {
            if (seq) {
                *seq = list->data[i].packet_seq;
            }

            dicey_waiting_list_erase(list, i);

            return true;
        }
    }

    return false;
}
