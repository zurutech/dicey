/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
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

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/getopt.h"
#include "util/packet-dump.h"
#include "util/packet-json.h"
#include "util/packet-xml.h"

#include "sval.h"

enum reqtime_mode {
    REQTIME_NONE,
    REQTIME_SHOW,
};

enum sval_op {
    SVAL_SET,
    SVAL_GET,
};

struct sval_estimate {
    double nreq;
    const char *base;
};

static struct sval_estimate estimate(const int64_t reqtime_us) {
    const double req_s = 1000000. / reqtime_us;

    if (req_s > 1000000000.) {
        return (struct sval_estimate) { .nreq = req_s / 1000000000., .base = "G" };
    }

    if (req_s > 1000000.) {
        return (struct sval_estimate) { .nreq = req_s / 1000000., .base = "M" };
    }

    if (req_s > 1000.) {
        return (struct sval_estimate) { .nreq = req_s / 1000., .base = "k" };
    }

    return (struct sval_estimate) { .nreq = req_s, .base = "" };
}

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

static void on_client_event(struct dicey_client *const client, void *const ctx, const struct dicey_packet packet) {
    (void) client;
    (void) ctx;

    assert(client);

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_printlnf(&dumper, "received event:");
    util_dumper_dump_packet(&dumper, packet);
}

static int do_op(const char *const addr, const char *const value, const enum reqtime_mode show_time) {
    const enum sval_op op = value ? SVAL_SET : SVAL_GET;

    struct dicey_client *client = NULL;

    enum dicey_error err = dicey_client_new(
        &client,
        &(struct dicey_client_args) {
            .inspect_func = &inspector,
            .on_event = &on_client_event,
        }
    );

    if (err) {
        return err;
    }

    struct dicey_addr daddr = { 0 };
    if (!dicey_addr_from_str(&daddr, addr)) {
        dicey_client_delete(client);

        return DICEY_ENOMEM;
    }

    struct dicey_packet packet = { 0 };
    switch (op) {
    case SVAL_SET:
        err = dicey_packet_message(
            &packet, 0, DICEY_OP_SET, SVAL_PATH, SVAL_SEL, (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = value }
        );
        break;

    case SVAL_GET:
        err = dicey_packet_message(&packet, 0, DICEY_OP_GET, SVAL_PATH, SVAL_SEL, (struct dicey_arg) { 0 });
        break;

    default:
        abort(); // unreachable
    }

    if (err) {
        dicey_client_delete(client);
        return err;
    }

    err = dicey_client_connect(client, daddr);
    if (err) {
        dicey_client_delete(client);
        dicey_packet_deinit(&packet);

        return err;
    }

    uv_timespec64_t start = { 0 }, end = { 0 };
    uv_clock_gettime(UV_CLOCK_MONOTONIC, &start);

    err = dicey_client_request(client, packet, &packet, 3000); // 3 seconds
    if (!err) {
        struct dicey_message msg = { 0 };
        if (dicey_packet_as_message(packet, &msg)) {
            assert(false);
        } else {
            struct dicey_errmsg errmsg = { 0 };
            if (dicey_value_get_error(&msg.value, &errmsg) == DICEY_OK) {
                fprintf(stderr, "error: %" PRIu16 " %s\n", errmsg.code, errmsg.message);
            } else {
                switch (op) {
                case SVAL_SET:
                    {
                        if (!dicey_value_is_unit(&msg.value)) {
                            fputs("error: received malformed reply\n", stderr);
                        }

                        break;
                    }

                case SVAL_GET:
                    {
                        const char *str = NULL;

                        if (dicey_value_get_str(&msg.value, &str) == DICEY_OK) {
                            assert(str);

                            printf(SVAL_PATH "#" SVAL_TRAIT ":" SVAL_PROP " = \"%s\"\n", *str ? str : "(empty)");
                        } else {
                            fputs("error: received malformed reply\n", stderr);
                        }

                        break;
                    }

                default:
                    abort(); // unreachable
                }
            }
        }
    }

    uv_clock_gettime(UV_CLOCK_MONOTONIC, &end);

    if (show_time == REQTIME_SHOW) {
        const int64_t reqtime_us = (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_nsec - start.tv_nsec) / 1000L;
        const struct sval_estimate est = estimate(reqtime_us);

        printf("reqtime: %" PRIu64 "us (%2f %sreq/s)\n", reqtime_us, est.nreq, est.base);
    }

    (void) dicey_client_disconnect(client);
    dicey_client_delete(client);
    dicey_packet_deinit(&packet);

    return err;
}

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...] SOCKET [VALUE]\n"                                                                          \
    "  -h  print this help message and exit\n"                                                                         \
    "  -t  show request time\n"                                                                                        \
    "\n"                                                                                                               \
    "If VALUE is not specified, a GET is performed, otherwise VALUE is used as an argument to SET.\n"

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    const char *val = NULL;
    char *socket = NULL;
    enum reqtime_mode show_time = REQTIME_NONE;

    int opt = 0;

    while ((opt = getopt(argc, argv, "ht")) != -1) {
        switch (opt) {
        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case 't':
            show_time = REQTIME_SHOW;
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

    switch (argc - optind) {
    case 0:
        fputs("error: missing socket or pipe name\n", stderr);
        print_help(progname, stderr);

        return EXIT_FAILURE;

    case 2:
        val = argv[optind + 1];

        // fallthrough

    case 1:
        socket = argv[optind];
        break;

    default:
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    enum dicey_error err = do_op(socket, val, show_time);
    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
