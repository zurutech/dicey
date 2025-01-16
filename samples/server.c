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

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _XOPEN_SOURCE 700

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/dicey.h>

#include <util/dumper.h>
#include <util/getopt.h>
#include <util/packet-dump.h>
#include <util/strext.h>

#include "sval.h"
#include "timer.h"

#if defined(DICEY_IS_LINUX) || defined(DICEY_IS_WINDOWS)
#define PIPE_NEEDS_CLEANUP false
#if defined(DICEY_IS_WINDOWS)
#define PIPE_NAME "\\\\.\\pipe\\uvsock"
#else
#define PIPE_NAME "@/tmp/.uvsock"
#endif
#else
#define PIPE_NEEDS_CLEANUP true
#define PIPE_NAME "/tmp/.uvsock"
#endif

#define TIMER_STATE_KEY "$timer.state"

#define DUMMY_PATH "/foo/bar"
#define DUMMY_TRAIT "dummy.Trait"
#define DUMMY_POINTS_ELEMENT "Points"
#define DUMMY_POINTS_SIG "[{ff}]"

#define SELF_PATH "/dicey/sample_server"
#define SELF_TRAIT "dicey.sample.Server"
#define HALT_ELEMENT "Halt"
#define HALT_SIGNATURE "$ -> $"

#define ECHO_PATH "/dicey/test/echo"
#define ECHO_TRAIT "dicey.test.Echo"
#define ECHO_ECHO_ELEMENT "Echo"
#define ECHO_ECHO_SIGNATURE "v -> v"

#define TEST_MGR_PATH "/dicey/test/manager"
#define TEST_MGR_TRAIT "dicey.test.Manager"
#define TEST_MGR_ADD_ELEMENT "Add"
#define TEST_MGR_ADD_SIGNATURE "s -> @"
#define TEST_MGR_DEL_ELEMENT "Delete"
#define TEST_MGR_DEL_SIGNATURE "@ -> $"

#define TEST_OBJ_PATH_BASE "/dicey/test/object/"
#define TEST_OBJ_PATH_FMT TEST_OBJ_PATH_BASE "%zu"
#define TEST_OBJ_TRAIT "dicey.test.Object"
#define TEST_OBJ_NAME_ELEMENT "Name"
#define TEST_OBJ_NAME_SIGNATURE "s"

static struct dicey_server *global_server = NULL;
static bool print_logs = false;

static int out(const char *fmt, ...) {
    if (!print_logs) {
        return 0;
    }

    va_list args;
    va_start(args, fmt);

    int ret = vfprintf(stdout, fmt, args);

    va_end(args);

    return ret;
}

static inline void dump_packet(const struct dicey_packet packet) {
    if (print_logs) {
        struct util_dumper dumper = util_dumper_for(stdout);
        util_dumper_dump_packet(&dumper, packet);
    }
}

#if DICEY_HAS_PLUGINS

#include "dummy_plugin.h"

static _Atomic bool dummy_running = false;

static enum dicey_error quit_dummy(struct dicey_server *const server) {
    assert(server);

    if (dummy_running) {
        int64_t res = 0;

        // this is not great, we should store the name the plugin returns somewhere instead. Still this is just a
        // testing example
        return dicey_server_plugin_quit_and_wait(server, DUMMY_PLUGIN_NAME, &res);
    }

    return DICEY_OK;
}

#endif

static enum dicey_error shutdown_server(struct dicey_server *const server) {
    assert(server);

    return dicey_server_stop(server);
}

#if defined(DICEY_IS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static BOOL quit_server(_In_ const DWORD dwCtrlType) {
    (void) dwCtrlType;

    if (global_server) {
        const enum dicey_error err = shutdown_server(global_server);
        assert(!err);
        (void) err;
    }

    return TRUE;
}

static bool register_break_hook(void) {
    return SetConsoleCtrlHandler(&quit_server, TRUE);
}

#elif defined(DICEY_IS_UNIX)

#include <unistd.h>

static void quit_server(const int sig) {
    (void) sig;

    if (global_server) {
        const enum dicey_error err = shutdown_server(global_server);
        assert(!err);
        (void) err;
    }
}

static bool register_break_hook(void) {
    struct sigaction sa = { 0 };
    sa.sa_handler = &quit_server;
    (void) sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    return sigaction(SIGINT, &sa, NULL) == 0;
}

#endif

struct server_ctx {
    struct dicey_hashtable *hash;

    uv_sem_t startup_sem;
    enum dicey_error startup_err;
};

struct test_element {
    enum dicey_element_type type;
    const char *name;
    const char *signature;

