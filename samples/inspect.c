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
#endif

#include <uv.h>

#include <util/dumper.h>
#include <util/getopt.h>
#include <util/packet-dump.h>

#include "dicey_config.h"

#if defined(DICEY_CC_IS_MSVC_LIKE)
#pragma warning(disable : 4200) // borked C11 flex array
#pragma warning(disable : 4996) // strdup
#endif

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

enum path_kind {
    PATH_KIND_OBJECT,
    PATH_KIND_ALIAS,
};

static enum dicey_error check_path_kind(
    struct dicey_client *const client,
    const char *const path,
    enum path_kind *const dest
) {
    assert(client && path && dest);

    const enum dicey_error err = dicey_client_is_path_alias(client, path, DEFAULT_TIMEOUT);

    switch (err) {
    case DICEY_OK:
        *dest = PATH_KIND_ALIAS;
        return DICEY_OK;

    case DICEY_EPATH_NOT_ALIAS:
        *dest = PATH_KIND_OBJECT;
        return DICEY_OK;

    default:
        return err;
    }
}

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

struct path {
    enum path_kind kind;
    char *path, *alias; // alias is only used for PATH_KIND_ALIAS
};

static void path_delete(void *const pentry) {
    if (pentry) {
        const struct path *const path = pentry;
        free(path->path);
        free(path->alias);

        free(pentry);
    }
}

static struct path *path_new(const char *const path) {
    assert(path);

    struct path *const pentry = malloc(sizeof(struct path));
    if (!pentry) {
        return NULL; // memory allocation failed
    }

    char *const pclone = strdup(path);
    if (!pclone) {
        return false; // memory allocation failed
    }

    *pentry = (struct path) {
        .kind = PATH_KIND_OBJECT,
        .path = pclone,
    };

    return pentry;
}

static struct path *alias_new(const char *const target, const char *const link) {
    assert(target && link);

    struct path *const pentry = malloc(sizeof(struct path));
    if (!pentry) {
        return NULL; // memory allocation failed
    }

    char *const path = strdup(target);
    if (!path) {
        free(pentry);

        return NULL; // memory allocation failed
    }

    char *const alias = strdup(link);
    if (!alias) {
        free(path);
        free(pentry);

        return NULL; // memory allocation failed
    }

    *pentry = (struct path) {
        .kind = PATH_KIND_ALIAS,
        .path = path,
        .alias = alias,
    };

    return pentry;
}

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

static enum dicey_error query_objects(struct dicey_client *const client, struct dicey_hashtable **const dest) {
    assert(client && dest && !*dest);

    struct dicey_hashtable *table = NULL;

    struct dicey_packet objs_result = { 0 };
    enum dicey_error err = dicey_client_list_objects(client, &objs_result, DEFAULT_TIMEOUT);
    if (err) {
        return err;
    }

    struct dicey_message msg = { 0 };
    err = dicey_packet_as_message(objs_result, &msg);
    if (err) {
        goto quit;
    }

    struct dicey_list plist = { 0 };
    err = dicey_value_get_array(&msg.value, &plist);
    if (err) {
        goto quit;
    }

    struct dicey_iterator it = dicey_list_iter(&plist);

    struct dicey_value entry = { 0 };
    while (dicey_iterator_next(&it, &entry) == DICEY_OK) {
        const char *path = NULL;
        err = dicey_value_get_path(&entry, &path);
        if (err) {
            goto quit; // failed to get path
        }

        struct path *const pentry = path_new(path);
        if (!pentry) {
            err = DICEY_ENOMEM; // memory allocation failed

            goto quit;
        }

        const enum dicey_hash_set_result res = dicey_hashtable_set(&table, path, pentry, NULL);
        switch (res) {
        case DICEY_HASH_SET_FAILED:
            err = DICEY_ENOMEM; // memory allocation failed

            goto quit;

        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            assert(false); // should never happen, server is broken if this happens
            err = DICEY_EINVAL;

            goto quit;
        }
    }

    *dest = table; // return the table to the caller

quit:
    if (err) {
        dicey_hashtable_delete(table, path_delete);
    } else {
        *dest = table; // return the table to the caller
    }

    dicey_packet_deinit(&objs_result);

    return err;
}

static enum dicey_error query_real_path(struct dicey_client *const client, const char *const path, char **const dest) {
    assert(client && path && dest);

    struct dicey_packet packet = { 0 };
    enum dicey_error err = dicey_client_get_real_path(client, path, &packet, DEFAULT_TIMEOUT);
    if (err) {
        return err; // failed to get real path
    }

    struct dicey_message msg = { 0 };
    err = dicey_packet_as_message(packet, &msg);
    if (err) {
        dicey_packet_deinit(&packet);
        return err; // failed to parse the packet as a message
    }

    struct dicey_errmsg errmsg = { 0 };
    err = dicey_value_get_error(&msg.value, &errmsg);
    if (!err) {
        dicey_packet_deinit(&packet);

        return (enum dicey_error) errmsg.code; // the server returned an error code
    }

