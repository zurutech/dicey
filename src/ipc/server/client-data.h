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

#if !defined(NFXODQMLCB_CLIENT_DATA_H)
#define NFXODQMLCB_CLIENT_DATA_H

#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/core/type.h>
#include <dicey/core/version.h>
#include <dicey/ipc/server.h>

#include "ipc/chunk.h"

#include "pending-reqs.h"

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

    struct dicey_pending_requests *pending;

    struct dicey_hashset *subscriptions;
};

void dicey_client_data_delete(struct dicey_client_data *client);
struct dicey_client_data *dicey_client_data_new(struct dicey_server *parent, size_t id);

bool dicey_client_data_is_subscribed(const struct dicey_client_data *client, const char *elemdescr);
enum dicey_error dicey_client_data_subscribe(struct dicey_client_data *client, const char *elemdescr);
bool dicey_client_data_unsubscribe(struct dicey_client_data *client, const char *elemdescr);

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
