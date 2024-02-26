// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/ipc/server.h>

#include "client-data.h"

#define BASE_CAP 128

#if defined(_WIN32) || defined(__unix__) || defined(__unix) || defined(unix) ||                                        \
    (defined(__APPLE__) && defined(__MACH__))
#define ZERO_PTRLIST(TYPE, BASE, LEN) memset((BASE), 0, sizeof *(BASE) * (LEN))
#else
#define ZERO_PTRLIST(TYPE, BASE, END)                                                                                  \
    for (TYPE *it = (BASE); it != (END); ++it) {                                                                       \
        *it = NULL;                                                                                                    \
    }
#endif

struct dicey_client_list {
    size_t cap;
    struct dicey_client_data *clients[];
};

static struct dicey_client_list *client_list_grow(struct dicey_client_list *list) {
    const size_t new_cap = list && list->cap ? list->cap * 3 / 2 : BASE_CAP;

    if (new_cap > (size_t) PTRDIFF_MAX) {
        return NULL;
    }

    const size_t old_cap = list->cap;

    list = realloc(list, sizeof *list + sizeof(*list->clients) * new_cap);
    if (!list) {
        return NULL;
    }

    ZERO_PTRLIST(struct dicey_client_state, list->clients + old_cap, new_cap - old_cap);

    list->cap = new_cap;

    return list;
}

void dicey_client_data_delete(struct dicey_client_data *const client) {
    if (!client) {
        return;
    }

    free(client->chunk);
    free(client);
}

struct dicey_client_data *dicey_client_data_new(struct dicey_server *const parent, const size_t id) {
    struct dicey_client_data *new_client = calloc(1U, sizeof *new_client);
    if (!new_client) {
        return NULL;
    }

    *new_client = (struct dicey_client_data) {
        .seq_cnt = 1U, // server-initiated packets are odd

        .info = {
            .id = id,
        },

        .parent = parent,
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

    struct dicey_client_list *list = *list_ptr;

    if (list) {
        for (size_t i = 0; i < list->cap; ++i) {
            if (!list->clients[i]) {
                *id = i;
                *bucket_dest = &list->clients[i];

                return *bucket_dest;
            }
        }
    }

    *list_ptr = client_list_grow(list);
    if (!list) {
        return NULL;
    }

    return dicey_client_list_new_bucket(list_ptr, bucket_dest, id);
}