    bool readonly;
};

struct test_trait {
    const char *name;
    const struct test_element *const *elements;
};

static const struct test_trait test_traits[] = {
    {
     .name = DUMMY_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_PROPERTY,
                    .name = DUMMY_POINTS_ELEMENT,
                    .signature = DUMMY_POINTS_SIG,
                    .readonly = true,
                },
                NULL,
            }, },
    {
     .name = SVAL_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_PROPERTY,
                    .name = SVAL_PROP,
                    .signature = SVAL_SIG,
                },
                NULL,
            }, },
    {
     .name = SELF_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_OPERATION,
                    .name = HALT_ELEMENT,
                    .signature = HALT_SIGNATURE,
                },
                NULL,
            }, },
    {
     .name = ECHO_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_OPERATION,
                    .name = ECHO_ECHO_ELEMENT,
                    .signature = ECHO_ECHO_SIGNATURE,
                },
                NULL,
            }, },
    {
     .name = TEST_MGR_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_OPERATION,
                    .name = TEST_MGR_ADD_ELEMENT,
                    .signature = TEST_MGR_ADD_SIGNATURE,
                },
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_OPERATION,
                    .name = TEST_MGR_DEL_ELEMENT,
                    .signature = TEST_MGR_DEL_SIGNATURE,
                },
                NULL,
            }, },
    {
     .name = TEST_OBJ_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_PROPERTY,
                    .name = TEST_OBJ_NAME_ELEMENT,
                    .signature = TEST_OBJ_NAME_SIGNATURE,
                    .readonly = true,
                },
                NULL,
            }, },
    {
     .name = TEST_TIMER_TRAIT,
     .elements =
            (const struct test_element *[]) {
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_OPERATION,
                    .name = TEST_TIMER_START_ELEMENT,
                    .signature = TEST_TIMER_START_SIGNATURE,
                },
                &(struct test_element) {
                    .type = DICEY_ELEMENT_TYPE_SIGNAL,
                    .name = TEST_TIMER_TIMERFIRED_ELEMENT,
                    .signature = TEST_TIMER_TIMERFIRED_SIGNATURE,
                },
                NULL,
            }, },
};

struct test_object {
    const char *path;
    const char *const *traits;
};

static const struct test_object test_objects[] = {
    {.path = DUMMY_PATH,       .traits = (const char *[]) { DUMMY_TRAIT, NULL }     },
    { .path = SVAL_PATH,       .traits = (const char *[]) { SVAL_TRAIT, NULL }      },
    { .path = SELF_PATH,       .traits = (const char *[]) { SELF_TRAIT, NULL }      },
    { .path = ECHO_PATH,       .traits = (const char *[]) { ECHO_TRAIT, NULL }      },
    { .path = TEST_MGR_PATH,   .traits = (const char *[]) { TEST_MGR_TRAIT, NULL }  },
    { .path = TEST_TIMER_PATH, .traits = (const char *[]) { TEST_TIMER_TRAIT, NULL }},
};

struct timer_state {
    struct dicey_server *server;

    uv_thread_t thread;
    uv_mutex_t mux;

    uv_timeval64_t target;
    bool quit;
};

struct dummy_points {
    double x, y;
} points[] = {
    {.x = 1.0,   .y = 2.0 },
    { .x = 3.2,  .y = -4.5},
    { .x = 5.0,  .y = 6.0 },
    { .x = 7.4,  .y = -8.9},
    { .x = -9.0, .y = 10.0},
};

