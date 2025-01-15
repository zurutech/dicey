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

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1
#define _XOPEN_SOURCE 700

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#if defined(DICEY_IS_WINDOWS)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <uv.h>

#include <util/dumper.h>
#include <util/getopt.h>
#include <util/packet-dump.h>

#include "dummy_plugin.h"

#include "dicey_config.h"

#if defined(DICEY_CC_IS_MSVC_LIKE)
#pragma warning(disable : 4200) // borked C11 flex array
#pragma warning(disable : 4996) // strdup
#endif

#define DEFAULT_TIMEOUT 3000U // 3 seconds

static uv_sem_t halt_sem = { 0 };

static void inspector(struct dicey_client *const client, void *const ctx, struct dicey_client_event event) {
    (void) client;
    (void) ctx;

    assert(client);

    if (event.type == DICEY_CLIENT_EVENT_ERROR) {
        fprintf(stderr, "error: [%s] %s\n", dicey_error_msg(event.error.err), event.error.msg);

        if (dicey_client_is_running(client) && dicey_client_disconnect(client) != DICEY_OK) {
            fprintf(stderr, "error: failed to stop client\n");

            exit(EXIT_FAILURE);
        }
    }
}

static void on_client_event(struct dicey_client *const client, void *const ctx, struct dicey_packet *const packet) {
    (void) client;
    (void) ctx;

    assert(client && packet);

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_printlnf(&dumper, "received event:");
    util_dumper_dump_packet(&dumper, *packet);
}

enum print_flags {
    PRINT_START_BAR = 1 << 0,
    PRINT_CONTINUE_BAR = 1 << 1,
};

static enum dicey_error dump_element(
    struct util_dumper *const dumper,
    const uint8_t print_flags,
    const struct dicey_value *const elem_entry
) {
    assert(dumper && elem_entry);

    struct dicey_pair element = { 0 };
    enum dicey_error err = dicey_value_get_pair(elem_entry, &element);
    if (err) {
        fprintf(stderr, "error: failed to get element: %s\n", dicey_error_msg(err));

        return err;
    }

    const char *name = NULL;

    err = dicey_value_get_str(&element.first, &name);
    if (err) {
        fprintf(stderr, "error: failed to get element name: %s\n", dicey_error_msg(err));

        return err;
    }

    struct dicey_list elem_data = { 0 };
    err = dicey_value_get_tuple(&element.second, &elem_data);
    if (err) {
        fprintf(stderr, "error: failed to get element data: %s\n", dicey_error_msg(err));

        return err;
    }

    struct dicey_iterator dit = dicey_list_iter(&elem_data);

    struct dicey_value entry = { 0 };

    uint8_t kind_byte = 0;
    const char *signature = NULL;
    bool readonly = false;

    err = dicey_iterator_next(&dit, &entry);
    if (err) {
        fprintf(stderr, "error: failed to get trait data entry: %s\n", dicey_error_msg(err));

        return err;
    }

    err = dicey_value_get_byte(&entry, &kind_byte);
    if (err) {
        fprintf(stderr, "error: failed to get trait data kind: %s\n", dicey_error_msg(err));

        return err;
    }

    err = dicey_iterator_next(&dit, &entry);
    if (err) {
        fprintf(stderr, "error: failed to get trait data entry: %s\n", dicey_error_msg(err));

        return err;
    }

    err = dicey_value_get_str(&entry, &signature);
    if (err) {
        fprintf(stderr, "error: failed to get trait data signature: %s\n", dicey_error_msg(err));

        return err;
    }

    if (dicey_iterator_has_next(dit)) {
        err = dicey_iterator_next(&dit, &entry);
        if (err) {
            fprintf(stderr, "error: failed to get trait data entry: %s\n", dicey_error_msg(err));

            return err;
        }

        err = dicey_value_get_bool(&entry, &readonly);
        if (err) {
            fprintf(stderr, "error: failed to get trait data readonly: %s\n", dicey_error_msg(err));

            return err;
        }
    }

    const bool start_bar = print_flags & PRINT_START_BAR;
    const bool continue_bar = print_flags & PRINT_CONTINUE_BAR;

    const enum dicey_element_type kind = (enum dicey_element_type) kind_byte;

    const char *const rotag = kind == DICEY_ELEMENT_TYPE_PROPERTY ? (readonly ? " (ro)" : " (rw)") : "";

    const char *const line_start = start_bar ? "│" : " ";
    const char *item_start = continue_bar ? "├" : "└";

    // print the dash if this is not the last
    util_dumper_printlnf(
        dumper, "%s   %s── %s %s: %s%s", line_start, item_start, dicey_element_type_name(kind), name, signature, rotag
    );

    return DICEY_OK;
}

