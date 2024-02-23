// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

void dicey_queue_deinit(struct dicey_queue *queue, void (*free_data)(void *data));
enum dicey_error dicey_queue_init(struct dicey_queue *queue);
bool dicey_queue_pop(struct dicey_queue *queue, void **val, enum dicey_locking_policy policy);
bool dicey_queue_push(struct dicey_queue *queue, void *val, enum dicey_locking_policy policy);
size_t dicey_queue_size(const struct dicey_queue *queue);

#endif // NYSSJKURJT_QUEUE_H
