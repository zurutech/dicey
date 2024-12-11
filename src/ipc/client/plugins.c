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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/client.h>
#include <dicey/ipc/plugins.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "ipc/plugin-macros.h"

#include "client-internal.h"

struct dicey_plugin_work_ctx {
    struct dicey_packet request;
    struct dicey_message_builder builder;
};

struct dicey_plugin_work_list {
    size_t len, cap;
    struct dicey_plugin_work_ctx jobs[];
};

struct dicey_plugin {
    struct dicey_client client;

    dicey_plugin_quit_fn
        *on_quit; //< the function to call when the server asks the plugin to quit, will call exit(FAILURE) if not set
    dicey_plugin_do_work_fn *on_work_received; //< the function to call when the server asks the plugin to do work
};

static void quit_immediately(void) {
    exit(EXIT_FAILURE);
}

void dicey_plugin_delete(struct dicey_plugin *const plugin) {
    if (plugin) {
        dicey_client_deinit(&plugin->client);
    }
}

enum dicey_error dicey_plugin_new(
    const int argc,
    const char *const argv[],
    struct dicey_plugin **const dest,
    const struct dicey_plugin_args *const args
) {
    // future proof: we don't do anything with argc and argv, but we might in the future
    // and it's better not having to change the signature of this function
    DICEY_UNUSED(argc);
    DICEY_UNUSED(argv);

    assert(argc && argv && dest && args);

    struct dicey_plugin *const plugin = calloc(1U, sizeof *plugin);
    if (!plugin) {
        return TRACE(DICEY_ENOMEM);
    }

    enum dicey_error err = dicey_client_init(&plugin->client, &args->cargs);
    if (err) {
        free(plugin);

        return err;
    }

    plugin->on_quit = args->on_quit ? args->on_quit : &quit_immediately;
    plugin->on_work_received = args->on_work_received;

    err = dicey_client_open_fd(&plugin->client, DICEY_PLUGIN_FD);
    if (err) {
        dicey_plugin_delete(plugin);
    }

    *dest = plugin;

    return err;
}
