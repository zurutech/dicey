// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _CRT_NONSTDC_NO_DEPRECATE 1
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
#include <util/packet-dump.h>

#include "sval.h"

#if defined(__linux__) || defined(_WIN32)
#define PIPE_NEEDS_CLEANUP false
#if defined(_WIN32)
#define PIPE_NAME "\\\\.\\pipe\\uvsock"
#else
#define PIPE_NAME "@/tmp/.uvsock"
#endif
#else
#define PIPE_NEEDS_CLEANUP true
#define PIPE_NAME "/tmp/.uvsock"
#endif

#define DUMMY_PATH "/foo/bar"
#define DUMMY_TRAIT "a.street.trait.named.Bob"
#define DUMMY_ELEMENT "Bobbable"
#define DUMMY_SIGNATURE "([b]bfeec${sv})"

#define SELF_PATH "/dicey/sample_server"
#define SELF_TRAIT "dicey.sample.Server"
#define HALT_ELEMENT "Halt"
#define HALT_SIGNATURE "u"

#define ECHO_PATH "/dicey/test/echo"
#define ECHO_TRAIT "dicey.test.Echo"
#define ECHO_ECHO_ELEMENT "Echo"
#define ECHO_ECHO_SIGNATURE "v"

#define TEST_MGR_PATH "/dicey/test/manager"
#define TEST_MGR_TRAIT "dicey.test.Manager"
#define TEST_MGR_ADD_ELEMENT "Add"
#define TEST_MGR_ADD_SIGNATURE "s"
#define TEST_MGR_DEL_ELEMENT "Delete"
#define TEST_MGR_DEL_SIGNATURE "@"

#define TEST_OBJ_PATH_BASE "/dicey/test/object/"
#define TEST_OBJ_PATH_FMT TEST_OBJ_PATH_BASE "%zu"
#define TEST_OBJ_TRAIT "dicey.test.Object"
#define TEST_OBJ_NAME_ELEMENT "Name"
#define TEST_OBJ_NAME_SIGNATURE "s"

static struct dicey_server *global_server = NULL;

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static BOOL quit_server(_In_ const DWORD dwCtrlType) {
    (void) dwCtrlType;

    if (global_server) {
        const enum dicey_error err = dicey_server_stop(global_server);
        assert(!err);
        (void) err;
    }

    return TRUE;
}

static bool register_break_hook(void) {
    return SetConsoleCtrlHandler(&quit_server, TRUE);
}

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))

#include <unistd.h>

static void quit_server(const int sig) {
    (void) sig;

    if (global_server) {
        const enum dicey_error err = dicey_server_stop(global_server);
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
                    .name = DUMMY_ELEMENT,
                    .signature = DUMMY_SIGNATURE,
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
};

struct test_object {
    const char *path;
    const char *const *traits;
};

static const struct test_object test_objects[] = {
    {.path = DUMMY_PATH,     .traits = (const char *[]) { DUMMY_TRAIT, NULL }   },
    { .path = SVAL_PATH,     .traits = (const char *[]) { SVAL_TRAIT, NULL }    },
    { .path = SELF_PATH,     .traits = (const char *[]) { SELF_TRAIT, NULL }    },
    { .path = ECHO_PATH,     .traits = (const char *[]) { ECHO_TRAIT, NULL }    },
    { .path = TEST_MGR_PATH, .traits = (const char *[]) { TEST_MGR_TRAIT, NULL }},
};

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
                (struct dicey_element
                ) { .type = element->type, .signature = element->signature, .readonly = element->readonly }
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

    printf("info: client %zu connected\n", id);

    *user_data = NULL;

    return true;
}

static void on_client_disconnect(struct dicey_server *const server, const struct dicey_client_info *const cln) {
    (void) server;

    printf("info: client %zu disconnected\n", cln->id);
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

    err = dicey_server_send(server, cln->id, packet);
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

    return dicey_server_send(server, cln->id, fixed);
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
        server, cln, seq, req->path, req->selector, (struct dicey_arg) { .type = DICEY_TYPE_PATH, .str = str_buffer }
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

static void on_request_received(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    struct dicey_packet packet
) {
    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(packet, &msg);
    if (err) {
        fprintf(stderr, "error: malformed message: %s\n", dicey_error_name(err));
        return;
    }

    printf(
        "info: received request #%" PRIu32 " from client %zu for `%s#%s:%s`\n",
        seq,
        cln->id,
        msg.path,
        msg.selector.trait,
        msg.selector.elem
    );

    struct util_dumper dumper = util_dumper_for(stdout);

    util_dumper_dump_packet(&dumper, packet);

    if (matches_elem(&msg, DUMMY_PATH, DUMMY_TRAIT, DUMMY_ELEMENT)) {
        err = send_reply(
            server, cln, seq, msg.path, msg.selector, (struct dicey_arg) { .type = DICEY_TYPE_BOOL, .boolean = true }
        );
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }
    } else if (matches_elem(&msg, SVAL_PATH, SVAL_TRAIT, SVAL_PROP)) {
        err = on_sval_req(server, cln, seq, &msg);
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }
    } else if (matches_elem(&msg, SELF_PATH, SELF_TRAIT, HALT_ELEMENT)) {
        err = send_reply(server, cln, seq, msg.path, msg.selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
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
    }

    // this function receives a copy of the packet that must be freed
    dicey_packet_deinit(&packet);
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

int main(void) {
    struct dicey_hashtable **ctx = NULL;

    enum dicey_error err = dicey_server_new(
        &global_server,
        &(struct dicey_server_args) {
            .on_connect = &on_client_connect,
            .on_disconnect = &on_client_disconnect,
            .on_error = &on_client_error,
            .on_request = &on_request_received,
        }
    );

    if (err) {
        fprintf(stderr, "dicey_server_init: %s\n", dicey_error_name(err));

        goto quit;
    }

    err = registry_fill(dicey_server_get_registry(global_server));
    if (err) {
        fprintf(stderr, "registry_init: %s\n", dicey_error_name(err));

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

    puts("starting Dicey sample server on " PIPE_NAME "...");

    ctx = malloc(sizeof *ctx);
    if (!ctx) {
        fprintf(stderr, "error: malloc failed\n");

        goto quit;
    }

    *ctx = dicey_hashtable_new();
    if (!*ctx) {
        fprintf(stderr, "error: hashtable_new failed\n");

        goto quit;
    }

    dicey_server_set_context(global_server, ctx);

    err = dicey_server_start(global_server, addr);
    if (err) {
        fprintf(stderr, "dicey_server_start: %s\n", dicey_error_name(err));

        goto quit;
    }

    uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

quit:
    if (ctx) {
        dicey_hashtable_delete(*ctx, &free);

        free(ctx);
    }

    dicey_server_delete(global_server);

    return err == DICEY_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