static enum dicey_error craft_dummy_points(struct dicey_packet *const dest, const uint32_t seq) {
    assert(dest);

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = dicey_message_builder_init(&builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_RESPONSE);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_seq(&builder, seq);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&builder, DUMMY_PATH);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(
        &builder,
        (struct dicey_selector) {
            .trait = DUMMY_TRAIT,
            .elem = DUMMY_POINTS_ELEMENT,
        }
    );

    if (err) {
        goto fail;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        goto fail;
    }

    err = dicey_value_builder_array_start(&value_builder, DICEY_TYPE_PAIR);
    if (err) {
        goto fail;
    }

    struct dicey_value_builder point_builder = { 0 };
    const struct dummy_points *const points_end = points + sizeof(points) / sizeof(points[0]);

    for (const struct dummy_points *point = points; point < points_end; ++point) {
        err = dicey_value_builder_next(&value_builder, &point_builder);
        if (err) {
            goto fail;
        }

        err = dicey_value_builder_pair_start(&point_builder);
        if (err) {
            goto fail;
        }

        struct dicey_value_builder item = { 0 };
        const double items[2U] = { point->x, point->y };

        for (size_t i = 0U; i < 2U; ++i) {
            err = dicey_value_builder_next(&point_builder, &item);
            if (err) {
                goto fail;
            }

            err = dicey_value_builder_set(&item, (struct dicey_arg) { .type = DICEY_TYPE_FLOAT, .floating = items[i] });
            if (err) {
                goto fail;
            }
        }

        err = dicey_value_builder_pair_end(&point_builder);
        if (err) {
            goto fail;
        }
    }

    err = dicey_value_builder_array_end(&value_builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_value_end(&builder, &value_builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_build(&builder, dest);
    if (err) {
        goto fail;
    }

    return DICEY_OK;

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

static enum dicey_error craft_timer_event(struct dicey_packet *const dest, const uv_timeval64_t tv) {
    assert(dest);

    return dicey_packet_message(
        dest,
        0U,
        DICEY_OP_SIGNAL,
        TEST_TIMER_PATH,
        (struct dicey_selector) {
            .trait = TEST_TIMER_TRAIT,
            .elem = TEST_TIMER_TIMERFIRED_ELEMENT,
        },
        (struct dicey_arg) {
            .type = DICEY_TYPE_TUPLE,
            .tuple = {
                .nitems = 2U,
                .elems = (struct dicey_arg[]) {
                    { .type = DICEY_TYPE_INT64, .i64 = tv.tv_sec },
                    { .type = DICEY_TYPE_INT32, .i32 = tv.tv_usec },
                },
            },
        }
    );
}

bool timeval_is_zero(const uv_timeval64_t tv) {
    return tv.tv_sec == 0 && tv.tv_usec == 0;
}

intmax_t timeval_cmp(const uv_timeval64_t a, const uv_timeval64_t b) {
    return a.tv_sec == b.tv_sec ? a.tv_usec - b.tv_usec : a.tv_sec - b.tv_sec;
}

void timer_thread_fn(void *const arg) {
    struct timer_state *const state = arg;

    uv_timeval64_t now = { 0 };

    for (;;) {
        uv_mutex_lock(&state->mux);

        if (state->quit) {
            uv_mutex_unlock(&state->mux);

            break;
        }

        uv_gettimeofday(&now);

        if (timeval_cmp(now, state->target) >= 0 && !timeval_is_zero(state->target)) {
            // assume the target time has been reached

            struct dicey_packet packet = { 0 };

            enum dicey_error err = craft_timer_event(&packet, now);

            assert(!err);

            err = dicey_server_raise_and_wait(state->server, packet);
            assert(!err);

            (void) err;

            state->target = (uv_timeval64_t) { 0 };
        }

        uv_mutex_unlock(&state->mux);

        uv_sleep(10U);
    }
}

void timer_state_deinit(struct timer_state *const state) {
    assert(state);

    uv_mutex_lock(&state->mux);

    state->quit = true;

    uv_mutex_unlock(&state->mux);

    uv_thread_join(&state->thread);

    uv_mutex_destroy(&state->mux);
}

void timer_state_init(struct timer_state *const state, struct dicey_server *const server) {
    assert(state && server);

    // it's pointless to properly check this in a sample, so I will just assert

    *state = (struct timer_state) { .server = server };

    int res = uv_mutex_init(&state->mux);
    assert(!res);

    res = uv_thread_create(&state->thread, &timer_thread_fn, state);
    assert(!res);

    (void) res;
}

void timer_state_fire_after(struct timer_state *const state, const int32_t s) {
    assert(state);

    uv_mutex_lock(&state->mux);

    uv_gettimeofday(&state->target);

    state->target.tv_sec += s;

    uv_mutex_unlock(&state->mux);
}

static bool matches_elem(
    const struct dicey_message *const msg,
    const char *const path,
    const char *const trait,
    const char *const elem
) {
    return !strcmp(msg->path, path) && !strcmp(msg->selector.trait, trait) && !strcmp(msg->selector.elem, elem);
}

static bool matches_elem_under_root(
    const struct dicey_message *const msg,
    const char *const root,
    const char *const trait,
    const char *const elem
) {
    return !strncmp(msg->path, root, strlen(root)) && !strcmp(msg->selector.trait, trait) &&
           !strcmp(msg->selector.elem, elem);
}

static enum dicey_error registry_fill(struct dicey_registry *const registry) {
    assert(registry);

    const struct test_trait *const traits_end = test_traits + sizeof(test_traits) / sizeof(test_traits[0]);
    for (const struct test_trait *trait_def = test_traits; trait_def < traits_end; ++trait_def) {
        assert(trait_def->name && trait_def->elements);

        struct dicey_trait *const trait = dicey_trait_new(trait_def->name);
        if (!trait) {
            return DICEY_ENOMEM;
        }

        for (const struct test_element *const *element_def_ptr = trait_def->elements; *element_def_ptr;
             ++element_def_ptr) {
            const struct test_element *const element = *element_def_ptr;

            assert(element->name && element->signature && element->type != DICEY_ELEMENT_TYPE_INVALID);

            const enum dicey_error err = dicey_trait_add_element(
                trait,
                element->name,
                (struct dicey_element) {
                    .type = element->type,
                    .signature = element->signature,
                    .flags = element->readonly ? DICEY_ELEMENT_READONLY : 0,
                }
            );

            if (err) {
                dicey_trait_delete(trait);

                return err;
            }
        }

        const enum dicey_error err = dicey_registry_add_trait(registry, trait);
        if (err) {
            dicey_trait_delete(trait);

            return err;
        }
    }

    const struct test_object *const objects_end = test_objects + sizeof(test_objects) / sizeof(test_objects[0]);
    for (const struct test_object *object_def = test_objects; object_def < objects_end; ++object_def) {
        assert(object_def->path && object_def->traits);

        struct dicey_hashset *traits_set = dicey_hashset_new();
        if (!traits_set) {
            return DICEY_ENOMEM;
        }

        for (const char *const *trait_name_ptr = object_def->traits; *trait_name_ptr; ++trait_name_ptr) {
            const char *const trait_name = *trait_name_ptr;

            const enum dicey_hash_set_result result = dicey_hashset_add(&traits_set, trait_name);
            switch (result) {
            case DICEY_HASH_SET_FAILED:
                dicey_hashset_delete(traits_set);

                return DICEY_ENOMEM;

            case DICEY_HASH_SET_ADDED:
                break;

            case DICEY_HASH_SET_UPDATED:
                dicey_hashset_delete(traits_set);

                return DICEY_EINVAL;
            }
        }

        const enum dicey_error err = dicey_registry_add_object_with_trait_set(registry, object_def->path, traits_set);
        if (err) {
            dicey_hashset_delete(traits_set);

            return err;
        }
    }

    return DICEY_OK;
}

static bool on_client_connect(struct dicey_server *const server, const size_t id, void **const user_data) {
    (void) server;

    out("info: client %zu connected\n", id);

    *user_data = NULL;

    return true;
}

static void on_client_disconnect(struct dicey_server *const server, const struct dicey_client_info *const cln) {
    (void) server;

    out("info: client %zu disconnected\n", cln->id);
}

static void on_client_error(
    struct dicey_server *const server,
    const enum dicey_error err,
    const struct dicey_client_info *const cln,
    const char *const msg,
    ...
) {
    (void) server;

    va_list args;
    va_start(args, msg);

    fprintf(stderr, "%sError (%s)", dicey_error_name(err), dicey_error_msg(err));

    if (cln) {
        fprintf(stderr, " (on client %zu)", cln->id);
    }

    fprintf(stderr, ": ");
    vfprintf(stderr, msg, args);
    fputc('\n', stderr);

    va_end(args);
}

static enum dicey_error send_reply(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const char *const path,
    const struct dicey_selector sel,
    struct dicey_arg payload
) {

    struct dicey_packet packet = { 0 };
    enum dicey_error err = dicey_packet_message(&packet, seq, DICEY_OP_RESPONSE, path, sel, payload);
    if (err) {
        return err;
    }

    err = dicey_server_send_response(server, cln->id, packet);
    if (err) {
        dicey_packet_deinit(&packet);
        return err;
    }

    return DICEY_OK;
}

static enum dicey_error on_dummy_points_req(struct dicey_server *const server, const size_t id, const uint32_t seq) {
    assert(server && seq); // seq != 0

    struct dicey_packet packet = { 0 };

    enum dicey_error err = craft_dummy_points(&packet, seq);
    if (err) {
        return err;
    }

    err = dicey_server_send_response(server, id, packet);
    if (err) {
        dicey_packet_deinit(&packet);

        return err;
    }

    return DICEY_OK;
}

static enum dicey_error on_echo_req(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    struct dicey_packet packet,
    const struct dicey_message *const req
) {
    assert(server && cln && dicey_packet_is_valid(packet) && req && req->type == DICEY_OP_EXEC);

    struct dicey_packet fixed = { 0 };

    // rewrite the message as a response
    const enum dicey_error err =
        dicey_packet_forward_message(&fixed, packet, seq, DICEY_OP_RESPONSE, req->path, req->selector);
    if (err) {
        return err;
    }

    return dicey_server_send_response(server, cln->id, fixed);
}

static enum dicey_error on_sval_req(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message *const req
) {
    assert(server && cln && req);

    struct dicey_hashtable **const table = dicey_server_get_context(server);
    assert(table && *table);

    const char sval_entry[] = { SVAL_PATH "#" SVAL_TRAIT ":" SVAL_PROP };

    switch (req->type) {
    case DICEY_OP_GET:
        {
            const char *const sval = dicey_hashtable_get(*table, sval_entry);

            return send_reply(
                server,
                cln,
                seq,
                req->path,
                req->selector,
                (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = sval ? sval : "" }
            );
        }

    case DICEY_OP_SET:
        {
            const char *str = NULL;
            const enum dicey_error err = dicey_value_get_str(&req->value, &str);
            if (err) {
                (void) send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_ERROR, .error = {
                    .code = err,
                    .message = dicey_error_msg(err),
                } });

                return err;
            }

            char *const sval = strdup(str);
            if (!sval) {
                return DICEY_ENOMEM;
            }

            void *old_sval = NULL;
            const enum dicey_hash_set_result set_res = dicey_hashtable_set(table, sval_entry, sval, &old_sval);

            free(old_sval);

            if (set_res == DICEY_HASH_SET_FAILED) {
                free(sval);

                return DICEY_ENOMEM;
            }

            return send_reply(
                server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT }
            );
        }

    default:
        abort();
    }
}

