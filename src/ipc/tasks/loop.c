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

    uint64_t next_id;
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

static void halt_loop(uv_async_t *const async) {
    assert(async);

    struct dicey_task_loop *const tloop = async->data;

    assert(tloop);

    uv_stop(tloop->loop);
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
    (void) ctx;

    assert(expired_item);

    struct dicey_task_request *const task = expired_item;

    assert(task->at_end);

    struct dicey_task_error *const err = dicey_task_error_new(DICEY_ETIMEDOUT, "task timed out");

    task->at_end(id, err, task->ctx);

    free(err);
}

static void check_timeout(uv_timer_t *const timer) {
    struct dicey_task_loop *const tloop = timer->data;

    assert(tloop);

    dicey_task_list_prune(tloop->pending_tasks, &task_timed_out, NULL);
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

    uv_idle_stop(idle);
}

struct task_queue_free_ctx {
    struct dicey_task_loop *tloop;
    struct dicey_task_error *err;
};

static void free_pending_task(void *const ctx, void *const data) {
    assert(ctx && data);

    struct task_queue_free_ctx *const free_ctx = ctx;
    assert(free_ctx->tloop);

    struct dicey_task_request *const req = data;

    complete_task(free_ctx->tloop, -1, req, free_ctx->err);

    free(req);
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

    dicey_queue_deinit(&tloop->queue, &free_pending_task, &free_ctx);

    free(free_ctx.err);
}

static void loop_thread(void *const arg) {
    assert(arg);

    uv_async_t jobs_async = { 0 }, halt_async = { 0 };
    uv_loop_t loop = { 0 };
    uv_timer_t timer = { 0 };
    struct loop_checker up_check = { 0 };

    struct thread_init_req *const req = arg;
    assert(req->sem && req->tloop);

    req->err = dicey_error_from_uv(uv_loop_init(&loop));
    if (req->err) {
        goto deinit_loop;
    }

    req->err = dicey_error_from_uv(uv_async_init(&loop, &jobs_async, &process_queue));
    if (req->err) {
        goto deinit_jobs_async;
    }

    jobs_async.data = req->tloop;

    req->err = dicey_error_from_uv(uv_async_init(&loop, &halt_async, &halt_loop));
    if (req->err) {
        goto deinit_halt_async;
    }

    req->err = dicey_error_from_uv(uv_timer_init(&loop, &timer));
    if (req->err) {
        goto deinit_timer;
    }

    timer.data = req->tloop;

    req->err = dicey_error_from_uv(uv_idle_init(&loop, &up_check.idle));
    if (req->err) {
        goto deinit_idle;
    }

    up_check.tloop = req->tloop;
    up_check.sem = req->sem;

    struct dicey_task_loop *const tloop = req->tloop;
    tloop->loop = &loop;
    tloop->jobs_async = &jobs_async;
    tloop->timer = &timer;

    req->err = dicey_queue_init(&req->tloop->queue);
    if (req->err) {
        goto deinit_queue;
    }

    tloop->pending_tasks = NULL;

    req->err = dicey_error_from_uv(uv_idle_start(&up_check.idle, &notify_running));
    if (req->err) {
        goto clear_all;
    }

    req->err = dicey_error_from_uv(uv_timer_start(&timer, &check_timeout, TIMEOUT_CHECK_MS, TIMEOUT_CHECK_MS));
    if (req->err) {
        goto clear_all;
    }

    const enum dicey_error loop_err = dicey_error_from_uv(uv_run(&loop, UV_RUN_DEFAULT));
    if (loop_err && !tloop->running) {
        req->err = loop_err;
        uv_sem_post(up_check.sem);
    }

    tloop->running = false;

clear_all:
deinit_queue:
    cancel_all_pending(tloop);

deinit_idle:
    if (uv_is_closing((uv_handle_t *) &up_check.idle)) {
        uv_close((uv_handle_t *) &up_check.idle, NULL);
    }

deinit_timer:
    uv_close((uv_handle_t *) &timer, NULL);

deinit_halt_async:
    uv_close((uv_handle_t *) &halt_async, NULL);

deinit_jobs_async:
    uv_close((uv_handle_t *) &jobs_async, NULL);

deinit_loop:
    uv_loop_close(&loop);
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
    assert(tloop);

    dicey_task_loop_stop(tloop);

    free(tloop);
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

        free(err);

        dicey_task_list_erase(tloop->pending_tasks, id);
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
    uv_sem_destroy(&sem);

    if (req.err) {
        uv_thread_join(&tloop->thread);

        return req.err;
    }

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

        *tloop = (struct dicey_task_loop) { 0 };
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
