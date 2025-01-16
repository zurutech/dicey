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
        fprintf(stderr, "error[child]: [%s] %s\n", dicey_error_msg(event.error.err), event.error.msg);

        if (dicey_client_is_running(client) && dicey_client_disconnect(client) != DICEY_OK) {
            fprintf(stderr, "error[child]: failed to stop client\n");

            exit(EXIT_FAILURE);
        }
    }
}

static void on_client_event(struct dicey_client *const client, void *const ctx, struct dicey_packet *const packet) {
    (void) client;
    (void) ctx;

    assert(client && packet);

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_printlnf(&dumper, "info[child]: received event = ");
    util_dumper_dump_packet(&dumper, *packet);
}

static void on_quit_requested(void) {
    puts("info[child]: server asked us to quit");

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

    struct dicey_value_builder *resp = NULL;
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

    err = dicey_value_builder_set(resp, arg);
    assert(!err);

    err = dicey_plugin_work_response_done(ctx);
    assert(!err);
}

#if defined(DICEY_IS_UNIX)
#include <signal.h>

// it's pointless to catch signals on Windows anyway

void dummy_signal_handler(const int signum) {
    printf("info[child]: signal %d received, quitting", signum);

    signal(signum, SIG_DFL);
    raise(signum);
}

#endif

int main(const int argc, const char *const argv[]) {
#if defined(DICEY_IS_UNIX)
    signal(SIGINT, &dummy_signal_handler);
    signal(SIGTERM, &dummy_signal_handler);
#endif

    // uv_sleep(7000);

    const int uverr = uv_sem_init(&halt_sem, 0);
    if (uverr) {
        fprintf(stderr, "error[child]: failed to initialise semaphore: %s\n", uv_strerror(uverr));

        return EXIT_FAILURE;
    }

    struct dicey_plugin *plugin = NULL;

    puts("info[child]: dummy plugin launched");

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
        fprintf(stderr, "error[child]: failed to initialise plugin: %s\n", dicey_error_msg(err));

        return EXIT_FAILURE;
    }

    puts("info[child]: dummy plugin initialised");

    uv_sem_wait(&halt_sem);

    puts("info[child]: dummy plugin quitting");

    err = dicey_plugin_finish(plugin);

    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
