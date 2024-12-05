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

#if !defined(JUYPLEPMAY_SERVER_H)
#define JUYPLEPMAY_SERVER_H

#include <stdint.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/views.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/server.h>

#include "ipc/queue.h"

#include "client-data.h"

#include "dicey_config.h"

enum dicey_server_state {
    SERVER_STATE_UNINIT,
    SERVER_STATE_INIT,
    SERVER_STATE_RUNNING,
    SERVER_STATE_QUITTING,
};

struct dicey_server {
    // first member is the uv_pipe_t to allow for type punning
    uv_pipe_t pipe;

    _Atomic enum dicey_server_state state;

    uint32_t seq_cnt; // used for ALL server-initiated packets. Starts with 1, and will roll over after UINT32_MAX.

    // super ugly way to unlock the callers of dicey_server_stop when the server is actually stopped
    // avoiding this would probably require to use the same task system the client uses - which is way more complex than
    // whatever the server needs
    uv_sem_t *shutdown_hook;

    uv_loop_t loop;
    uv_async_t async;

    struct dicey_queue queue;

    dicey_server_on_connect_fn *on_connect;
    dicey_server_on_disconnect_fn *on_disconnect;
    dicey_server_on_error_fn *on_error;
    dicey_server_on_request_fn *on_request;

    struct dicey_client_list *clients;
    struct dicey_registry registry;

    struct dicey_view_mut scratchpad;

#if DICEY_HAS_PLUGINS
    dicey_server_on_plugin_event_fn *on_plugin_event;
#endif

    void *ctx;
};

#endif // JUYPLEPMAY_SERVER_H
