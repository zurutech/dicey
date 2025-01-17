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

#if !defined(NLOSCKAFJH_SERVER_CLIENTS_H)
#define NLOSCKAFJH_SERVER_CLIENTS_H

#include <stddef.h>

#include <dicey/core/errors.h>

#include "client-data.h"

// shared logic for adding and dropping a client to the server between plugins and external clients

enum dicey_error dicey_server_cleanup_id(struct dicey_server *server, size_t id);
struct dicey_client_data *dicey_server_release_id(struct dicey_server *server, size_t id);
enum dicey_error dicey_server_remove_client(struct dicey_server *server, size_t id);
enum dicey_error dicey_server_reserve_id(
    struct dicey_server *server,
    struct dicey_client_data ***bucket_ptr,
    size_t *id
);

#endif // NLOSCKAFJH_SERVER_CLIENTS_H
