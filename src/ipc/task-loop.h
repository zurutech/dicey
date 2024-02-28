// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(JRUPPTCCIV_TASK_LOOP_H)
#define JRUPPTCCIV_TASK_LOOP_H

#include <uv.h>

#include <dicey/core/errors.h>

enum dicey_task_result_kind {
    DICEY_TASK_CONTINUE,
    DICEY_TASK_ERROR,
    DICEY_TASK_RETRY,
};

struct dicey_task_error {
    enum dicey_error error;
    char message[];
};

typedef struct dicey_task_result dicey_task_loop_do_work_fn(uv_loop_t *loop, void *data);
typedef void dicey_task_loop_at_end(struct dicey_task_error *err, void *ctx);

struct dicey_task_request {
    dicey_task_loop_do_work_fn *const *work;
    uint32_t timeout_ms;
    void *ctx;

    dicey_task_loop_at_end *at_end;
};

struct dicey_task_result {
    enum dicey_task_result_kind kind;
    struct dicey_task_error *error;
};

struct dicey_task_error *dicey_task_error_new(enum dicey_error error, const char *fmt, ...);

struct dicey_task_loop;

enum dicey_error dicey_task_loop_init(struct dicey_task_loop **dest);
bool dicey_task_loop_is_running(struct dicey_task_loop *tloop);
enum dicey_error dicey_task_loop_start(struct dicey_task_loop *tloop);
enum dicey_error dicey_task_loop_submit(struct dicey_task_loop *tloop, struct dicey_task_request *req);
enum dicey_error dicey_task_loop_stop(struct dicey_task_loop *tloop);

#endif // JRUPPTCCIV_TASK_LOOP_H
