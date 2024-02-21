// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>

#include "queue.h"

void dicey_queue_deinit(struct dicey_queue *const queue, void (*const free_data)(void *data)) {
    uv_mutex_destroy(&queue->mutex);
    uv_cond_destroy(&queue->cond);

    if (free_data) {
        for (ptrdiff_t i = queue->head; i != queue->tail; i = (i + 1) % REQUEST_QUEUE_CAP) {
            free_data(queue->data[i]);
        }
    }
}

enum dicey_error dicey_queue_init(struct dicey_queue *const queue) {
    *queue = (struct dicey_queue) { 0 };

    int error = uv_mutex_init(&queue->mutex);
    if (error < 0) {
        return error;
    }

    error = uv_cond_init(&queue->cond);
    if (error < 0) {
        uv_mutex_destroy(&queue->mutex);

        return error;
    }

    return 0;
}

bool dicey_queue_pop(struct dicey_queue *const queue, void **const val, const enum dicey_locking_policy policy) {
    uv_mutex_lock(&queue->mutex);

    while (queue->head == queue->tail) {
        if (policy == DICEY_LOCKING_POLICY_NONBLOCKING) {
            uv_mutex_unlock(&queue->mutex);

            return false;
        }

        uv_cond_wait(&queue->cond, &queue->mutex);
    }

    *val = queue->data[queue->head];
    queue->data[queue->head] = NULL;

    queue->head = (queue->head + 1) % REQUEST_QUEUE_CAP;

    uv_cond_signal(&queue->cond);
    uv_mutex_unlock(&queue->mutex);

    return true;
}

bool dicey_queue_pop_packet(
    struct dicey_queue *const queue,
    struct dicey_packet *const dest,
    const enum dicey_locking_policy policy
) {
    assert(queue && dest);

    void *packet = NULL;
    if (!dicey_queue_pop(queue, &packet, policy)) {
        return false;
    }

    *dest = *(struct dicey_packet *) packet;

    free(packet);

    return true;
}

bool dicey_queue_push(struct dicey_queue *const queue, void *const req, const enum dicey_locking_policy policy) {
    uv_mutex_lock(&queue->mutex);

    const ptrdiff_t new_tail = (queue->tail + 1) % REQUEST_QUEUE_CAP;

    while (new_tail == queue->head) {
        if (policy == DICEY_LOCKING_POLICY_NONBLOCKING) {
            uv_mutex_unlock(&queue->mutex);

            return false;
        }

        uv_cond_wait(&queue->cond, &queue->mutex);
    }

    assert(!queue->data[queue->tail]);

    queue->data[queue->tail] = req;
    queue->tail = new_tail;

    uv_cond_signal(&queue->cond);
    uv_mutex_unlock(&queue->mutex);

    return true;
}

bool dicey_queue_push_packet(
    struct dicey_queue *const queue,
    const struct dicey_packet packet,
    const enum dicey_locking_policy policy
) {
    assert(queue && dicey_packet_is_valid(packet));

    struct dicey_packet *const req = malloc(sizeof *req);
    if (!req) {
        return false;
    }

    *req = packet;

    return dicey_queue_push(queue, req, policy);
}

size_t dicey_queue_size(const struct dicey_queue *const queue) {
    return (size_t) ((queue->tail - queue->head + REQUEST_QUEUE_CAP) % REQUEST_QUEUE_CAP);
}
