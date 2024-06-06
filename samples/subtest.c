// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

#include "timer.h"

static bool parse_int32(const char *const input, int32_t *const dest) {
    assert(input && dest);

    char *end = NULL;
    const long val = strtol(input, &end, 10);

    if (end == input || *end != '\0' || val < INT32_MIN || val > INT32_MAX) {
        return false;
    }

    *dest = (int32_t) val;

    return true;
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

static enum dicey_error check_success(const struct dicey_packet packet) {
    // attempt extracting an error code, or find errors in the reply
    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(packet, &msg);

    if (!err) {
        struct dicey_errmsg errmsg = { 0 };

        const enum dicey_error as_err_err = dicey_value_get_error(&msg.value, &errmsg);

        switch (as_err_err) {
        case DICEY_OK:
            err = errmsg.code;
            break;

        case DICEY_EVALUE_TYPE_MISMATCH:
            // finally, test that the error message is of the expected typ
            err = dicey_value_is_unit(&msg.value) ? DICEY_OK : DICEY_EBADMSG;

            break;

        default:
            err = as_err_err;

            break;
        }
    }

    return err;
}

static int do_op(const char *const addr, const int32_t value) {
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

    err = dicey_client_connect(client, daddr);
    if (err) {
        dicey_client_delete(client);

        return err;
    }

    err = dicey_client_subscribe_to(
        client,
        TEST_TIMER_PATH,
        (struct dicey_selector) { .trait = TEST_TIMER_TRAIT, .elem = TEST_TIMER_TIMERFIRED_ELEMENT },
        3000
    ); // 3 seconds
    if (err) {
        dicey_client_disconnect(client);
        dicey_client_delete(client);

        return err;
    }

    struct dicey_packet response = { 0 };
    err = dicey_client_exec(
        client,
        TEST_TIMER_PATH,
        (struct dicey_selector) { .trait = TEST_TIMER_TRAIT, .elem = TEST_TIMER_START_ELEMENT },
        (struct dicey_arg) {
            .type = DICEY_TYPE_INT32,
            .i32 = value,
        },
        &response,
        3000
    ); // 3 seconds

    if (err) {
        dicey_client_disconnect(client);
        dicey_client_delete(client);

        return err;
    }

    err = check_success(response);
    dicey_packet_deinit(&response);

    if (err) {
        dicey_client_disconnect(client);
        dicey_client_delete(client);

        return err;
    }

    uv_sleep((value + 1) * 1000); // wait for the timer to fire

    (void) dicey_client_disconnect(client);
    dicey_client_delete(client);

    return err;
}

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...] SOCKET DELAY\n"                                                                            \
    "  -h  print this help message and exit\n"                                                                         \
    "\n"                                                                                                               \
    "DELAY represents the delay in seconds after which the server will raise an event\n"

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    const char *val = NULL;
    char *socket = NULL;

    int opt = 0;

    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case '?':
            fprintf(stderr, "error: unknown option -%c\n", optopt);

            print_help(progname, stderr);
            return EXIT_FAILURE;

        default:
            abort();
        }
    }

    const int npositionals = argc - optind;
    if (npositionals < 2) {
        fputs("error: missing socket or pipe name\n", stderr);
        print_help(progname, stderr);

        return EXIT_FAILURE;
    } else if (npositionals > 2) {
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    socket = argv[optind++];
    val = argv[optind++];

    int32_t delay = 0;
    if (!parse_int32(val, &delay)) {
        fprintf(stderr, "error: invalid delay value: %s\n", val);
        return EXIT_FAILURE;
    }

    if (delay < 0) {
        fprintf(stderr, "error: delay must be non-negative\n");
        return EXIT_FAILURE;
    }

    enum dicey_error err = do_op(socket, delay);
    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
