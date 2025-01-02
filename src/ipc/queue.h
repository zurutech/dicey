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

#if !defined(NYSSJKURJT_QUEUE_H)
#define NYSSJKURJT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>

#define REQUEST_QUEUE_CAP 128

enum dicey_locking_policy {
    DICEY_LOCKING_POLICY_BLOCKING,
    DICEY_LOCKING_POLICY_NONBLOCKING,
};

struct dicey_queue {
    uv_mutex_t mutex;
    uv_cond_t cond;

    void **data[REQUEST_QUEUE_CAP];
    ptrdiff_t head;
    ptrdiff_t tail;
};

typedef void free_data_fn(void *ctx, void *data);

void dicey_queue_deinit(struct dicey_queue *queue, free_data_fn *free_fn, void *ctx);
enum dicey_error dicey_queue_init(struct dicey_queue *queue);
bool dicey_queue_pop(struct dicey_queue *queue, void **val, enum dicey_locking_policy policy);
bool dicey_queue_push(struct dicey_queue *queue, void *val, enum dicey_locking_policy policy);
size_t dicey_queue_size(const struct dicey_queue *queue);

#endif // NYSSJKURJT_QUEUE_H
