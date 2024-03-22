// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(NFXODQMLCB_CLIENT_DATA_H)
#define NFXODQMLCB_CLIENT_DATA_H

#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include <dicey/core/version.h>
#include <dicey/ipc/server.h>

#include "ipc/chunk.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

enum dicey_client_state {
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_RUNNING,
    CLIENT_STATE_DEAD,
};

struct dicey_client_data {
    uv_pipe_t pipe;

    enum dicey_client_state state;

    uint32_t seq_cnt;

    struct dicey_client_info info;
    struct dicey_version version;

    struct dicey_chunk *chunk;

    struct dicey_server *parent;
};

void dicey_client_data_delete(struct dicey_client_data *client);
struct dicey_client_data *dicey_client_data_new(struct dicey_server *parent, size_t id);
uint32_t dicey_client_data_next_seq(struct dicey_client_data *client);

struct dicey_client_list;

struct dicey_client_data *const *dicey_client_list_begin(const struct dicey_client_list *list);
struct dicey_client_data *dicey_client_list_drop_client(struct dicey_client_list *list, size_t id);
struct dicey_client_data *const *dicey_client_list_end(const struct dicey_client_list *list);
struct dicey_client_data *dicey_client_list_get_client(const struct dicey_client_list *list, size_t id);
bool dicey_client_list_is_empty(const struct dicey_client_list *list);
struct dicey_client_data **dicey_client_list_new_bucket(
    struct dicey_client_list **list,
    struct dicey_client_data ***bucket_dest,
    size_t *id
);

#endif // NFXODQMLCB_CLIENT_DATA_H
