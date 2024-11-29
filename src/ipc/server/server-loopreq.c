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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/errors.h>

#include "sup/uvtools.h"

#include "ipc/queue.h"

#include "server-loopreq.h"
#include "server.h"

enum dicey_error dicey_server_submit_request(
    struct dicey_server *const server,
    struct dicey_server_loop_request *const req
) {
    assert(server && req);

    const bool success = dicey_queue_push(&server->queue, req, DICEY_LOCKING_POLICY_BLOCKING);

    assert(success);
    (void) success; // suppress unused variable warning with NDEBUG and MSVC

    return dicey_error_from_uv(uv_async_send(&server->async));
}

enum dicey_error dicey_server_blocking_request(
    struct dicey_server *const server,
    struct dicey_server_loop_request *const req // must be malloc'd, and will be freed by this function
) {
    assert(server && req);

    uv_sem_t sem = { 0 };
    uv_sem_init(&sem, 0);

    req->sem = &sem;

    enum dicey_error err = dicey_server_submit_request(server, req);

    // there is no way async send can fail, honestly, and it if does, there is no possible way to recover
    assert(!err);
    (void) err; // suppress unused variable warning with NDEBUG and MSVC

    uv_sem_wait(&sem);

    uv_sem_destroy(&sem);

    err = req->err;
    free(req);

    return err;
}
