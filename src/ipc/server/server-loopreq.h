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

#if !defined(LLUCQCORBC_SERVER_LOOPREQ_H)
#define LLUCQCORBC_SERVER_LOOPREQ_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/views.h>
#include <dicey/ipc/server.h>

#include "sup/view-ops.h"

#include "client-data.h"

// - server may be null. If this is the case, it means the request has been cancelled and the callback should only
//   clean up the payload in request data
// - client may be null only if target was < 0
// - req_data is the payload data passed to the request. The memory of this data is managed by the loop or the caller,
//   but the callback (or the caller) is responsible for freeing any resources the callback stored in the payload.
typedef enum dicey_error dicey_server_loop_request_fn(
    struct dicey_server *server,
    struct dicey_client_data *client,
    void *req_data
);

// a request to be processed in the server loop.
// the callback will be called on the loop context, and will receive the server, the client (if target is >= 0), and
// the payload data.
// If the caller is interested in performing a synchronous operation, it should provide a semaphore to be posted when
// the operation is done. In this case, the loop will not free the request data, and the caller is responsible for
// its cleanup.
// if the request is aborted and no semaphore is set, the callback will be called with a null server, and the caller
// should only clean up the contents of the payload; the loop will free the request data object itself. Otherwise,
// the loop will post on the semaphore and the caller is responsible for freeing the request data.
struct dicey_server_loop_request {
    dicey_server_loop_request_fn *cb;

    ptrdiff_t target;

    uv_sem_t *sem;
    enum dicey_error err;

    char payload[];
};

// these macros are designed to make pretty unsafe things safe(r). Use with caution.

#define DICEY_SERVER_LOOP_REQ_NO_TARGET ((ptrdiff_t) -1)
#define DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(N) calloc(1, sizeof(struct dicey_server_loop_request) + (N))
#define DICEY_SERVER_LOOP_REQ_NEW(TYPE) DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(sizeof(TYPE))
#define DICEY_SERVER_LOOP_REQ_NEW_EMPTY() DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(0)
#define DICEY_SERVER_LOOP_REQ_GET_PAYLOAD(DEST, REQ, TYPE) ((void) memcpy((DEST), (REQ).payload, sizeof(TYPE)))
#define DICEY_SERVER_LOOP_REQ_GET_PAYLOAD_AS_VIEW_MUT(REQ, SIZE) dicey_view_mut_from((REQ).payload, (SIZE))

#define DICEY_SERVER_LOOP_SET_PAYLOAD_BYTES(DEST, PAYLOADPTR, N) ((void) memcpy((DEST)->payload, (PAYLOADPTR), (N)))

#define DICEY_SERVER_LOOP_SET_PAYLOAD(DEST, TYPE, PAYLOADPTR)                                                          \
    DICEY_SERVER_LOOP_SET_PAYLOAD_BYTES(DEST, PAYLOADPTR, sizeof(TYPE))

// req is always malloc'd, and will be freed by these functions
enum dicey_error dicey_server_submit_request(struct dicey_server *server, struct dicey_server_loop_request *req);
enum dicey_error dicey_server_blocking_request(struct dicey_server *server, struct dicey_server_loop_request *req);

#endif // LLUCQCORBC_SERVER_LOOPREQ_H
