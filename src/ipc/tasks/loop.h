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

#if !defined(JRUPPTCCIV_TASK_LOOP_H)
#define JRUPPTCCIV_TASK_LOOP_H

#include <uv.h>

#include <dicey/core/errors.h>

#include "sup/util.h"

#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4200)
#endif

enum dicey_task_result_kind {
    DICEY_TASK_CONTINUE,
    DICEY_TASK_NEXT,
    DICEY_TASK_ERROR,
    DICEY_TASK_RETRY,
};

struct dicey_task_error {
    enum dicey_error error;
    char message[];
};

struct dicey_task_error *dicey_task_error_new(enum dicey_error error, const char *fmt, ...) DICEY_FORMAT(2, 3);

struct dicey_task_loop;

typedef struct dicey_task_result dicey_task_loop_do_work_fn(
    struct dicey_task_loop *tloop,
    int64_t id,
    void *ctx,
    void *input
);

typedef void dicey_task_loop_at_end(int64_t id, struct dicey_task_error *err, void *ctx);

typedef void dicey_task_loop_global_at_end(void *ctx, int64_t id, struct dicey_task_error *err);
typedef void dicey_task_loop_global_stopped(void *ctx);

struct dicey_task_loop_args {
    // called when any task is done. It basically works as a global inspector, useful to clean up state before calling
    // the tasks' at_end callback.
    dicey_task_loop_global_at_end *global_at_end;

    // called when the task loop is stopped, but before the thread quits. It's useful to clean up state before the task
    // loop is deleted.
    dicey_task_loop_global_stopped *global_stopped;
};

struct dicey_task_request {
    dicey_task_loop_do_work_fn *const *work;
    int32_t timeout_ms;
    void *ctx;

    dicey_task_loop_at_end *at_end;
};

struct dicey_task_result {
    enum dicey_task_result_kind kind;
    struct dicey_task_error *error;
};

struct dicey_task_result dicey_task_continue(void);
struct dicey_task_result dicey_task_fail(enum dicey_error error, const char *fmt, ...);
struct dicey_task_result dicey_task_fail_with(struct dicey_task_error *err);
struct dicey_task_result dicey_task_next(void);
struct dicey_task_result dicey_task_retry(void);

/**
 * @brief A work function that does nothing except returning a continue result.
 */
struct dicey_task_result dicey_task_noop(struct dicey_task_loop *tloop, int64_t id, void *ctx, void *input);

enum dicey_error dicey_task_loop_new(struct dicey_task_loop **dest, struct dicey_task_loop_args *args);
void dicey_task_loop_delete(struct dicey_task_loop *tloop);

void dicey_task_loop_advance(struct dicey_task_loop *tloop, int64_t id, void *input);
void dicey_task_loop_fail(struct dicey_task_loop *tloop, int64_t id, enum dicey_error error, const char *fmt, ...);
void dicey_task_loop_fail_with(struct dicey_task_loop *tloop, int64_t id, struct dicey_task_error *err);
void *dicey_task_loop_get_context(const struct dicey_task_loop *tloop);
bool dicey_task_loop_is_running(struct dicey_task_loop *tloop);
void *dicey_task_loop_set_context(struct dicey_task_loop *tloop, void *ctx);
enum dicey_error dicey_task_loop_start(struct dicey_task_loop *tloop);
enum dicey_error dicey_task_loop_submit(struct dicey_task_loop *tloop, struct dicey_task_request *req);
void dicey_task_loop_stop(struct dicey_task_loop *tloop);
void dicey_task_loop_stop_and_wait(struct dicey_task_loop *tloop);

uv_loop_t *dicey_task_loop_get_uv_handle(struct dicey_task_loop *tloop);

#endif // JRUPPTCCIV_TASK_LOOP_H
