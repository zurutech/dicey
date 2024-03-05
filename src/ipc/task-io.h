// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(OCMWWAUVGQ_TASK_IO_H)
#define OCMWWAUVGQ_TASK_IO_H

#include <stdint.h>

#include <uv.h>

#include <dicey/ipc/address.h>

#include "task-loop.h"

struct dicey_task_error *dicey_task_op_close(struct dicey_task_loop *tloop, int64_t id, uv_handle_t *handle);

struct dicey_task_error *dicey_task_op_connect_pipe(
    struct dicey_task_loop *tloop,
    int64_t id,
    uv_pipe_t *pipe,
    const struct dicey_addr addr
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

#endif // OCMWWAUVGQ_TASK_IO_H
