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
#define _XOPEN_SOURCE 700

#if defined(_MSC_VER)
#pragma warning(disable : 4200) // borked C11 flex array
#pragma warning(disable : 4996) // strdup
#endif

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
#endif

#include <uv.h>

#include <util/dumper.h>
#include <util/getopt.h>
#include <util/packet-dump.h>

#define DEFAULT_TIMEOUT 3000U // 3 seconds

enum output_mode {
    OUTPUT_NATIVE,
    OUTPUT_XML,
};

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

static void print_introspect_data(
    FILE *const out,
    const enum verbosity verbosity,
    const struct dicey_message *const msg
) {
    assert(msg && out);

    struct util_dumper dumper = util_dumper_for(out);

    struct dicey_list trait_list = { 0 };

    enum dicey_error err = dicey_value_get_array(&msg->value, &trait_list);
    if (err) {
        fprintf(stderr, "error: failed to get trait list: %s\n", dicey_error_msg(err));

        return;
    }

    util_dumper_printlnf(&dumper, "object %s", msg->path);

    const bool quiet = verbosity == NO_VERBOSE_DUMP;

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

        // skip introspection trait if not verbose
        if (quiet && !strcmp(name, DICEY_INTROSPECTION_TRAIT_NAME)) {
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

static ptrdiff_t query_paths(struct dicey_client *const client, const char *const target, char **const dest) {
    assert(client && target && dest);

    if (strcmp(target, "all")) {
        // return just target as a list
        *dest = strdup(target);
        if (!*dest) {
            return DICEY_ENOMEM;
        }

        return (ptrdiff_t) strlen(target) + 1;
    }

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

struct inspect_args {
    const char *addr;
    const char *path;
    enum output_mode op;
    FILE *output;
    bool verbose;
};

static int do_op(const struct inspect_args *const args) {
    assert(args);

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
    if (!dicey_addr_from_str(&daddr, args->addr)) {
        dicey_client_delete(client);

        return DICEY_ENOMEM;
    }

    err = dicey_client_connect(client, daddr);
    if (err) {
        dicey_client_delete(client);

        return err;
    }

    char *path_list = NULL;
    const ptrdiff_t res = query_paths(client, args->path, &path_list);
    if (res < 0) {
        err = (enum dicey_error) res;

        goto quit;
    }

    assert(path_list);

    const size_t nbytes = (size_t) res;
    const char *const end = path_list + nbytes;

    for (const char *ptr = path_list; !err && ptr < end; ptr += strlen(ptr) + 1) {
        struct dicey_packet result = { 0 };
        switch (args->op) {
        case OUTPUT_NATIVE:
            err = dicey_client_inspect_path(client, ptr, &result, DEFAULT_TIMEOUT);
            break;

        case OUTPUT_XML:
            err = dicey_client_inspect_path_as_xml(client, ptr, &result, DEFAULT_TIMEOUT);
            break;

        default:
            abort(); // unreachable
        }

        struct dicey_message msg = { 0 };
        err = dicey_packet_as_message(result, &msg);
        if (!err) {
            if (args->op == OUTPUT_XML) {
                const char *xml = NULL;

                err = dicey_value_get_str(&msg.value, &xml);
                if (err) {
                    goto quit;
                }

                fprintf(args->output, "%s\n", xml);
            } else {
                print_introspect_data(args->output, args->verbose ? VERBOSE_DUMP : NO_VERBOSE_DUMP, &msg);
            }
        }

        dicey_packet_deinit(&result);
    }

quit:
    free(path_list);
    fclose(args->output);
    (void) dicey_client_disconnect(client);
    dicey_client_delete(client);

    if (err) {
        return err;
    }

    return err;
}

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...] SOCKET PATH\n"                                                                             \
    "  -h      print this help message and exit\n"                                                                     \
    "  -o FILE outputs to FILE instead of stdout\n"                                                                    \
    "  -x      request XML\n"                                                                                          \
    "  -v      inspect hidden objects and traits (verbose)\n"                                                          \
    "If PATH is `all`, all objects in the server will be inspected. Note that `all` is not compatible with `-x`.\n"    \
    "\n"

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    const char *const progname = argv[0];

    struct inspect_args args = { .output = stdout };

    int opt = 0;

    while ((opt = getopt(argc, argv, "ho:vx")) != -1) {
        switch (opt) {
        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case 'o':
            args.output = fopen(optarg, "w");
            if (!args.output) {
                perror("error: failed to open output file: ");
                return EXIT_FAILURE;
            }

            break;

        case 'v':
            args.verbose = true;
            break;

        case 'x':
            args.op = OUTPUT_XML;
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
        fputs("error: missing socket and path to inspect\n", stderr);
        print_help(progname, stderr);

        return EXIT_FAILURE;

    case 2:
        args.path = argv[optind + 1];
        args.addr = argv[optind];

        break;

    case 1:
        fputs("error: missing path to inspect\n", stderr);
        print_help(progname, stderr);

        return EXIT_FAILURE;

    default:
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    if (args.op == OUTPUT_XML && !strcmp(args.path, "all")) {
        fputs("error: `all` is not compatible with XML output\n", stderr);

        return EXIT_FAILURE;
    }

#if defined(DICEY_IS_WINDOWS)
    if (args.output == stdout) {
        SetConsoleOutputCP(CP_UTF8);
    }
#endif

    enum dicey_error err = do_op(&args);
    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    // don't bother fclosing the file, we're exiting anyway

    return EXIT_SUCCESS;
}
