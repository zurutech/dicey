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
#include <stddef.h>
#include <stdlib.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "client-data.h"
#include "server-clients.h"
#include "server-internal.h"

static void on_client_end(uv_handle_t *const handle) {
    struct dicey_client_data *const client = (struct dicey_client_data *) handle;

    if (client->parent->on_disconnect) {
        client->parent->on_disconnect(client->parent, &client->info);
    }

    // either ignore or assert on cleanup errors - there's nothing we can do here anyway
    const enum dicey_error err = dicey_client_data_cleanup(client);
    DICEY_UNUSED(err);
    assert(!err);
}

enum dicey_error dicey_server_cleanup_id(struct dicey_server *const server, const size_t id) {
    if (!server) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_client_data *const bucket = dicey_server_release_id(server, id);

    if (!bucket) {
        return DICEY_OK; // nothing to do
    }

    return dicey_client_data_cleanup(bucket);
}

struct dicey_client_data *dicey_server_release_id(struct dicey_server *const server, const size_t id) {
    return dicey_client_list_drop_client(server->clients, id);
}

enum dicey_error dicey_server_remove_client(struct dicey_server *const server, const size_t index) {
    if (!server) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_client_data *const bucket = dicey_server_release_id(server, index);

    if (!bucket) {
        return TRACE(DICEY_EINVAL);
    }

    uv_close((uv_handle_t *) bucket, &on_client_end);

    return DICEY_OK;
}

enum dicey_error dicey_server_reserve_id(
    struct dicey_server *server,
    struct dicey_client_data ***bucket_ptr,
    size_t *id
) {
    assert(server && bucket_ptr && id);

    if (!dicey_client_list_new_bucket(&server->clients, bucket_ptr, id)) {
        return TRACE(DICEY_ENOMEM);
    }

    assert(*bucket_ptr);

    return DICEY_OK;
}