static enum dicey_error on_test_add(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message *const req
) {
    assert(server && cln && req && req->type == DICEY_OP_EXEC);

    const char test_mgr[] = { TEST_MGR_PATH "#" TEST_MGR_TRAIT ":" TEST_MGR_ADD_ELEMENT };

    const char *str = NULL;
    enum dicey_error err = dicey_value_get_str(&req->value, &str);
    if (err) {
        return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_ERROR, .error = {
            .code = err,
            .message = dicey_error_msg(err),
        } });
    }

    struct dicey_hashtable **const table = dicey_server_get_context(server);

    assert(table && *table);

    // get index from table - if not found, malloc a new one and set it to 0
    size_t *index = dicey_hashtable_get(*table, test_mgr);
    if (index) {
        ++*index;
    } else {
        size_t *const new_index = calloc(1U, sizeof *new_index);
        if (!new_index) {
            return DICEY_ENOMEM;
        }

        void *old_index = NULL;
        const enum dicey_hash_set_result set_res = dicey_hashtable_set(table, test_mgr, new_index, &old_index);

        assert(!old_index);

        if (set_res == DICEY_HASH_SET_FAILED) {
            free(new_index);

            return DICEY_ENOMEM;
        }

        index = new_index;
    }

    // swanky buffer to hold the object path we're going to snprintf. this is enough space for numbers up to 2^128 at
    // the least
    char str_buffer[sizeof(TEST_OBJ_PATH_FMT) + 40] = { 0 };

    snprintf(str_buffer, sizeof str_buffer, TEST_OBJ_PATH_FMT, *index);

    err = dicey_server_add_object_with(server, str_buffer, TEST_OBJ_TRAIT, NULL);
    if (err) {
        return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_ERROR, .error = {
            .code = err,
            .message = dicey_error_msg(err),
        } });
    }

    char *const name = strdup(str);
    if (!name) {
        return DICEY_ENOMEM;
    }

    void *old_obj = NULL;
    const enum dicey_hash_set_result set_res = dicey_hashtable_set(table, str_buffer, name, &old_obj);

    assert(!old_obj && set_res != DICEY_HASH_SET_UPDATED);

    if (set_res == DICEY_HASH_SET_FAILED) {
        free(name);
        return DICEY_ENOMEM;
    }

    return send_reply(
        server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_PATH, .path = str_buffer }
    );
}