enum verbosity {
    NO_VERBOSE_DUMP,
    VERBOSE_DUMP,
};

static void print_introspect_data(const struct dicey_message *const msg) {
    assert(msg);

    struct util_dumper dumper = util_dumper_for(stdout);

    struct dicey_list trait_list = { 0 };

    enum dicey_error err = dicey_value_get_array(&msg->value, &trait_list);
    if (err) {
        fprintf(stderr, "error: failed to get trait list: %s\n", dicey_error_msg(err));

        return;
    }

    util_dumper_printlnf(&dumper, "object %s", msg->path);

    struct dicey_value trait = { 0 };
    struct dicey_iterator it = dicey_list_iter(&trait_list);
    while (dicey_iterator_has_next(it)) {
        err = dicey_iterator_next(&it, &trait);
        if (err) {
            fprintf(stderr, "error: failed to get trait: %s\n", dicey_error_msg(err));

            return;
        }

        const bool last = !dicey_iterator_has_next(it);

        struct dicey_pair tentry = { 0 };
        err = dicey_value_get_pair(&trait, &tentry);
        if (err) {
            fprintf(stderr, "error: failed to get trait entry: %s\n", dicey_error_msg(err));

            return;
        }

        const char *name = NULL;
        err = dicey_value_get_str(&tentry.first, &name);
        if (err) {
            fprintf(stderr, "error: failed to get trait name: %s\n", dicey_error_msg(err));

            return;
        }

        // skip introspection trait
        if (!strcmp(name, DICEY_INTROSPECTION_TRAIT_NAME)) {
            continue;
        }

        util_dumper_printlnf(&dumper, (last ? "└── %s" : "├── %s"), name);

        struct dicey_list trait_data = { 0 };
        err = dicey_value_get_array(&tentry.second, &trait_data);
        if (err) {
            fprintf(stderr, "error: failed to get trait data: %s\n", dicey_error_msg(err));

            return;
        }

        struct dicey_value entry = { 0 };
        struct dicey_iterator dit = dicey_list_iter(&trait_data);

        while (dicey_iterator_has_next(dit)) {
            err = dicey_iterator_next(&dit, &entry);
            if (err) {
                fprintf(stderr, "error: failed to get trait data entry: %s\n", dicey_error_msg(err));

                return;
            }

            const bool last_element = !dicey_iterator_has_next(dit);

            uint8_t print_flags = 0;

            if (!last) {
                print_flags |= PRINT_START_BAR;
            }

            if (!last_element) {
                print_flags |= PRINT_CONTINUE_BAR;
            }

            err = dump_element(&dumper, print_flags, &entry);
            if (err) {
                return;
            }
        }
    }
}

static ptrdiff_t query_paths(struct dicey_client *const client, char **const dest) {
    assert(client && dest);

    struct dicey_packet result = { 0 };
    enum dicey_error err = dicey_client_list_objects(client, &result, DEFAULT_TIMEOUT);
    if (err) {
        return err;
    }

    size_t needed = DICEY_OK;

    struct dicey_message msg = { 0 };
    err = dicey_packet_as_message(result, &msg);
    if (err) {
        goto quit;
    }

    struct dicey_list paths = { 0 };
    err = dicey_value_get_array(&msg.value, &paths);
    if (err) {
        goto quit;
    }

    struct dicey_iterator it = dicey_list_iter(&paths);

    while (dicey_iterator_has_next(it)) {
        struct dicey_value entry = { 0 };
        err = dicey_iterator_next(&it, &entry);
        if (err) {
            goto quit;
        }

        const char *path = NULL;
        err = dicey_value_get_path(&entry, &path);
        if (err) {
            goto quit;
        }

        needed += strlen(path) + 1;
    }

    *dest = calloc(needed, sizeof *dest);
    if (!*dest) {
        err = DICEY_ENOMEM;

        goto quit;
    }

    char *ptr = *dest;
    it = dicey_list_iter(&paths);

    while (dicey_iterator_has_next(it)) {
        struct dicey_value entry = { 0 };
        err = dicey_iterator_next(&it, &entry);
        if (err) {
            goto quit;
        }

        const char *path = NULL;
        err = dicey_value_get_path(&entry, &path);
        if (err) {
            goto quit;
        }

        // also copy the null byte
        const size_t nbytes = strlen(path) + 1;

        memcpy(ptr, path, nbytes);

        ptr += nbytes;
    }

quit:
    if (err) {
        free(*dest);
    }

    dicey_packet_deinit(&result);

    return err ? err : (ptrdiff_t) needed;
}