    const char *real_path = NULL;
    err = dicey_value_get_path(&msg.value, &real_path);
    if (err) {
        dicey_packet_deinit(&packet);

        return err; // failed to get the real path from the value
    }

    *dest = strdup(real_path);
    dicey_packet_deinit(&packet);

    // return an error if strdup failed
    return *dest ? DICEY_OK : DICEY_ENOMEM;
}

static enum dicey_error query_paths(
    struct dicey_client *const client,
    const char *const target,
    struct dicey_hashtable **const dest
) {
    assert(client && target && dest);

    if (strcmp(target, "all")) {
        enum path_kind kind = PATH_KIND_OBJECT;
        enum dicey_error err = check_path_kind(client, target, &kind);
        if (err) {
            return err; // failed to check path kind
        }

        struct path *path = NULL;

        if (kind == PATH_KIND_OBJECT) {
            path = path_new(target);
        } else {
            char *alias = NULL;
            err = query_real_path(client, target, &alias);
            if (err) {
                return err; // failed to query real path
            }

            assert(alias);

            path = alias_new(target, alias);
            free(alias);
        }

        if (!path) {
            return DICEY_ENOMEM; // memory allocation failed
        }

        const enum dicey_hash_set_result res = dicey_hashtable_set(dest, target, path, NULL);
        if (res == DICEY_HASH_SET_FAILED) {
            free(path);
            return DICEY_ENOMEM; // memory allocation failed
        }

        return DICEY_OK; // successfully added the path
    }

    // handle the "all" case

    struct dicey_hashtable *paths = NULL;

    enum dicey_error err = query_objects(client, &paths);
    if (err) {
        goto quit; // failed to query objects
    }

    struct dicey_packet paths_result = { 0 };
    err = dicey_client_list_paths(client, &paths_result, DEFAULT_TIMEOUT);
    if (err) {
        goto quit; // failed to list paths
    }

    struct dicey_message msg = { 0 };
    err = dicey_packet_as_message(paths_result, &msg);
    if (err) {
        goto quit;
    }

    struct dicey_list plist = { 0 };
    err = dicey_value_get_array(&msg.value, &plist);
    if (err) {
        goto quit;
    }

    struct dicey_iterator it = dicey_list_iter(&plist);
    struct dicey_value entry = { 0 };

    while (dicey_iterator_next(&it, &entry) == DICEY_OK) {
        const char *path = NULL;
        err = dicey_value_get_path(&entry, &path);
        if (err) {
            goto quit; // failed to get path
        }

        // if the path is already in the hashtable, then skip it. Otherwise, it's an alias
        if (dicey_hashtable_contains(paths, path)) {
            continue;
        }

        char *apath = NULL;
        err = query_real_path(client, path, &apath);
        if (err) {
            goto quit; // failed to query real path
        }

        assert(apath);

        struct path *const alias = alias_new(path, apath);
        free(apath); // free the alias string, we don't need it anymore
        if (!alias) {
            err = DICEY_ENOMEM; // memory allocation failed

            goto quit;
        }

        const enum dicey_hash_set_result res = dicey_hashtable_set(&paths, path, alias, NULL);
        switch (res) {
        case DICEY_HASH_SET_FAILED:
            path_delete(apath);
            err = DICEY_ENOMEM; // memory allocation failed

            goto quit;

        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            // this should never happen, server is broken if this happens
            assert(false);
            path_delete(apath);
            err = DICEY_EINVAL;

            goto quit;
        }
    }

    *dest = paths; // return the hashtable to the caller

quit:
    if (err) {
        dicey_hashtable_delete(paths, path_delete);
    }

    dicey_packet_deinit(&paths_result);

    return err;
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
            .on_signal = &on_client_event,
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

    struct dicey_hashtable *paths = NULL;
    err = query_paths(client, args->path, &paths);
    if (err) {
        goto quit;
    }

    assert(paths && dicey_hashtable_size(paths));

    struct dicey_hashtable_iter it = dicey_hashtable_iter_start(paths);
    const char *path = NULL;
    void *value = NULL;
    while (dicey_hashtable_iter_next(&it, &path, &value)) {
        assert(path && value);

        const struct path *const pentry = value;
        if (pentry->kind == PATH_KIND_ALIAS) {
            fprintf(args->output, "alias %s -> %s\n", pentry->path, path);

            continue; // don't inspect aliases
        }

        struct dicey_packet result = { 0 };
        switch (args->op) {
        case OUTPUT_NATIVE:
            err = dicey_client_inspect_path(client, path, &result, DEFAULT_TIMEOUT);
            break;

        case OUTPUT_XML:
            err = dicey_client_inspect_path_as_xml(client, path, &result, DEFAULT_TIMEOUT);
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
    dicey_hashtable_delete(paths, path_delete);
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