static enum dicey_error on_test_del(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message *const req
) {
    assert(server && cln && req && req->type == DICEY_OP_EXEC);

    const char *path = NULL;
    enum dicey_error err = dicey_value_get_path(&req->value, &path);
    if (err) {
        return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_ERROR, .error = {
            .code = err,
            .message = dicey_error_msg(err),
        } });
    }

    if (strncmp(path, TEST_OBJ_PATH_BASE, sizeof(TEST_OBJ_PATH_BASE) - 1)) {
        return send_reply(
            server, cln, seq, req->path, req->selector, (struct dicey_arg) {
                .type = DICEY_TYPE_ERROR,
                .error = {
                    .code = (uint16_t) DICEY_EINVAL,
                    .message = "can't delete the given path - not a test object",
                },
            }
        );
    }

    struct dicey_hashtable **const table = dicey_server_get_context(server);

    assert(table && *table);

    char *const name = dicey_hashtable_remove(*table, path);

    if (!name) {
        return send_reply(
            server, cln, seq, req->path, req->selector, (struct dicey_arg) {
                .type = DICEY_TYPE_ERROR,
                .error = {
                    .code = (uint16_t) DICEY_EPATH_NOT_FOUND,
                    .message = "can't delete the given path - not found",
                },
            }
        );
    }

    free(name);

    err = dicey_server_delete_object(server, path);
    if (err) {
        return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) {
            .type = DICEY_TYPE_ERROR,
            .error = {
                .code = err,
                .message = dicey_error_msg(err),
            },
        });
    }

    return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
}

