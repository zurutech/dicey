// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ORNYMSAZGZ_PENDING_LIST_H)
#define ORNYMSAZGZ_PENDING_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/client.h>

struct dicey_pending_reply {
    uint32_t seq;

    uv_timespec64_t expires_at;

    dicey_client_on_reply_fn *callback;
    void *data;
};

struct dicey_pending_list {
    size_t len, cap;
    struct dicey_pending_reply waiting[];
};

bool dicey_pending_list_append(struct dicey_pending_list **list_ptr, struct dicey_pending_reply *reply);
void dicey_pending_list_erase(struct dicey_pending_list *list, size_t entry);
void dicey_pending_list_prune(struct dicey_pending_list *list, struct dicey_client *client);
bool dicey_pending_list_search_and_remove(
    struct dicey_pending_list *list,
    uint32_t seq,
    struct dicey_pending_reply *found
);

#endif // ORNYMSAZGZ_PENDING_LIST_H
