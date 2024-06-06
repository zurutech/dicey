// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

void dicey_client_data_delete(struct dicey_client_data *const client) {
    if (!client) {
        return;
    }

    free(client->chunk);
    free(client->pending);
    free(client);
}

struct dicey_client_data *dicey_client_data_new(struct dicey_server *const parent, const size_t id) {
    struct dicey_hashset *const subscriptions = dicey_hashset_new();
    if (!subscriptions) {
        return NULL;
    }

    struct dicey_client_data *new_client = calloc(1U, sizeof *new_client);
    if (!new_client) {
        dicey_hashset_delete(subscriptions);

        return NULL;
    }

    *new_client = (struct dicey_client_data) {
        .seq_cnt = 1U, // server-initiated packets are odd

        .info = {
            .id = id,
        },

        .parent = parent,
        .subscriptions = subscriptions,
    };

    return new_client;
}

uint32_t dicey_client_data_next_seq(struct dicey_client_data *const client) {
    const uint32_t next = client->seq_cnt;

    client->seq_cnt += 2U;

    if (client->seq_cnt < next) {
        abort(); // TODO: handle overflow
    }

    return next;
}

bool dicey_client_data_is_subscribed(const struct dicey_client_data *client, const char *elemdescr) {
    return dicey_hashset_contains(client->subscriptions, elemdescr);
}

enum dicey_error dicey_client_data_subscribe(struct dicey_client_data *const client, const char *const elemdescr) {
    assert(client && elemdescr);

    switch (dicey_hashset_add(&client->subscriptions, elemdescr)) {
    case DICEY_HASH_SET_FAILED:
        // assume that all hashset failures are due to OOM
        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_ADDED:
        return DICEY_OK;

    case DICEY_HASH_SET_UPDATED:
        return DICEY_EEXIST; // do not trace EEXIST - it is a possible condition

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

struct dicey_client_data **dicey_client_list_new_bucket(
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

                return *bucket_dest;
            }
        }

        old_cap = list->cap;
    }

    // if there's no list, or no empty slot, grow the list and return the first new slot
    *list_ptr = client_list_grow(*list_ptr);
    if (!*list_ptr) {
        return NULL;
    }

    *id = old_cap;

    return *bucket_dest = &(*list_ptr)->clients[old_cap];
}
