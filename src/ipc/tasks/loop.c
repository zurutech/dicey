// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/errors.h>

#include "ipc/queue.h"
#include "ipc/uvtools.h"

#include "list.h"
#include "loop.h"

#define TIMEOUT_CHECK_MS 10U

struct dicey_task_loop {
    _Atomic bool running;

    uv_thread_t thread;
    uv_async_t *jobs_async, *halt_async;
    uv_loop_t *loop;
    uv_timer_t *timer;

    struct dicey_queue queue;
    struct dicey_task_list *pending_tasks;

    dicey_task_loop_global_at_end *global_at_end;
    dicey_task_loop_global_stopped *global_stopped;

    void *_Atomic ctx;
};

struct close_ctx {
    struct dicey_task_loop *tloop;
    const ptrdiff_t *next_offsets;
};

struct thread_init_req {
    enum dicey_error err;

    struct dicey_task_loop *tloop;

    uv_sem_t *sem;
};

static void complete_task(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_task_request *const task,
    struct dicey_task_error *const err
) {
    assert(tloop && task && task->at_end);

    // first call the global inspect callback - this is useful to cleanup any global state, like maps based on the id
    if (tloop->global_at_end) {
        tloop->global_at_end(dicey_task_loop_get_context(tloop), id, err);
    }

    // then, call the per-task callback, which is required to cleanup any per-task state
    task->at_end(id, err, task->ctx);

    free(task);

    const bool success = dicey_task_list_erase(tloop->pending_tasks, id);
    (void) success;

    assert(success || id < 0); // if id is negative, it was never added to the list
}

static void fail_task(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_task_request *const task,
    struct dicey_task_error *const err
) {
    assert(tloop && task && err);

    task->work = NULL;
    complete_task(tloop, id, task, err);

    free(err);
}

static bool step_task(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_task_request *const task,
    void *const input
) {
    assert(tloop && task && task->work && *task->work);

    struct dicey_task_result result = (*task->work)(tloop, id, task->ctx, input);

    switch (result.kind) {
    case DICEY_TASK_CONTINUE:
        assert(!result.error);

        ++task->work;

        if (!*task->work) {
            complete_task(tloop, id, task, NULL);

            return true;
        }

        return false;

    case DICEY_TASK_ERROR:
        fail_task(tloop, id, task, result.error);

        return true;

    case DICEY_TASK_RETRY:
        assert(!result.error);

        return true;

    default:
        abort();
    }
}

static bool start_task(struct dicey_task_loop *const tloop, int64_t id, struct dicey_task_request *const task) {
    assert(tloop && task && task->work && *task->work);

    return step_task(tloop, id, task, NULL);
}

static const ptrdiff_t handle_offsets[] = {
    offsetof(struct dicey_task_loop, jobs_async),
    offsetof(struct dicey_task_loop, halt_async),
    offsetof(struct dicey_task_loop, timer),
    -1,
};

static void on_close_handle(uv_handle_t *const handle) {
    assert(handle);

    struct close_ctx *const ctx = handle->data;
    assert(ctx);

    // when all

    const ptrdiff_t next_offset = *ctx->next_offsets++;

    if (next_offset >= 0) {
        uv_handle_t *const next_handle = *(uv_handle_t **) ((char *) ctx->tloop + next_offset);

        if (next_handle && !uv_is_closing(next_handle)) {
            next_handle->data = ctx;

            uv_close(next_handle, &on_close_handle);
        }
    } else {
        uv_stop(ctx->tloop->loop);

        free(ctx);
    }
}

static void close_handles(struct dicey_task_loop *const tloop) {
    assert(tloop);

    const ptrdiff_t *offsets = handle_offsets;

    while (*offsets >= 0) {
        uv_handle_t *const handle = *(uv_handle_t **) ((char *) tloop + *offsets);

        ++offsets;

        if (!handle || uv_is_closing(handle)) {
            continue;
        }

        struct close_ctx *ctx = malloc(sizeof *ctx);
        if (!ctx) {
            // accept the leak
            uv_stop(tloop->loop);

            return;
        }

        *ctx = (struct close_ctx) {
            .tloop = tloop,
            .next_offsets = offsets,
        };

        handle->data = ctx;

        uv_close(handle, &on_close_handle);

        // the handles will be closed in the callback
        return;
    }

    // no handles to close. stop the loop
    uv_stop(tloop->loop);
}