static enum dicey_error on_test_obj_name(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message *const req
) {
    assert(server && cln && req && req->type == DICEY_OP_GET);

    struct dicey_hashtable **const table = dicey_server_get_context(server);

    assert(table && *table);

    const char *name = dicey_hashtable_get(*table, req->path);

    assert(name);

    return send_reply(
        server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = name }
    );
}

static enum dicey_error on_timer_start(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message *const req
) {
    assert(server && cln && req && req->type == DICEY_OP_EXEC);

    struct dicey_hashtable **const table = dicey_server_get_context(server);

    assert(table && *table);

    struct timer_state *const state = dicey_hashtable_get(*table, TIMER_STATE_KEY);

    assert(state);

    int32_t usec = 0;
    const enum dicey_error err = dicey_value_get_i32(&req->value, &usec);

    if (err) {
        return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) {
            .type = DICEY_TYPE_ERROR,
            .error = {
                .code = err,
                .message = dicey_error_msg(err),
            }
        });
    }

    timer_state_fire_after(state, usec);

    return send_reply(server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
}

#if DICEY_HAS_PLUGINS

#if defined(DICEY_IS_WINDOWS)
#define DUMMY_PLUGIN "dummy_plugin.exe"
#define SEPARATOR '\\'
#else
#define DUMMY_PLUGIN "dummy_plugin"
#define SEPARATOR '/'
#endif

// big enough for all paths, probably
#define PATHBUF 4096U

static void on_plugin_event(struct dicey_server *const server, const struct dicey_plugin_event event) {
    (void) server;

    out("info: plugin event \"%s\": {name = \"%s\", path = \"%s\"}\n",
        dicey_plugin_event_kind_to_string(event.kind),
        event.info.name ? event.info.name : "N/A (not handshaked yet)",
        event.info.path);

    if (event.info.name && !strcmp(event.info.name, DUMMY_PLUGIN_NAME)) {
        switch (event.kind) {
        case DICEY_PLUGIN_EVENT_READY:
            dummy_running = true;
            break;

        case DICEY_PLUGIN_EVENT_FAILED:
        case DICEY_PLUGIN_EVENT_QUIT:
            dummy_running = false;
            break;

        default:
            break;
        }
    }
}

static char *exedir(char *const dest, size_t *const size) {
    assert(dest && size && *size);

    char exepath[PATHBUF] = { 0 };
    size_t psize = PATHBUF;

    if (uv_exepath(exepath, &psize) < 0) {
        return NULL;
    }

    char *last_sep = strrchr(exepath, SEPARATOR);
    if (!last_sep) {
        return NULL;
    }

    *last_sep = '\0';

    const size_t dsize = strlen(exepath);

    if (dsize + 1 >= *size) {
        return NULL;
    }

    *size = dsize;

    return memcpy(dest, exepath, dsize + 1); // copy null pointer
}

char *plugin_path(char *const dest, size_t *const size) {
    assert(dest && size && *size);

    char dirbuf[PATHBUF] = { 0 };
    size_t dsize = PATHBUF;

    char *const dir = exedir(dirbuf, &dsize);
    if (!dir) {
        return NULL;
    }

    const size_t req_size = dsize + 1 + sizeof DUMMY_PLUGIN; // dirpath + separator + plugin name + null terminator
    if (req_size >= *size) {
        return NULL;
    }

    *size = req_size;
    (void) snprintf(dest, req_size, "%s%c" DUMMY_PLUGIN, dir, SEPARATOR);

    return dest;
}

enum dicey_error send_work_test(struct dicey_server *const server) {
    assert(server);

    const double a = 42.2, b = -7.6;

    struct dicey_owning_value response = { 0 };

