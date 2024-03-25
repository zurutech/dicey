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
#define PIPE_NAME "\0/tmp/.uvsock"
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

static enum dicey_error registry_fill(struct dicey_registry *const registry) {
    assert(registry);

    enum dicey_error err = dicey_registry_add_trait(
        registry,
        DUMMY_TRAIT,
        DUMMY_ELEMENT,
        (struct dicey_element) { .type = DICEY_ELEMENT_TYPE_PROPERTY, .signature = DUMMY_SIGNATURE },
        NULL
    );

    if (err) {
        return err;
    }

    err = dicey_registry_add_trait(
        registry,
        SVAL_TRAIT,
        SVAL_PROP,
        (struct dicey_element) { .type = DICEY_ELEMENT_TYPE_PROPERTY, .signature = SVAL_SIG },
        NULL
    );

    if (err) {
        return err;
    }

    err = dicey_registry_add_trait(
        registry,
        SELF_TRAIT,
        HALT_ELEMENT,
        (struct dicey_element) { .type = DICEY_ELEMENT_TYPE_OPERATION, .signature = HALT_SIGNATURE },
        NULL
    );

    if (err) {
        return err;
    }

    err = dicey_registry_add_object(registry, DUMMY_PATH, DUMMY_TRAIT, NULL);
    if (err) {
        return err;
    }

    err = dicey_registry_add_object(registry, SVAL_PATH, SVAL_TRAIT, NULL);
    if (err) {
        return err;
    }

    err = dicey_registry_add_object(registry, SELF_PATH, SELF_TRAIT, NULL);
    if (err) {
        return err;
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

static enum dicey_error on_sval_req(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const struct dicey_message req
) {
    assert(server && cln);

    switch (req.type) {
    case DICEY_OP_GET:
        {
            const char *const sval = dicey_server_get_context(server);

            return send_reply(
                server,
                cln,
                seq,
                req.path,
                req.selector,
                (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = sval ? sval : "" }
            );
        }

    case DICEY_OP_SET:
        {
            const char *str = NULL;
            const enum dicey_error err = dicey_value_get_str(&req.value, &str);
            if (err) {
                (void) send_reply(server, cln, seq, req.path, req.selector, (struct dicey_arg) { .type = DICEY_TYPE_ERROR, .error = {
                    .code = err,
                    .message = dicey_error_msg(err),
                } });

                return err;
            }

            char *const sval = strdup(str);
            if (!sval) {
                return DICEY_ENOMEM;
            }

            free(dicey_server_set_context(server, sval));

            return send_reply(server, cln, seq, req.path, req.selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
        }

    default:
        abort();
    }
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

    if (!strcmp(msg.path, DUMMY_PATH) && !strcmp(msg.selector.trait, DUMMY_TRAIT) &&
        !strcmp(msg.selector.elem, DUMMY_ELEMENT)) {
        err = send_reply(
            server, cln, seq, msg.path, msg.selector, (struct dicey_arg) { .type = DICEY_TYPE_BOOL, .boolean = true }
        );
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }
    } else if (!strcmp(msg.path, SVAL_PATH) && !strcmp(msg.selector.trait, SVAL_TRAIT) && !strcmp(msg.selector.elem, SVAL_PROP)) {
        err = on_sval_req(server, cln, seq, msg);
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }
    } else if (!strcmp(msg.path, SELF_PATH) && !strcmp(msg.selector.trait, SELF_TRAIT) && !strcmp(msg.selector.elem, HALT_ELEMENT)) {
        err = send_reply(server, cln, seq, msg.path, msg.selector, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }

        dicey_server_stop(server);
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
#endif

    int main(void) {
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

        err = dicey_server_start(global_server, PIPE_NAME, sizeof PIPE_NAME - 1U);
        if (err) {
            fprintf(stderr, "dicey_server_start: %s\n", dicey_error_name(err));

            goto quit;
        }

        uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

quit:
        // free any dummy string
        free(dicey_server_get_context(global_server));

        dicey_server_delete(global_server);

        return err == DICEY_OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
