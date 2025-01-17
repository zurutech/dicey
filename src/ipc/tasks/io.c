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
#include <stdint.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/address.h>

#include "sup/uvtools.h"

#include "io.h"
#include "loop.h"

enum task_lock_policy {
    TASK_UNLOCK_AFTER_OP,
    TASK_LOCK_INDEFINITELY,
};

struct task_cookie {
    struct dicey_task_loop *tloop;
    int64_t task_id;
};

struct connect_op {
    uv_connect_t conn;

    struct task_cookie cookie;
};

struct write_op {
    uv_write_t write;

    struct task_cookie cookie;
    enum task_lock_policy lock_policy;
};

static void unlock_task(const struct task_cookie tinfo, const int status) {
    struct dicey_task_loop *const tloop = tinfo.tloop;
    assert(tloop);

    const int64_t task_id = tinfo.task_id;

    if (status < 0) {
        dicey_task_loop_fail(tloop, task_id, dicey_error_from_uv(status), "connect failed: %s", uv_strerror(status));
    } else {
        dicey_task_loop_advance(tloop, task_id, NULL);
    }
}

static void on_close(uv_handle_t *const handle) {
    struct task_cookie *const context = handle->data;
    assert(context);

    unlock_task(*context, 0);

    free(context);
}

static void on_connect(uv_connect_t *const conn, const int status) {
    struct connect_op *const context = (struct connect_op *) conn;
    assert(context);

    unlock_task(context->cookie, status);

    free(conn);
}

static void on_write(uv_write_t *const write, const int status) {
    struct write_op *const context = (struct write_op *) write;
    assert(context);

    if (context->lock_policy == TASK_UNLOCK_AFTER_OP) {
        unlock_task(context->cookie, status);
    }

    free(write);
}

struct dicey_task_error *perform_write(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    uv_stream_t *const stream,
    uv_buf_t buf,
    const enum task_lock_policy lock_policy
) {
    assert(tloop && stream && buf.base && buf.len);

    struct write_op *const write = malloc(sizeof(*write));
    if (!write) {
        // this will almost certainly fail too, but we can't do anything about it
        return dicey_task_error_new(DICEY_ENOMEM, "failed to allocate write operation");
    }

    *write = (struct write_op) {
        .cookie = {tloop, id},
        .lock_policy = lock_policy,
    };

    const int uverr = uv_write((uv_write_t *) write, stream, &buf, 1, &on_write);
    if (uverr < 0) {
        free(write);

        return dicey_task_error_new(dicey_error_from_uv(uverr), "failed to issue write: %s", uv_strerror(uverr));
    }

    return NULL;
}

struct dicey_task_error *dicey_task_op_close(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    uv_handle_t *const handle
) {
    assert(tloop && handle && !handle->data);

    struct task_cookie *const tcookie = malloc(sizeof *tcookie);
    if (!tcookie) {
        // this will almost certainly fail too, but we can't do anything about it
        return dicey_task_error_new(DICEY_ENOMEM, "failed to allocate close operation");
    }

    *tcookie = (struct task_cookie) { tloop, id };

    handle->data = tcookie;

    uv_close(handle, &on_close);

    return NULL;
}

struct dicey_task_error *dicey_task_op_connect_pipe(
    struct dicey_task_loop *const tloop,
    int64_t id,
    uv_pipe_t *const pipe,
    const struct dicey_addr addr
) {
    assert(tloop && pipe && addr.addr && addr.len);

    // assume the pipe is uninitialized. Allocate it using safe defaults
    uv_loop_t *const loop = dicey_task_loop_get_uv_handle(tloop);
    assert(loop);

    int uverr = uv_pipe_init(loop, pipe, 0);
    if (uverr < 0) {
        return dicey_task_error_new(dicey_error_from_uv(uverr), "failed to initialize pipe: %s", uv_strerror(uverr));
    }

    struct connect_op *const conn = malloc(sizeof(*conn));
    if (!conn) {
        // this will almost certainly fail too, but we can't do anything about it
        return dicey_task_error_new(DICEY_ENOMEM, "failed to allocate connect operation");
    }

    *conn = (struct connect_op) {
        .cookie = {tloop, id},
    };

    uverr = uv_pipe_connect2((uv_connect_t *) conn, pipe, addr.addr, addr.len, 0, &on_connect);
    if (uverr < 0) {
        free(conn);

        return dicey_task_error_new(dicey_error_from_uv(uverr), "failed to issue connect: %s", uv_strerror(uverr));
    }

    return NULL;
}

struct dicey_task_error *dicey_task_op_write(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    uv_stream_t *const stream,
    uv_buf_t buf
) {
    return perform_write(tloop, id, stream, buf, TASK_UNLOCK_AFTER_OP);
}

struct dicey_task_error *dicey_task_op_write_and_wait(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    uv_stream_t *const stream,
    uv_buf_t buf
) {
    return perform_write(tloop, id, stream, buf, TASK_LOCK_INDEFINITELY);
}

struct dicey_task_error *dicey_task_op_open_pipe(
    struct dicey_task_loop *const tloop,
    int64_t id,
    uv_pipe_t *const pipe,
    const uv_file fd
) {
    assert(tloop && pipe && fd >= 0);
    DICEY_UNUSED(id);

    // assume the pipe is uninitialized. Allocate it using safe defaults
    uv_loop_t *const loop = dicey_task_loop_get_uv_handle(tloop);
    assert(loop);

    int uverr = uv_pipe_init(loop, pipe, 0);
    if (uverr < 0) {
        return dicey_task_error_new(dicey_error_from_uv(uverr), "failed to initialize pipe: %s", uv_strerror(uverr));
    }

    uverr = uv_pipe_open(pipe, fd);
    if (uverr < 0) {
        return dicey_task_error_new(dicey_error_from_uv(uverr), "failed to open pipe: %s", uv_strerror(uverr));
    }

    return NULL;
}