    enum dicey_error err = dicey_server_plugin_send_work_and_wait(
        server,
        DUMMY_PLUGIN_NAME, // again, hard coded value, not great
        (struct dicey_arg) {
            .type = DICEY_TYPE_PAIR,
            .pair = {
                .first = &(struct dicey_arg) { .type = DICEY_TYPE_FLOAT, .floating = a },
                .second = &(struct dicey_arg) { .type = DICEY_TYPE_FLOAT, .floating = b },
            },
        },
        &response
    );

    if (err) {
        return err;
    }

    const struct dicey_value *const value = dicey_owning_value_borrow(&response);
    if (!value) {
        return DICEY_EINVAL;
    }

    if (dicey_value_get_type(value) != DICEY_TYPE_FLOAT) {
        return DICEY_EINVAL;
    }

    double result = 0.0;
    err = dicey_value_get_float(value, &result);
    if (err) {
        abort(); // should never happen
    }

    out("info: plugin says that %f * %f is %f\n", a, b, result);

    return DICEY_OK;
}

enum dicey_error spawn_dummy_plugin(struct dicey_server *const server) {
    assert(server);

    char pathbuf[PATHBUF] = { 0 };
    size_t pathsize = PATHBUF;

    char *const path = plugin_path(pathbuf, &pathsize);
    if (!path) {
        return DICEY_EINVAL;
    }

    return dicey_server_spawn_plugin_and_wait(server, path, NULL);
}

#endif // DICEY_HAS_PLUGINS

static void on_request_received(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    struct dicey_packet packet
) {
    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(packet, &msg);
    if (err) {
        fprintf(stderr, "error: malformed message: %s\n", dicey_error_msg(err));
        return;
    }

    out("info: received request #%" PRIu32 " from client %zu for `%s#%s:%s`\n",
        seq,
        cln->id,
        msg.path,
        msg.selector.trait,
        msg.selector.elem);

    dump_packet(packet);

    if (matches_elem(&msg, DUMMY_PATH, DUMMY_TRAIT, DUMMY_POINTS_ELEMENT)) {
        err = on_dummy_points_req(server, cln->id, seq);

        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        }
    } else if (matches_elem(&msg, SVAL_PATH, SVAL_TRAIT, SVAL_PROP)) {
        err = on_sval_req(server, cln, seq, &msg);
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        }
    } else if (matches_elem(&msg, SELF_PATH, SELF_TRAIT, HALT_ELEMENT)) {
        err = send_reply(server, cln, seq, msg.path, msg.selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        }

        dicey_server_stop(server);
    } else if (matches_elem(&msg, ECHO_PATH, ECHO_TRAIT, ECHO_ECHO_ELEMENT)) {
        err = on_echo_req(server, cln, seq, packet, &msg);
    } else if (matches_elem(&msg, TEST_MGR_PATH, TEST_MGR_TRAIT, TEST_MGR_ADD_ELEMENT)) {
        err = on_test_add(server, cln, seq, &msg);
    } else if (matches_elem(&msg, TEST_MGR_PATH, TEST_MGR_TRAIT, TEST_MGR_DEL_ELEMENT)) {
        err = on_test_del(server, cln, seq, &msg);
    } else if (matches_elem_under_root(&msg, TEST_OBJ_PATH_BASE, TEST_OBJ_TRAIT, TEST_OBJ_NAME_ELEMENT)) {
        err = on_test_obj_name(server, cln, seq, &msg);
    } else if (matches_elem(&msg, TEST_TIMER_PATH, TEST_TIMER_TRAIT, TEST_TIMER_START_ELEMENT)) {
        err = on_timer_start(server, cln, seq, &msg);
    }

    // this function receives a copy of the packet that must be freed
    dicey_packet_deinit(&packet);
}

void on_startup_done(struct dicey_server *const server, enum dicey_error err) {
    assert(server);

    out("info: server %s\n", err ? "failed to start" : "started");

    struct server_ctx *const ctx = dicey_server_get_context(server);
    assert(ctx);

    ctx->startup_err = err;

    uv_sem_post(&ctx->startup_sem);
}

#if PIPE_NEEDS_CLEANUP
static enum dicey_error remove_socket_if_present(void) {
    const int err = uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

    switch (err) {
    case 0:
    case UV_ENOENT:
        return DICEY_OK;

    default:
        return DICEY_EUV_UNKNOWN;
    }
}
#endif

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...]\n"                                                                                         \
    "  -h  print this help message and exit\n"                                                                         \
    "  -v  print info\n"

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

struct thread_args {
    struct dicey_server *server;
    struct dicey_addr addr;
};

