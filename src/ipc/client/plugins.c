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
#include <dicey/core/value.h>
#include <dicey/ipc/builtins/plugins.h>
#include <dicey/ipc/builtins/server.h>
#include <dicey/ipc/client.h>
#include <dicey/ipc/plugins.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "ipc/plugin-macros.h"
#include "ipc/server/builtins/plugins/plugins.h"

#include "client-internal.h"

#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4996) // strdup
#endif

struct plugin_work_ctx {
    struct dicey_packet request;
    struct dicey_message_builder builder;
};

#define ARRAY_TYPE_NAME plugin_work_list
#define ARRAY_VALUE_TYPE struct plugin_work_ctx
#define ARRAY_TYPE_NEEDS_CLEANUP 1
#include "sup/array.inc"

struct dicey_plugin {
    struct dicey_client client;

    struct plugin_work_list *todo_list;

    char *dicey_path;

    // the plugin needs to hijack the event handler to intercept the commands signal
    // this is the user-provided event handler that will be called after the plugin's own event handler
    // has filtered out the commands
    dicey_client_signal_fn *user_on_signal;

    dicey_plugin_quit_fn *on_quit;
    dicey_plugin_do_work_fn *on_work_received;
};

static void clear_pending_job(struct plugin_work_ctx *const ctx) {
    if (ctx) {
        dicey_packet_deinit(&ctx->request);
        dicey_message_builder_discard(&ctx->builder);
    }
}

static enum dicey_error extract_path(struct dicey_packet response, char **dest) {
    assert(dest && dicey_packet_is_valid(response));

    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(response, &msg);
    if (err) {
        return err;
    }

    const char *dicey_path = NULL;
    err = dicey_value_get_path(&msg.value, &dicey_path);
    if (err) {
        return err;
    }

    *dest = strdup(dicey_path);
    if (!*dest) {
        err = TRACE(DICEY_ENOMEM);
    }

    return err;
}

static void plugin_on_signal(struct dicey_client *const client, void *const ctx, struct dicey_packet *const packet) {
    assert(client && ctx && packet);

    struct dicey_plugin *const plugin = (struct dicey_plugin *) client;

    assert(dicey_packet_is_valid(*packet));

    struct dicey_message msg = { 0 };
    DICEY_ASSUME(dicey_packet_as_message(*packet, &msg));

    assert(msg.type == DICEY_OP_SIGNAL);

    if (plugin->user_on_signal) {
        plugin->user_on_signal(client, ctx, packet);
    }
}

static enum dicey_error subscribe_to_commands(struct dicey_plugin *const plugin, const char *const path) {
    assert(plugin && path);

    return dicey_client_subscribe_to(
        (struct dicey_client *) plugin,
        path,
        (struct dicey_selector) {
            .trait = DICEY_PLUGIN_TRAIT_NAME,
            .elem = PLUGIN_COMMAND_SIGNAL_NAME,
        },
        CLIENT_DEFAULT_TIMEOUT
    );
}

static enum dicey_error plugin_client_handshake(struct dicey_plugin *const plugin, const char *const name) {
    assert(plugin && name && !plugin->dicey_path);

    // step 1. send the handshake packet with the name of the plugin
    struct dicey_packet response = { 0 };

    enum dicey_error err = dicey_client_exec(
        (struct dicey_client *) plugin,
        DICEY_SERVER_PATH,
        (struct dicey_selector) {
            .trait = DICEY_PLUGINMANAGER_TRAIT_NAME,
            .elem = PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME,
        },
        (struct dicey_arg) {
            .type = DICEY_TYPE_STR,
            .str = name,
        },
        &response,
        CLIENT_DEFAULT_TIMEOUT
    );

    if (err) {
        return err;
    }

    // step 2. extract the Dicey path for the plugin object from the response
    char *dicey_path = NULL;
    err = extract_path(response, &dicey_path);

    dicey_packet_deinit(&response);

    if (err) {
        return err;
    }

    // step 3. subscribe to the commands signal
    err = subscribe_to_commands(plugin, dicey_path);
    if (err) {
        free(dicey_path);

        return err;
    }

    plugin->dicey_path = dicey_path;

    return DICEY_OK;
}

static void quit_immediately(void) {
    exit(EXIT_FAILURE);
}

void dicey_plugin_delete(struct dicey_plugin *const plugin) {
    if (plugin) {
        dicey_client_deinit(&plugin->client);
        free(plugin->dicey_path);

        plugin_work_list_delete(plugin->todo_list, &clear_pending_job);

        free(plugin);
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

    const char *const name = args->name;
    assert(name);

    struct dicey_plugin *const plugin = calloc(1U, sizeof *plugin);
    if (!plugin) {
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_client_args cargs = args->cargs;

    // store the user's on_signal function
    plugin->user_on_signal = cargs.on_signal;
    cargs.on_signal = &plugin_on_signal;

    enum dicey_error err = dicey_client_init(&plugin->client, &cargs);
    if (err) {
        free(plugin);

        return err;
    }

    plugin->todo_list = NULL; // autoallocating
    plugin->on_quit = args->on_quit ? args->on_quit : &quit_immediately;
    plugin->on_work_received = args->on_work_received;

    err = dicey_client_open_fd(&plugin->client, DICEY_PLUGIN_FD);
    if (err) {
        dicey_plugin_delete(plugin);
    }

    err = plugin_client_handshake(plugin, name);
    if (err) {
        dicey_plugin_delete(plugin);
    }

    *dest = plugin;

    return err;
}
