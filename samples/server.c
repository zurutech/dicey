// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

    err = dicey_registry_add_object(registry, DUMMY_PATH, DUMMY_TRAIT, NULL);
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

static void on_request_received(
    struct dicey_server *const server,
    const struct dicey_client_info *const cln,
    const uint32_t seq,
    const char *const path,
    const struct dicey_selector sel,
    struct dicey_packet packet
) {
    (void) server;

    printf(
        "info: received request #%" PRIu32 " from client %zu for `%s:%s@%s`\n", seq, cln->id, sel.trait, sel.elem, path
    );

    struct util_dumper dumper = util_dumper_for(stdout);

    util_dumper_dump_packet(&dumper, packet);

    if (!strcmp(path, DUMMY_PATH) && !strcmp(sel.trait, DUMMY_TRAIT) && !strcmp(sel.elem, DUMMY_ELEMENT)) {
        const enum dicey_error err =
            send_reply(server, cln, seq, path, sel, (struct dicey_arg) { .type = DICEY_TYPE_BOOL, .boolean = true });
        if (err) {
            fprintf(stderr, "error: %s\n", dicey_error_name(err));
        }
    }
}

#if PIPE_NEEDS_CLEANUP
static enum dicey_error remove_socket_if_present(void) {
    const int err = uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

    return err == UV_ENOENT ? 0 : DICEY_EUV_UNKNOWN;
}
#endif

int main(void) {
    struct dicey_server *server = NULL;

    enum dicey_error err = dicey_server_new(
        &server,
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

    err = registry_fill(dicey_server_get_registry(server));
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

    err = dicey_server_start(server, PIPE_NAME, sizeof PIPE_NAME - 1U);
    if (err) {
        fprintf(stderr, "dicey_server_start: %s\n", dicey_error_name(err));

        goto quit;
    }

    uv_fs_unlink(NULL, &(uv_fs_t) { 0 }, PIPE_NAME, NULL);

quit:
    dicey_server_delete(server);

    return err == DICEY_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