static enum dicey_error do_list(struct dicey_client *const client) {
    assert(client);

    enum dicey_error err = DICEY_OK;
    char *path_list = NULL;
    const ptrdiff_t res = query_paths(client, &path_list);
    if (res < 0) {
        err = (enum dicey_error) res;

        goto quit;
    }

    assert(path_list);

    const size_t nbytes = (size_t) res;
    const char *const end = path_list + nbytes;

    for (const char *ptr = path_list; !err && ptr < end; ptr += strlen(ptr) + 1) {
        struct dicey_packet result = { 0 };

        err = dicey_client_inspect_path(client, ptr, &result, DEFAULT_TIMEOUT);
        assert(!err);

        struct dicey_message msg = { 0 };
        err = dicey_packet_as_message(result, &msg);
        if (!err) {
            print_introspect_data(&msg);
        }

        dicey_packet_deinit(&result);
    }

quit:
    free(path_list);

    if (err) {
        return err;
    }

    return err;
}

static void on_quit_requested(void) {
    uv_sem_post(&halt_sem);
}

static enum dicey_error dummy_operation(struct dicey_value *const value, double *const result) {
    assert(value && result);

    struct dicey_pair pair = { 0 };
    enum dicey_error err = dicey_value_get_pair(value, &pair);
    if (err) {
        return err;
    }

    double a = 0, b = 0;

    err = dicey_value_get_float(&pair.first, &a);
    if (err) {
        return err;
    }

    err = dicey_value_get_float(&pair.second, &b);
    if (err) {
        return err;
    }

    *result = a * b;

    return DICEY_OK;
}

static void on_work_request(struct dicey_plugin_work_ctx *const ctx, struct dicey_value *const value) {
    assert(ctx && value);

    // this function asserts because we're not expecting any errors here (and this is a dummy program)

    double result = 0;
    const enum dicey_error operr = dummy_operation(value, &result);

    struct dicey_value_builder resp = { 0 };
    enum dicey_error err = dicey_plugin_work_response_start(ctx, &resp);
    assert(!err);

    struct dicey_arg arg = { 0 };

    if (operr) {
        arg = (struct dicey_arg) {
            .type = DICEY_TYPE_ERROR,
            .error = {
                .code = operr,
                .message = dicey_error_msg(operr),
            },
        };
    } else {
        arg = (struct dicey_arg) {
            .type = DICEY_TYPE_FLOAT,
            .floating = result,
        };
    }

    err = dicey_value_builder_set(&resp, arg);
    assert(!err);

    err = dicey_plugin_work_response_done(ctx);
    assert(!err);
}

#if defined(DICEY_IS_UNIX)
#include <signal.h>

// it's pointless to catch signals on Windows anyway

void dummy_signal_handler(const int signum) {
    printf("info: signal %d received, quitting", signum);

    signal(signum, SIG_DFL);
    raise(signum);
}

#endif

int main(const int argc, const char *const argv[]) {
#if defined(DICEY_IS_UNIX)
    signal(SIGINT, &dummy_signal_handler);
    signal(SIGTERM, &dummy_signal_handler);
#endif

    // sleep(7);

    const int uverr = uv_sem_init(&halt_sem, 0);
    if (uverr) {
        fprintf(stderr, "error: failed to initialise semaphore: %s\n", uv_strerror(uverr));

        return EXIT_FAILURE;
    }

    struct dicey_plugin *plugin = NULL;

    puts("info: dummy plugin launched");

    enum dicey_error err = dicey_plugin_init(argc, argv, &plugin, &(struct dicey_plugin_args) {
        .cargs = {
            .on_signal = &on_client_event,
            .inspect_func = &inspector,
        },

        .name = DUMMY_PLUGIN_NAME,
        .on_quit = &on_quit_requested,
        .on_work_received = &on_work_request,
    });

    if (err) {
        fprintf(stderr, "error: failed to initialise plugin: %s\n", dicey_error_msg(err));

        return EXIT_FAILURE;
    }

    puts("info: dummy plugin initialised");

    err = do_list(dicey_plugin_get_client(plugin));
    if (err) {
        fprintf(stderr, "error: failed to list objects: %s\n", dicey_error_msg(err));
    }

    uv_sem_wait(&halt_sem);

    puts("info: dummy plugin quitting");

    err = dicey_plugin_finish(plugin);

    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