static void server_thread(void *arg) {
    struct thread_args *const args = arg;

    // ignore the error here - we'll get it from the callback
    (void) dicey_server_start(global_server, args->addr);
}

static enum dicey_error spawn_server_thread(uv_thread_t *const tid, struct thread_args *const args) {
    assert(tid && args && args->server && args->addr.addr);

    struct dicey_server *const server = args->server;

    struct server_ctx *const ctx = dicey_server_get_context(server);
    assert(ctx);

    int uverr = uv_sem_init(&ctx->startup_sem, 0);
    if (uverr) {
        return DICEY_EUV_UNKNOWN;
    }

    uverr = uv_thread_create(tid, &server_thread, args);
    if (uverr) {
        uv_sem_destroy(&ctx->startup_sem);

        return DICEY_EUV_UNKNOWN;
    }

    uv_sem_wait(&ctx->startup_sem);
    uv_sem_destroy(&ctx->startup_sem);

#if DICEY_HAS_PLUGINS
    enum dicey_error err = spawn_dummy_plugin(server);
    if (err) {
        return err;
    }

    err = send_work_test(server);
    if (err) {
        return err;
    }

    err = quit_dummy(server);
    if (err) {
        return err;
    }
#endif

    uverr = uv_thread_join(tid);
    if (uverr) {
        return DICEY_EUV_UNKNOWN;
    }

    return ctx->startup_err;
}

int main(const int argc, char *argv[]) {
    uv_setup_args(argc, argv);

    const char *const progname = argv[0];

    int opt = 0;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case 'v':
            print_logs = true;
            break;

        case '?':
            if (optopt == 'o') {
                fputs("error: -o requires an argument\n", stderr);
            } else {
                fprintf(stderr, "error: unknown option -%c\n", optopt);
            }

            print_help(progname, stderr);
            return EXIT_FAILURE;

        default:
            abort();
        }
    }

    if (argc - optind) {
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    struct server_ctx ctx = { 0 };

    struct timer_state tstate = { 0 };

    enum dicey_error err = dicey_server_new(
        &global_server,
        &(struct dicey_server_args) {
            .on_connect = &on_client_connect, .on_disconnect = &on_client_disconnect, .on_error = &on_client_error,
            .on_request = &on_request_received, .on_startup = &on_startup_done,

#if DICEY_HAS_PLUGINS // this is so ugly
            .on_plugin_event = &on_plugin_event,
#endif
        }
    );

    if (err) {
        fprintf(stderr, "dicey_server_init: %s\n", dicey_error_msg(err));

        goto quit;
    }

    err = registry_fill(dicey_server_get_registry(global_server));
    if (err) {
        fprintf(stderr, "registry_init: %s\n", dicey_error_msg(err));

        goto quit;
    }

#if PIPE_NEEDS_CLEANUP
    err = remove_socket_if_present();
    if (err) {
        fprintf(stderr, "uv_fs_unlink: %s\n", uv_err_name(err));

        goto quit;
    }
#endif

    if (!register_break_hook()) {
        fputs("warning: failed to register break hook. CTRL-C will not clean up the server properly\n", stderr);
    }

    struct dicey_addr addr = { 0 };

    if (!dicey_addr_from_str(&addr, PIPE_NAME)) {
        fprintf(stderr, "error: addr_from failed\n");

        goto quit;
    }

    out("starting Dicey sample server on " PIPE_NAME "...\n");

    // start and register the timer thread
    timer_state_init(&tstate, global_server);

    ctx.hash = dicey_hashtable_new();
    if (!ctx.hash) {
        fprintf(stderr, "error: hashtable_new failed\n");

        goto quit;
    }

    if (dicey_hashtable_set(&ctx.hash, TIMER_STATE_KEY, &tstate, NULL) == DICEY_HASH_SET_FAILED) {
        err = DICEY_ENOMEM;

        fprintf(stderr, "error: hashtable_set failed\n");

        goto quit;
    }

    dicey_server_set_context(global_server, &ctx);

    uv_thread_t tid = { 0 };
    struct thread_args targs = { .server = global_server, .addr = addr };

    err = spawn_server_thread(&tid, &targs);
    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));

        goto quit;
    }

    uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

quit:
    if (ctx.hash) {
        timer_state_deinit(&tstate);

        // remove the only statically allocated key in the hash
        (void) dicey_hashtable_remove(ctx.hash, TIMER_STATE_KEY);

        dicey_hashtable_delete(ctx.hash, &free);
    }

    dicey_server_delete(global_server);

    return err == DICEY_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
