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

#if !defined(OCMWWAUVGQ_TASK_IO_H)
#define OCMWWAUVGQ_TASK_IO_H

#include <stdint.h>

#include <uv.h>

#include <dicey/ipc/address.h>

#include "loop.h"

struct dicey_task_error *dicey_task_op_close(struct dicey_task_loop *tloop, int64_t id, uv_handle_t *handle);

struct dicey_task_error *dicey_task_op_connect_pipe(
    struct dicey_task_loop *tloop,
    int64_t id,
    uv_pipe_t *pipe,
    struct dicey_addr addr
);

struct dicey_task_error *dicey_task_op_write(
    struct dicey_task_loop *tloop,
    int64_t id,
    uv_stream_t *stream,
    uv_buf_t buf
);

struct dicey_task_error *dicey_task_op_write_and_wait(
    struct dicey_task_loop *tloop,
    int64_t id,
    uv_stream_t *stream,
    uv_buf_t buf
);

struct dicey_task_error *dicey_task_op_open_pipe(
    struct dicey_task_loop *tloop,
    int64_t id,
    uv_pipe_t *pipe,
    uv_file fd
);

#endif // OCMWWAUVGQ_TASK_IO_H
