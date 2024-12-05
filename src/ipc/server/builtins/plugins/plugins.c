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
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <dicey/core/builders.h>
#include <dicey/core/value.h>
#include <dicey/ipc/builtins/plugins.h>
#include <dicey/ipc/builtins/server.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "ipc/server/plugins-internal.h"

#include "../builtins.h"

#include "plugins.h"

enum plugin_op {
    PLUGIN_OP_HANDSHAKEINTERNAL = 0,
    PLUGIN_OP_LIST,
};

static const struct dicey_default_element pm_elements[] = {
    {
     .name = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_SIG,
     .opcode = PLUGIN_OP_LIST,
     },
    {
     .name = DICEY_PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_PLUGINMANAGER_HANDSHAKEINTERNAL_OP_SIG,
     },
};

static const struct dicey_default_element plugin_elements[] = {
    {
     .name = DICEY_PLUGIN_NAME_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_PLUGIN_NAME_PROP_SIG,
     .readonly = true,
     },
    {
     .name = DICEY_PLUGIN_PATH_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_PLUGIN_PATH_PROP_SIG,
     .readonly = true,
     },
};

static const struct dicey_default_trait plugin_traits[] = {
    {.name = DICEY_PLUGIN_TRAIT_NAME,         .elements = plugin_elements, .num_elements = DICEY_LENOF(plugin_elements)},
    { .name = DICEY_PLUGINMANAGER_TRAIT_NAME, .elements = pm_elements,     .num_elements = DICEY_LENOF(pm_elements)    },
};

static enum dicey_error craft_handshake_reply(
    struct dicey_message_builder *const builder,
    const char *const obj_path,
    struct dicey_packet *const response
) {
    assert(builder && obj_path && response);

    enum dicey_error err = dicey_message_builder_begin(builder, DICEY_OP_RESPONSE);
    if (err) {
        return err;
    }

    err = dicey_message_builder_set_path(builder, DICEY_SERVER_PATH);
    if (err) {
        return err;
    }

    err = dicey_message_builder_set_selector(
        builder,
        (struct dicey_selector) {
            .trait = DICEY_PLUGINMANAGER_TRAIT_NAME,
            .elem = DICEY_PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME,
        }
    );

    if (err) {
        return err;
    }

    err = dicey_message_builder_set_value(
        builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_PATH,
            .path = obj_path,
        }
    );

    if (err) {
        return err;
    }

    return dicey_message_builder_build(builder, response);
}

static enum dicey_error handle_handshake(
    struct dicey_server *server,
    struct dicey_client_data *client,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    assert(server && client && value && response);

    const char *name = NULL;
    enum dicey_error err = dicey_value_get_str(value, &name);
    if (err) {
        return err;
    }

    const char *obj_path = NULL;
    err = dicey_server_plugin_handshake(server, client, name, &obj_path);
    if (err) {
        return err;
    }

    assert(obj_path);

    struct dicey_message_builder builder = { 0 };
    err = dicey_message_builder_init(&builder);
    if (err) {
        return err;
    }

    err = craft_handshake_reply(&builder, obj_path, response);
    if (err) {
        dicey_message_builder_discard(&builder);
    }

    return err;
}

static enum dicey_error handle_list_plugins(struct dicey_server *const server, struct dicey_packet *const response) {
    assert(server && response);

    struct dicey_plugin_info *infos = NULL;
    uint16_t count = 0U;

    enum dicey_error err = dicey_server_list_plugins(server, &infos, &count);
    if (err) {
        return err;
    }

    struct dicey_message_builder builder = { 0 };
    err = dicey_message_builder_init(&builder);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_RESPONSE);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_set_path(&builder, DICEY_SERVER_PATH);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_set_selector(
        &builder,
        (struct dicey_selector) {
            .trait = DICEY_PLUGINMANAGER_TRAIT_NAME,
            .elem = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME,
        }
    );

    if (err) {
        goto quit;
    }

    struct dicey_value_builder array = { 0 };
    err = dicey_message_builder_value_start(&builder, &array);
    if (err) {
        goto quit;
    }

    err = dicey_value_builder_array_start(&array, DICEY_TYPE_PAIR);
    if (err) {
        goto quit;
    }

    const struct dicey_plugin_info *const end = infos + count;
    for (const struct dicey_plugin_info *it = infos; it != end; ++it) {
        assert(it->path);

        struct dicey_value_builder pair = { 0 };
        err = dicey_value_builder_next(&array, &pair);
        if (err) {
            goto quit;
        }

        err = dicey_value_builder_set(&pair, (struct dicey_arg) {
            .type = DICEY_TYPE_PAIR,
            .pair = {
                .first = &(struct dicey_arg) {
                    .type = DICEY_TYPE_STR,
                    .str = it->name ? it->name : "<INVALID>",
                },
                .second = &(struct dicey_arg) {
                    .type = DICEY_TYPE_STR,
                    .str = it->path,
                },
            },
        });

        if (err) {
            goto quit;
        }
    }

    err = dicey_value_builder_array_end(&array);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_value_end(&builder, &array);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_build(&builder, response);
    // fallthrough

quit:
    dicey_message_builder_discard(&builder);
    free(infos);

    return err;
}

static enum dicey_error handle_plugin_operation(
    struct dicey_builtin_context *const context,
    const uint8_t opcode,
    struct dicey_client_data *const client,
    const char *const src_path,
    const struct dicey_element_entry *const src_entry,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    (void) context;
    (void) src_path;
    (void) src_entry;

    assert(context && client && value && response);

    switch (opcode) {
    case PLUGIN_OP_HANDSHAKEINTERNAL:
        return handle_handshake(client->parent, client, value, response);

    case PLUGIN_OP_LIST:
        if (!dicey_value_is_unit(value)) {
            return TRACE(DICEY_EINVAL);
        }

        return handle_list_plugins(client->parent, response);
    }

    return TRACE(DICEY_EINVAL);
}

const struct dicey_registry_builtin_set dicey_registry_plugins_builtins = {
    .traits = plugin_traits,
    .ntraits = DICEY_LENOF(plugin_traits),

    .handler = &handle_plugin_operation,
};