static void halt_loop(uv_async_t *const async) {
    assert(async);

    struct dicey_task_loop *const tloop = async->data;

    assert(tloop);

    close_handles(tloop);
}

static void process_queue(uv_async_t *const async) {
    assert(async);

    struct dicey_task_loop *const task_loop = async->data;
    assert(task_loop);

    void *req_ptr = NULL;
    while (dicey_queue_pop(&task_loop->queue, &req_ptr, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(req_ptr);

        struct dicey_task_request *const req = req_ptr;
        assert(req->work && *req->work);

        const int64_t id = dicey_task_list_append(&task_loop->pending_tasks, req, req->timeout_ms);
        if (id >= 0) {
            if (start_task(task_loop, id, req)) {
                dicey_task_list_erase(task_loop->pending_tasks, id);
            }
        } else {
            fail_task(task_loop, -1, req, dicey_task_error_new(DICEY_ENOMEM, "failed to add task to pending list"));
        }
    }
}

static void task_timed_out(void *const ctx, const int64_t id, void *const expired_item) {
    assert(ctx && expired_item);

    struct dicey_task_loop *const tloop = ctx;

    struct dicey_task_request *const task = expired_item;

    fail_task(tloop, id, task, dicey_task_error_new(DICEY_ETIMEDOUT, "task timed out"));
}

static void check_timeout(uv_timer_t *const timer) {
    struct dicey_task_loop *const tloop = timer->data;

    assert(tloop);

    dicey_task_list_prune(tloop->pending_tasks, &task_timed_out, tloop);
}

struct loop_checker {
    uv_idle_t idle;

    struct dicey_task_loop *tloop;
    uv_sem_t *sem;
};

static void notify_running(uv_idle_t *const idle) {
    assert(idle);

    struct loop_checker *const up_check = (struct loop_checker *) idle;
    assert(up_check->sem && up_check->tloop);

    up_check->tloop->running = true;

    uv_sem_post(up_check->sem);

    uv_close((uv_handle_t *) idle, NULL);
}

struct task_queue_free_ctx {
    struct dicey_task_loop *tloop;
    struct dicey_task_error *err;
};

static void free_incoming_task(void *const ctx, void *const data) {
    assert(ctx && data);

    struct task_queue_free_ctx *const free_ctx = ctx;
    assert(free_ctx->tloop);

    struct dicey_task_request *const req = data;

    complete_task(free_ctx->tloop, -1, req, free_ctx->err);
}

static void cancel_all_pending(struct dicey_task_loop *const tloop) {
    assert(tloop);

    struct task_queue_free_ctx free_ctx = {
        .tloop = tloop,
        .err = dicey_task_error_new(DICEY_ECANCELLED, "task loop cancelled"),
    };

    if (tloop->pending_tasks) {
        const struct dicey_task_entry *const rend = dicey_task_list_begin(tloop->pending_tasks) - 1;

        for (const struct dicey_task_entry *rit = dicey_task_list_end(tloop->pending_tasks) - 1; rit != rend; --rit) {
            struct dicey_task_request *const req = rit->data;

            complete_task(tloop, rit->id, req, free_ctx.err);
        }
    }

    free(tloop->pending_tasks);
    dicey_queue_deinit(&tloop->queue, &free_incoming_task, &free_ctx);

    free(free_ctx.err);
}

static enum dicey_error init_loop(
    struct dicey_task_loop *tloop,
    uv_async_t *const jobs_async,
    uv_async_t *const halt_async,
    uv_loop_t *const loop,
    uv_timer_t *const timer,
    struct loop_checker *const up_check,
    uv_sem_t *const unlock_sem
) {
    assert(tloop && jobs_async && halt_async && loop && timer && up_check && unlock_sem);

    enum dicey_error err = dicey_error_from_uv(uv_loop_init(loop));
    if (err) {
        goto deinit_loop;
    }

    err = dicey_error_from_uv(uv_async_init(loop, jobs_async, &process_queue));
    if (err) {
        goto deinit_jobs_async;
    }

    jobs_async->data = tloop;

    err = dicey_error_from_uv(uv_async_init(loop, halt_async, &halt_loop));
    if (err) {
        goto deinit_halt_async;
    }

    halt_async->data = tloop;

    err = dicey_error_from_uv(uv_timer_init(loop, timer));
    if (err) {
        goto deinit_timer;
    }

    timer->data = tloop;

    err = dicey_error_from_uv(uv_idle_init(loop, &up_check->idle));
    if (err) {
        goto deinit_idle;
    }

    up_check->tloop = tloop;
    up_check->sem = unlock_sem;

    tloop->loop = loop;
    tloop->jobs_async = jobs_async;
    tloop->halt_async = halt_async;
    tloop->timer = timer;

    err = dicey_queue_init(&tloop->queue);
    if (err) {
        goto deinit_idle;
    }

    tloop->pending_tasks = NULL;

    err = dicey_error_from_uv(uv_idle_start(&up_check->idle, &notify_running));
    if (err) {
        goto clear_all;
    }

    err = dicey_error_from_uv(uv_timer_start(timer, &check_timeout, TIMEOUT_CHECK_MS, TIMEOUT_CHECK_MS));
    if (err) {
        goto clear_all;
    }

    return DICEY_OK;

clear_all:
deinit_idle:
    uv_close((uv_handle_t *) &up_check->idle, NULL);

deinit_timer:
    uv_close((uv_handle_t *) timer, NULL);

deinit_halt_async:
    uv_close((uv_handle_t *) halt_async, NULL);

deinit_jobs_async:
    uv_close((uv_handle_t *) jobs_async, NULL);

deinit_loop:
    {
        const int uverr = uv_loop_close(loop);
        assert(uverr != UV_EBUSY);

        (void) uverr;
    }

    return err;
}

static void loop_thread(void *const arg) {
    assert(arg);

    uv_async_t jobs_async = { 0 }, halt_async = { 0 };
    uv_loop_t loop = { 0 };
    uv_timer_t timer = { 0 };
    struct loop_checker up_check = { 0 };
    struct thread_init_req *const req = arg;

    assert(req->sem && req->tloop);

    struct dicey_task_loop *tloop = req->tloop;

    req->err = init_loop(tloop, &jobs_async, &halt_async, &loop, &timer, &up_check, req->sem);
    if (req->err) {
        goto clear_all;
    }

    const enum dicey_error loop_err = dicey_error_from_uv(uv_run(&loop, UV_RUN_DEFAULT));
    if (loop_err && !tloop->running) {
        // if running is false, it means the loop never ran, so req is still valid and `start` is still waiting
        // for the semaphore
        req->err = loop_err;
        uv_sem_post(req->sem);
    }

clear_all:
    {
        const int uverr = uv_loop_close(&loop);
        assert(uverr != UV_EBUSY);

        (void) uverr;
    }

    if (tloop) {
        cancel_all_pending(tloop);

        tloop->running = false;

        if (tloop->global_stopped) {
            tloop->global_stopped(dicey_task_loop_get_context(tloop));
        }
    }
}

static struct dicey_task_error *task_error_vnew(const enum dicey_error error, const char *const fmt, va_list ap) {
    va_list ap_copy;
    va_copy(ap_copy, ap);

    const int len = vsnprintf(NULL, 0, fmt, ap_copy);

    va_end(ap_copy);

    struct dicey_task_error *const err = calloc(1U, sizeof *err + len + 1);
    if (!err) {
        return NULL;
    }

    err->error = error;
    (void) vsnprintf(err->message, len + 1, fmt, ap);

    return err;
}

struct dicey_task_error *dicey_task_error_new(const enum dicey_error error, const char *const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    struct dicey_task_error *const err = task_error_vnew(error, fmt, ap);

    va_end(ap);

    return err;
}

struct dicey_task_result dicey_task_continue(void) {
    return (struct dicey_task_result) { .kind = DICEY_TASK_CONTINUE };
}

struct dicey_task_result dicey_task_fail(const enum dicey_error error, const char *const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    struct dicey_task_error *const err = task_error_vnew(error, fmt, ap);

    va_end(ap);

    return dicey_task_fail_with(err);
}

struct dicey_task_result dicey_task_fail_with(struct dicey_task_error *const err) {
    return (struct dicey_task_result) { .kind = DICEY_TASK_ERROR, .error = err };
}

struct dicey_task_result dicey_task_retry(void) {
    return (struct dicey_task_result) { .kind = DICEY_TASK_RETRY };
}

void dicey_task_loop_advance(struct dicey_task_loop *const tloop, const int64_t id, void *const input) {
    assert(tloop);

    const struct dicey_task_entry *entry = dicey_task_list_find(tloop->pending_tasks, id);
    if (entry) {
        struct dicey_task_request *const req = entry->data;
        assert(req);

        const bool done = step_task(tloop, id, req, input);
        if (done) {
            dicey_task_list_erase(tloop->pending_tasks, id);
        }
    }
}

void dicey_task_loop_delete(struct dicey_task_loop *const tloop) {
    if (tloop) {
        dicey_task_loop_stop_and_wait(tloop);

        free(tloop);
    }
}

void dicey_task_loop_fail(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    const enum dicey_error error,
    const char *const fmt,
    ...
) {
    va_list ap;
    va_start(ap, fmt);

    struct dicey_task_error *const err = task_error_vnew(error, fmt, ap);

    va_end(ap);

    dicey_task_loop_fail_with(tloop, id, err);
}

void dicey_task_loop_fail_with(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    struct dicey_task_error *const err
) {
    assert(tloop);

    const struct dicey_task_entry *const entry = dicey_task_list_find(tloop->pending_tasks, id);

    if (entry) {
        struct dicey_task_request *const req = entry->data;

        fail_task(tloop, id, req, err);

        dicey_task_list_erase(tloop->pending_tasks, id);
    } else {
        free(err);
    }
}

void *dicey_task_loop_get_context(const struct dicey_task_loop *const tloop) {
    return tloop ? tloop->ctx : NULL;
}

uv_loop_t *dicey_task_loop_get_raw_handle(struct dicey_task_loop *const tloop) {
    return tloop ? tloop->loop : NULL;
}

bool dicey_task_loop_is_running(struct dicey_task_loop *const tloop) {
    assert(tloop);

    return tloop->running;
}

enum dicey_error dicey_task_loop_new(struct dicey_task_loop **const dest, struct dicey_task_loop_args *const args) {
    assert(dest);

    struct dicey_task_loop *const tloop = calloc(1U, sizeof *tloop);
    if (!tloop) {
        return DICEY_ENOMEM;
    }

    if (args) {
        tloop->global_at_end = args->global_at_end;
        tloop->global_stopped = args->global_stopped;
    }

    *dest = tloop;

    return DICEY_OK;
}

void *dicey_task_loop_set_context(struct dicey_task_loop *const tloop, void *const ctx) {
    return tloop ? atomic_exchange(&tloop->ctx, ctx) : NULL;
}

enum dicey_error dicey_task_loop_start(struct dicey_task_loop *const tloop) {
    assert(tloop);

    if (tloop->loop) {
        return DICEY_EALREADY;
    }

    uv_sem_t sem = { 0 };
    const enum dicey_error sem_err = dicey_error_from_uv(uv_sem_init(&sem, 0));
    if (sem_err) {
        free(tloop);
        return sem_err;
    }

    struct thread_init_req req = {
        .tloop = tloop,
        .sem = &sem,
    };

    const enum dicey_error thread_err = dicey_error_from_uv(uv_thread_create(&tloop->thread, &loop_thread, &req));
    if (thread_err) {
        uv_sem_destroy(&sem);
        free(tloop);

        return thread_err;
    }

    uv_sem_wait(&sem);

    if (req.err) {
        uv_thread_join(&tloop->thread);

        return req.err;
    }

    uv_sem_destroy(&sem);

    return DICEY_OK;
}

void dicey_task_loop_stop(struct dicey_task_loop *const tloop) {
    assert(tloop);

    if (tloop->running) {
        uv_async_send(tloop->halt_async);
    }
}

void dicey_task_loop_stop_and_wait(struct dicey_task_loop *const tloop) {
    assert(tloop);

    if (tloop->running) {
        dicey_task_loop_stop(tloop);

        uv_thread_join(&tloop->thread);
    }
}

enum dicey_error dicey_task_loop_submit(struct dicey_task_loop *const tloop, struct dicey_task_request *const req) {
    assert(tloop && req);

    if (!tloop->running) {
        return DICEY_EINVAL;
    }

    if (!dicey_queue_push(&tloop->queue, req, DICEY_LOCKING_POLICY_BLOCKING)) {
        return DICEY_ENOMEM;
    }

    uv_async_send(tloop->jobs_async);

    return DICEY_OK;
}

struct dicey_task_result dicey_task_no_work(
    struct dicey_task_loop *const tloop,
    const int64_t id,
    void *const ctx,
    void *const input
) {
    (void) tloop;
    (void) id;
    (void) ctx;
    (void) input;

    return dicey_task_continue();
}
