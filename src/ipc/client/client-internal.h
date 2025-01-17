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

#if !defined(HWMIYVYDED_CLIENT_INTERNAL_H)
#define HWMIYVYDED_CLIENT_INTERNAL_H

#include <stdint.h>

#include <uv.h>

#include <dicey/ipc/client.h>

#include "ipc/chunk.h"
#include "ipc/client/waiting-list.h"
#include "ipc/tasks/loop.h"

#define CLIENT_DEFAULT_TIMEOUT ((int32_t) 1000U)

enum dicey_client_state {
    CLIENT_STATE_UNINIT,
    CLIENT_STATE_INIT,
    CLIENT_STATE_CONNECT_START,
    CLIENT_STATE_RUNNING,

    CLIENT_STATE_DEAD,

    CLIENT_STATE_CLOSING,
    CLIENT_STATE_CLOSED,
};

enum dicey_client_setup_type {
    CLIENT_CONNECT_ADDR,
    CLIENT_OPEN_FD,
};

struct dicey_client_setup_info {
    enum dicey_client_setup_type type;

    union {
        struct dicey_addr addr;
        uv_file fd;
    } data;
};

struct dicey_client {
    uv_pipe_t pipe;

    _Atomic enum dicey_client_state state;

    struct dicey_task_loop *tloop;

    dicey_client_inspect_fn *inspect_func;
    dicey_client_signal_fn *on_signal;

    struct dicey_waiting_list *waiting_tasks;
    struct dicey_chunk *recv_chunk;

    uint32_t next_seq;

    void *ctx;
};

void dicey_client_deinit(struct dicey_client *client);
enum dicey_error dicey_client_init(struct dicey_client *client, const struct dicey_client_args *args);
enum dicey_error dicey_client_open_fd(struct dicey_client *client, uv_file addr);

#endif // HWMIYVYDED_CLIENT_INTERNAL_H
