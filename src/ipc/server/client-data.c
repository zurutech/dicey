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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/unsafe.h"

#include "client-data.h"

#define BASE_CAP 128

struct dicey_client_list {
    size_t cap;
    struct dicey_client_data *clients[];
};

static struct dicey_client_list *client_list_grow(struct dicey_client_list *list) {
    const size_t old_cap = list ? list->cap : 0U;
    const size_t new_cap = old_cap ? old_cap * 3 / 2 : BASE_CAP;

    if (new_cap > (size_t) PTRDIFF_MAX) {
        return NULL;
    }

    list = realloc(list, sizeof *list + sizeof(*list->clients) * new_cap);
    if (!list) {
        return NULL;
    }

    ZERO_ARRAY(list->clients + old_cap, new_cap - old_cap);

    list->cap = new_cap;

    return list;
}

static enum dicey_error finish_client_data_cleanup(struct dicey_client_data *client) {
    if (client) {
        dicey_hashset_delete(client->subscriptions);

        free(client->chunk);
        free(client->pending);
        free(client);
    }

    return DICEY_OK;
}

enum dicey_error dicey_client_data_cleanup(struct dicey_client_data *const client) {
    if (!client) {
        return DICEY_OK; // nothing to do means nothing to fail
    }

    // if there's a cleanup callback, call it. Otherwise, call the cleanup function directly
    return client->cleanup_cb ? client->cleanup_cb(client, &finish_client_data_cleanup)
                              : finish_client_data_cleanup(client);
}

struct dicey_client_data *dicey_client_data_init(
    struct dicey_client_data *const client,
    struct dicey_server *const parent,
    const size_t id
) {
    assert(client && parent);

    *client = (struct dicey_client_data) {
        .info = {
            .id = id,
        },

        .parent = parent,
    };

    return client;
}

enum dicey_client_data_state dicey_client_data_get_state(const struct dicey_client_data *const client) {
    assert(client);

    return client->state;
}

struct dicey_client_data *dicey_client_data_new(struct dicey_server *const parent, const size_t id) {
    // ok, we should malloc + *client = (struct dicey_client_data) { 0 } here instead, but really, where will this
    // not work?
    struct dicey_client_data *new_client = calloc(1U, sizeof *new_client);
    if (!new_client) {
        return NULL;
    }

    if (!dicey_client_data_init(new_client, parent, id)) {
        free(new_client);

        return NULL;
    }

    return new_client;
}

bool dicey_client_data_is_subscribed(const struct dicey_client_data *const client, const char *const elemdescr) {
    return dicey_hashset_contains(client->subscriptions, elemdescr);
}

void dicey_client_data_set_state(struct dicey_client_data *const client, const enum dicey_client_data_state state) {
    assert(client && client->state != CLIENT_DATA_STATE_DEAD);

    client->state = state;
}

enum dicey_error dicey_client_data_subscribe(struct dicey_client_data *const client, const char *const elemdescr) {
    assert(client && elemdescr);

    switch (dicey_hashset_add(&client->subscriptions, elemdescr)) {
    case DICEY_HASH_SET_FAILED:
        // assume that all hashset failures are due to OOM
        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_ADDED:
    case DICEY_HASH_SET_UPDATED: // we're lenient here, we don't care if the client is already subscribed
        return DICEY_OK;

    default:
        abort(); // unreachable
    }
}

bool dicey_client_data_unsubscribe(struct dicey_client_data *const client, const char *const elemdescr) {
    assert(client && elemdescr);

    return dicey_hashset_remove(client->subscriptions, elemdescr);
}

struct dicey_client_data *const *dicey_client_list_begin(const struct dicey_client_list *const list) {
    return list ? list->clients : NULL;
}

struct dicey_client_data *dicey_client_list_drop_client(struct dicey_client_list *const list, const size_t id) {
    if (id < list->cap) {
        struct dicey_client_data *const ret = list->clients[id];

        list->clients[id] = NULL;

        return ret;
    }

    return NULL;
}

struct dicey_client_data *const *dicey_client_list_end(const struct dicey_client_list *const list) {
    return list ? list->clients + list->cap : NULL;
}

bool dicey_client_list_is_empty(const struct dicey_client_list *const list) {
    if (list) {
        struct dicey_client_data *const *const end = dicey_client_list_end(list);

        for (struct dicey_client_data *const *it = dicey_client_list_begin(list); it != end; ++it) {
            if (*it) {
                return false;
            }
        }
    }

    return true;
}

struct dicey_client_data *dicey_client_list_get_client(const struct dicey_client_list *const list, const size_t id) {
    if (list && id < list->cap) {
        return list->clients[id];
    }

    return NULL;
}

// in short: this function is used to find an empty slot in the client list
// `bucket_dest` is a pointer to the slot, and `id` is the index of the slot
// given that the list is a list of pointers, the function should ideally return a pointer to a slot the caller can then
// fill. This is a "bucket" and has type `struct dicey_client_data **`.
// Given that the function also returns the index as an output value, the function does not return the pointer directly,
// but rather through a pointer-to-pointer. This unfortunately ends up the type of `bucket_dest` as `struct
// dicey_client_data ***`, a triple pointer, which is a cumbersome way of saying "a pointer to a variable that can hold
// a pointer to a slot in a list of pointers".
//
//                                ── ───┬──────┬── ──
//                                      │      │
//                                      │      │
//                                      │      │
//                                ──── ─┴──────┴── ───
//                                     └────────┘
//                                         ▲
//                                         │
//                                         │
//                                         │
//                                         │
//    ┌─────────────┬──────────────────────┘
//    │             │
//    │ bucket_dest │
//    │             │
//    └─────────────┘
//
bool dicey_client_list_new_bucket(
    struct dicey_client_list **const list_ptr,
    struct dicey_client_data ***const bucket_dest,
    size_t *const id
) {
    assert(list_ptr && bucket_dest && id);

    size_t old_cap = 0U;

    // if there's a list, search for an empty slot
    if (*list_ptr) {
        struct dicey_client_list *list = *list_ptr;

        for (size_t i = 0; i < list->cap; ++i) {
            if (!list->clients[i]) {
                *id = i;
                *bucket_dest = &list->clients[i];

                return true;
            }
        }

        old_cap = list->cap;
    }

    // if there's no list, or no empty slot, grow the list and return the first new slot
    *list_ptr = client_list_grow(*list_ptr);
    if (!*list_ptr) {
        return false;
    }

    *id = old_cap;
    *bucket_dest = &(*list_ptr)->clients[old_cap];

    return true;
}
