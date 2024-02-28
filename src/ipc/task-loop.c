// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/errors.h>

#include "queue.h"
#include "task-list.h"
#include "uvtools.h"

#include "task-loop.h"

#define TIMEOUT_CHECK_MS 10U

struct dicey_task_loop {
    _Atomic bool running;

    uv_thread_t thread;
    uv_async_t *jobs_async, *halt_async;
    uv_loop_t *loop;
    uv_timer_t *timer;

    struct dicey_queue queue;
    struct dicey_task_list *pending_tasks;
};

struct thread_init_req {
    enum dicey_error err;

    struct dicey_task_loop *tloop;

    uv_sem_t *sem;
};

static void task_timed_out(void *const ctx, void *const expired_item) {
    (void) ctx;

    assert(expired_item);

    struct dicey_task_request *const task = expired_item;

    assert(task->at_end);

    struct dicey_task_error *const err = dicey_task_error_new(DICEY_ETIMEDOUT, "task timed out");

    task->at_end(err, task->ctx);

    free(err);
}

static void check_timeout(uv_timer_t *const timer) {
    struct dicey_task_loop *const tloop = timer->data;

    assert(tloop);

    dicey_task_list_prune(tloop->pending_tasks, &task_timed_out, NULL);
}

static void complete_task(
    struct dicey_task_loop *const tloop,
    struct dicey_task_request *const task,
    struct dicey_task_error *const err
) {
    assert(tloop && task && task->at_end);

    task->at_end(err, task->ctx);
}

static bool step_task(struct dicey_task_loop *const tloop, struct dicey_task_request *const task) {
    assert(tloop && task && task->work && *task->work && task->at_end);

    struct dicey_task_error *err = NULL;
    struct dicey_task_result result = (*task->work)(tloop->loop, task->ctx);

    switch (result.kind) {
    case DICEY_TASK_CONTINUE:
        assert(!result.error);

        ++task->work;

        break;

    case DICEY_TASK_ERROR:
        err = result.error;
        task->work = NULL;

        break;

    case DICEY_TASK_RETRY:
        assert(!result.error);

        break;
    }

    // task is done
    if (!task->work) {
        complete_task(tloop, task, err);

        free(err);
    }

    return task->work;
}

static void halt_loop(uv_async_t *const async) {
    assert(async);

    struct dicey_task_loop *const tloop = async->data;

    assert(tloop);

    uv_stop(tloop->loop);
}

static void process_queue(uv_async_t *async) {
    assert(async);

    struct dicey_task_loop *const task_loop = async->data;
    assert(task_loop);

    void *req_ptr = NULL;
    while (dicey_queue_pop(&task_loop->queue, &req_ptr, DICEY_LOCKING_POLICY_NONBLOCKING)) {
        assert(req_ptr);

        struct dicey_task_request *const req = req_ptr;
        assert(req->work && *req->work);

        if (step_task(task_loop, req)) {
            dicey_task_list_append(&task_loop->pending_tasks, req, req->timeout_ms);
        }

        free(req);
    }
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

    complete_task(free_ctx->tloop, req, free_ctx->err);

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

            complete_task(tloop, req, free_ctx.err);
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

    *req = (struct thread_init_req) { 0 };

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

struct dicey_task_error *dicey_task_error_new(const enum dicey_error error, const char *const fmt, ...) {
    va_list ap, ap_copy;
    va_start(ap, fmt);
    va_copy(ap_copy, ap);

    const int len = vsnprintf(NULL, 0, fmt, ap_copy);

    va_end(ap_copy);

    struct dicey_task_error *const err = calloc(1U, sizeof *err + len + 1);
    if (!err) {
        return NULL;
    }

    err->error = error;
    (void) vsnprintf(err->message, len + 1, fmt, ap);

    va_end(ap);

    return err;
}

enum dicey_error dicey_task_loop_init(struct dicey_task_loop **const dest) {
    assert(dest);

    struct dicey_task_loop *const tloop = calloc(1U, sizeof *tloop);
    if (!tloop) {
        return DICEY_ENOMEM;
    }

    *dest = tloop;

    return DICEY_OK;
}

bool dicey_task_loop_is_running(struct dicey_task_loop *const tloop) {
    assert(tloop);

    return tloop->running;
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

        free(tloop);

        return req.err;
    }

    return DICEY_OK;
}

enum dicey_error dicey_task_loop_stop(struct dicey_task_loop *const tloop) {
    assert(tloop);

    if (!tloop->running) {
        return DICEY_EALREADY;
    }

    uv_async_send(tloop->halt_async);

    uv_thread_join(&tloop->thread);

    return DICEY_OK;
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
