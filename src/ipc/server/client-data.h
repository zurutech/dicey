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

#if !defined(NFXODQMLCB_CLIENT_DATA_H)
#define NFXODQMLCB_CLIENT_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/core/type.h>
#include <dicey/core/version.h>
#include <dicey/ipc/server-api.h>
#include <dicey/ipc/server.h>

#include "ipc/chunk.h"

#include "pending-reqs.h"

#include "dicey_config.h"

#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4200)
#endif

enum dicey_client_data_state {
    CLIENT_DATA_STATE_CONNECTED,
    CLIENT_DATA_STATE_RUNNING,
    CLIENT_DATA_STATE_DEAD,
};

struct dicey_client_data;

// cleanup callback that a data cleanup is bound to call after the cleanup is done
typedef enum dicey_error dicey_client_data_after_cleanup_fn(struct dicey_client_data *client);

// cleanup callback used by plugins to clean up their data
typedef enum dicey_error dicey_client_data_cleanup_fn(
    struct dicey_client_data *client,
    dicey_client_data_after_cleanup_fn *after_cleanup
);

struct dicey_client_data {
    uv_pipe_t pipe;

    enum dicey_client_data_state state;

    uint32_t seq_cnt;

    struct dicey_client_info info;

    struct dicey_chunk *chunk;

    struct dicey_server *parent;

    struct dicey_pending_requests *pending;

    struct dicey_hashset *subscriptions;

    dicey_client_data_cleanup_fn *cleanup_cb;
};

enum dicey_error dicey_client_data_cleanup(struct dicey_client_data *client);
struct dicey_client_data *dicey_client_data_init(
    struct dicey_client_data *client,
    struct dicey_server *parent,
    size_t id
);
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

bool dicey_client_list_new_bucket(struct dicey_client_list **list, struct dicey_client_data ***bucket_dest, size_t *id);

#endif // NFXODQMLCB_CLIENT_DATA_H
