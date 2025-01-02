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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <uv.h>

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
#include "sup/uvtools.h"

#include "ipc/plugin-common.h"
#include "ipc/server/builtins/plugins/plugins.h"

#include "client-internal.h"

#include "dicey_config.h"

#if defined(DICEY_CC_IS_MSVC_LIKE)
#pragma warning(disable : 4996) // strdup
#endif

struct dicey_plugin_work_ctx {
    struct dicey_plugin *plugin;
    uint64_t task_id;
    struct dicey_packet request;
    struct dicey_value payload;
    struct dicey_message_builder builder;
};

#define ARRAY_TYPE_NAME plugin_work_list
#define ARRAY_VALUE_TYPE struct dicey_plugin_work_ctx
#define ARRAY_VALUE_TYPE_NEEDS_CLEANUP 1
#include "sup/array.inc"

struct dicey_plugin {
    struct dicey_client client;

    bool quitting; // stupid hack: if true, the plugin is dead and we should reject all work
    uv_mutex_t list_lock;
    struct plugin_work_list *todo_list;

    char *dicey_path;

    // the plugin needs to hijack the event handler to intercept the commands signal
    // this is the user-provided event handler that will be called after the plugin's own event handler
    // has filtered out the commands
    dicey_client_signal_fn *user_on_signal;

    dicey_plugin_quit_fn *on_quit;
    dicey_plugin_do_work_fn *on_work_received;
};

struct command_request {
    uint64_t task_id;
    enum dicey_plugin_command command;
    struct dicey_value value;
};

static const struct dicey_selector command_sig = {
    .trait = DICEY_PLUGIN_TRAIT_NAME,
    .elem = PLUGIN_COMMAND_SIGNAL_NAME,
};

static const struct dicey_selector command_reply = {
    .trait = DICEY_PLUGIN_TRAIT_NAME,
    .elem = PLUGIN_REPLY_OP_NAME,
};

static void clear_pending_job(struct dicey_plugin_work_ctx *const ctx) {
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

static bool is_cmd_valid(const uint8_t cmd) {
    switch (cmd) {
    case PLUGIN_COMMAND_DO_WORK:
    case PLUGIN_COMMAND_HALT:
        return true;

    default:
        return false;
    }
}

static bool is_fd_valid(const uv_file fd) {
    // this will cause an assertion on Debug targets on Windows due to __get_osfhandle raising an assertion
    // when the file descriptor is invalid. This can't be avoided because libuv is already hijacking the
    // CRT's assert handlers.
    return uv_guess_handle(fd) != UV_UNKNOWN_HANDLE;
}

static bool start_work(
    struct dicey_plugin *const plugin,
    struct dicey_packet packet,
    const uint64_t task_id,
    struct dicey_value value
) {
    assert(plugin && dicey_packet_is_valid(packet) && plugin->on_work_received);

    struct dicey_plugin_work_ctx ctx = {
        .plugin = plugin,
        .task_id = task_id,
        .request = packet,
        .payload = value,
    };

    uv_mutex_lock(&plugin->list_lock);

    // if another thread stops the client while it's handing work, we may end up in a situation where `delete` or
    // similar get the mutex before we do, causing the client loop to stall forever. Checking the quitting flag here is
    // a simple way to avoid problems

    if (plugin->quitting) {
        uv_mutex_unlock(&plugin->list_lock);
        goto fail;
    }

    struct dicey_plugin_work_ctx *const stored_ctx = plugin_work_list_append(&plugin->todo_list, &ctx);
    uv_mutex_unlock(&plugin->list_lock);

    if (!stored_ctx) {
        goto fail;
    }

    plugin->on_work_received(stored_ctx, &stored_ctx->payload);

    return true;

fail:
    clear_pending_job(&ctx);

    return false;
}

static bool handle_command(
    struct dicey_plugin *const plugin,
    struct dicey_packet *const packet,
    struct command_request *const creq
) {
    assert(plugin && packet && dicey_packet_is_valid(*packet) && creq);

    switch (creq->command) {
    case PLUGIN_COMMAND_DO_WORK:
        if (plugin->on_work_received) {
            struct dicey_packet stolen_packet = *packet;
            *packet = (struct dicey_packet) { 0 }; // steal the packet fron the callback

            return start_work(plugin, stolen_packet, creq->task_id, creq->value);
        }

        return true; // no work to do

    case PLUGIN_COMMAND_HALT:
        if (plugin->on_quit) {
            plugin->on_quit();
        }

        return true;

    default:
        DICEY_UNREACHABLE(); // we check for this in try_get_command, so this should never happen

        return false;
    }
}

// returns >1 if the command was successfully extracted, 0 if the message was not a command, <0 if an error occurred
static ptrdiff_t try_get_command(
    const char *const path,
    struct dicey_packet packet,
    struct command_request *const creq
) {
    assert(creq && path && dicey_packet_is_valid(packet));

    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(packet, &msg);
    if (err) {
        return err;
    }

    if (strcmp(msg.path, path)) {
        return 0; // can't be a command, not directed to us
    }

    if (dicey_selector_cmp(msg.selector, command_sig)) {
        return 0; // not a command
    }

    if (msg.type != DICEY_OP_SIGNAL) {
        return 0;
    }

    struct dicey_list tuple = { 0 };
    err = dicey_value_get_tuple(&msg.value, &tuple);
    if (err) {
        return err;
    }

    struct dicey_iterator iter = dicey_list_iter(&tuple);

    struct dicey_value tuple_elem = { 0 };
    err = dicey_iterator_next(&iter, &tuple_elem);
    if (err) {
        return err;
    }

    uint64_t task_id = 0U;
    err = dicey_value_get_u64(&tuple_elem, &task_id);
    if (err) {
        return err;
    }

    err = dicey_iterator_next(&iter, &tuple_elem);
    if (err) {
        return err;
    }

    uint8_t cmd = 0U;
    err = dicey_value_get_byte(&tuple_elem, &cmd);
    if (err) {
        return err;
    }

    if (!is_cmd_valid(cmd)) {
        return TRACE(DICEY_EBADMSG);
    }

    // extract the third, variant element we'll pass on to the user callback
    err = dicey_iterator_next(&iter, &tuple_elem);
    if (err) {
        return err;
    }

    // tuple must be exhausted
    if (dicey_iterator_has_next(iter)) {
        return TRACE(DICEY_EBADMSG);
    }

    *creq = (struct command_request) {
        .task_id = task_id,
        .command = (enum dicey_plugin_command) cmd,
        .value = tuple_elem, // borrowed from packet. Keep packet alive until we're done with the value
    };

    return 1; // success
}

static void plugin_on_signal(struct dicey_client *const client, void *const ctx, struct dicey_packet *const packet) {
    assert(client && ctx && packet);

    struct dicey_plugin *const plugin = (struct dicey_plugin *) client;

    assert(dicey_packet_is_valid(*packet));

    struct command_request creq = { 0 };
    const ptrdiff_t result = try_get_command(plugin->dicey_path, *packet, &creq);
    switch (result) {
    case 0:
        // if it's not a command, we pass it to the user's event handler
        if (plugin->user_on_signal) {
            plugin->user_on_signal(client, ctx, packet);
        }

        break;

    case 1:
        // if it's a command, intercept it
        handle_command(plugin, packet, &creq);

        break;

    default:
        break; // if we add logging, this would be a great place to put a warning
    }
}

static enum dicey_error subscribe_to_commands(struct dicey_plugin *const plugin, const char *const path) {
    assert(plugin && path);

    return dicey_client_subscribe_to((struct dicey_client *) plugin, path, command_sig, CLIENT_DEFAULT_TIMEOUT);
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
        uv_mutex_lock(&plugin->list_lock);

        plugin->quitting = true;

        // this way, when deinit attempts to stop the client, the loop will not be stuck waiting for the lock
        uv_mutex_unlock(&plugin->list_lock);

        // first, deinit the client. After this we know we will not get any more requests
        dicey_client_deinit(&plugin->client);

        free(plugin->dicey_path);

        plugin_work_list_delete(plugin->todo_list, &clear_pending_job);

        uv_mutex_destroy(&plugin->list_lock);

        free(plugin);
    }
}

