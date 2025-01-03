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

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <dicey/dicey.h>

static void inspector(struct dicey_client *const client, void *const ctx, struct dicey_client_event event) {
    (void) client;
    (void) ctx;

    assert(client);

    switch (event.type) {
    case DICEY_CLIENT_EVENT_CONNECT:
        puts("client connected");
        break;

    case DICEY_CLIENT_EVENT_ERROR:
        fprintf(stderr, "error: [%s] %s\n", dicey_error_msg(event.error.err), event.error.msg);

        if (dicey_client_is_running(client) && dicey_client_disconnect(client) != DICEY_OK) {
            fprintf(stderr, "error: failed to stop client\n");

            exit(EXIT_FAILURE);
        }
        break;

    case DICEY_CLIENT_EVENT_HANDSHAKE_START:
        printf(
            "handshake started, presenting version %" PRId16 "r%" PRId16 "\n",
            event.version.major,
            event.version.revision
        );
        break;

    case DICEY_CLIENT_EVENT_INIT:
        puts("client initialized");
        break;

    case DICEY_CLIENT_EVENT_MESSAGE_RECEIVING:
        puts("receiving message");
        break;

    case DICEY_CLIENT_EVENT_MESSAGE_SENDING:
        puts("sending message");
        break;

    case DICEY_CLIENT_EVENT_SERVER_BYE:
        puts("server said goodbye");
        break;

    case DICEY_CLIENT_EVENT_QUITTING:
        puts("client quitting");
        break;

    case DICEY_CLIENT_EVENT_QUIT:
        puts("client quit");
        break;

    default:
        abort();
    }
}

static void quit_requested(void) {
    // probably not ideal, but it's a dummy program anyway
    exit(EXIT_SUCCESS);
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

    struct dicey_plugin *plugin = NULL;

    puts("info: dummy plugin launched");

    enum dicey_error err = dicey_plugin_init(argc, argv, &plugin, &(struct dicey_plugin_args) {
        .cargs = {
            .on_signal = NULL,
            .inspect_func = &inspector,
        },

        .name = "dummy_plugin",
        .on_quit = &quit_requested,
        .on_work_received = NULL,
    });

    if (err) {
        fprintf(stderr, "error: failed to initialise plugin: %s\n", dicey_error_msg(err));

        return EXIT_FAILURE;
    }

    puts("info: dummy plugin initialised");

    return EXIT_SUCCESS;
}