enum dicey_error dicey_plugin_init(
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

    if (!is_fd_valid(DICEY_PLUGIN_FD)) {
        return TRACE(DICEY_EBADF);
    }

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

    err = dicey_error_from_uv(uv_mutex_init(&plugin->list_lock));
    if (err) {
        dicey_plugin_delete(plugin);

        return err;
    }

    plugin->todo_list = NULL; // autoallocating
    plugin->on_quit = args->on_quit ? args->on_quit : &quit_immediately;
    plugin->on_work_received = args->on_work_received;

    err = dicey_client_open_fd(&plugin->client, DICEY_PLUGIN_FD);
    if (err) {
        dicey_plugin_delete(plugin);

        return err;
    }

    err = plugin_client_handshake(plugin, name);
    if (err) {
        dicey_plugin_delete(plugin);

        return err;
    }

    *dest = plugin;

    return err;
}

enum dicey_error dicey_plugin_work_response_done(struct dicey_plugin_work_ctx *const ctx) {
    assert(ctx);

    struct dicey_plugin *const plugin = ctx->plugin;
    assert(plugin);

    struct dicey_packet output = { 0 };

    enum dicey_error err = DICEY_OK;

    uv_mutex_lock(&plugin->list_lock); // probably unnecessary

    err = dicey_message_builder_build(&ctx->builder, &output);

    if (!err) {
        // destroy the context now that we're done with it
        clear_pending_job(ctx);

        DICEY_UNUSED(plugin_work_list_erase(plugin->todo_list, ctx));
    }

    uv_mutex_unlock(&plugin->list_lock);

    if (err) {
        return err;
    }

    struct dicey_packet srv_response = { 0 };

    err = dicey_client_request((struct dicey_client *) plugin, output, &srv_response, CLIENT_DEFAULT_TIMEOUT);

#if !defined(NDEBUG)
    if (!err) {
        // if we're building in debug mode, check the validity of the server response
        struct dicey_message srv_msg = { 0 };
        DICEY_ASSUME(dicey_packet_as_message(srv_response, &srv_msg));
        assert(dicey_value_is_unit(&srv_msg.value));
    }
#endif

    return err;
}

enum dicey_error dicey_plugin_work_response_start(
    struct dicey_plugin_work_ctx *const ctx,
    struct dicey_value_builder *const value
) {
    assert(ctx && value);

    struct dicey_plugin *const plugin = ctx->plugin;
    assert(plugin);

    enum dicey_error err = DICEY_OK;

    uv_mutex_lock(&plugin->list_lock); // may be unnecessary
    if (dicey_message_builder_is_pending(&ctx->builder)) {
        err = TRACE(DICEY_EALREADY);

        goto fail;
    }

    err = dicey_message_builder_init(&ctx->builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&ctx->builder, DICEY_OP_EXEC);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&ctx->builder, plugin->dicey_path);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(&ctx->builder, command_reply);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_value_start(&ctx->builder, value);

fail:
    if (err) {
        dicey_message_builder_discard(&ctx->builder);
    }

    uv_mutex_unlock(&plugin->list_lock);

    return err;
}
