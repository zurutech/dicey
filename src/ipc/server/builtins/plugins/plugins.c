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
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/builders.h>
#include <dicey/core/value.h>
#include <dicey/ipc/builtins/plugins.h>
#include <dicey/ipc/builtins/server.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "ipc/plugin-common.h"

#include "ipc/server/plugins-internal.h"
#include "ipc/server/server-clients.h"

#include "../builtins.h"

#include "plugins.h"

enum plugin_op {
    PLUGIN_OP_LIST,
    PLUGIN_OP_HANDSHAKEINTERNAL,
    PLUGIN_GET_NAME,
    PLUGIN_GET_PATH,
    PLUGIN_CMD_RESPONSE,
};

struct work_response {
    uint64_t jid;             // the job id
    struct dicey_value value; // the response value
};

static const struct dicey_default_element pm_elements[] = {
    {
     .name = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_SIG,
     .opcode = PLUGIN_OP_LIST,
     },
    {
     .name = PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = PLUGINMANAGER_HANDSHAKEINTERNAL_OP_SIG,
     .flags = DICEY_ELEMENT_INTERNAL,
     .opcode = PLUGIN_OP_HANDSHAKEINTERNAL,
     },
};

static const struct dicey_default_element plugin_elements[] = {
    {
     .name = DICEY_PLUGIN_NAME_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_PLUGIN_NAME_PROP_SIG,
     .flags = DICEY_ELEMENT_READONLY,
     .opcode = PLUGIN_GET_NAME,
     },
    {
     .name = DICEY_PLUGIN_PATH_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_PLUGIN_PATH_PROP_SIG,
     .flags = DICEY_ELEMENT_READONLY,
     .opcode = PLUGIN_GET_PATH,
     },

 // internal stuff
    {
     .name = PLUGIN_COMMAND_SIGNAL_NAME,
     .type = DICEY_ELEMENT_TYPE_SIGNAL,
     .signature = PLUGIN_COMMAND_SIGNAL_SIG,
     .flags = DICEY_ELEMENT_INTERNAL,
     },
    {
     .name = PLUGIN_REPLY_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = PLUGIN_REPLY_OP_SIG,
     .flags = DICEY_ELEMENT_INTERNAL,
     .opcode = PLUGIN_CMD_RESPONSE,
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
            .elem = PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME,
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
    struct dicey_plugin_data *const plugin,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    assert(server && plugin && value && response);

    const char *name = NULL;
    enum dicey_error err = dicey_value_get_str(value, &name);
    if (err) {
        return err;
    }

    const char *obj_path = NULL;
    err = dicey_server_plugin_handshake(server, plugin, name, &obj_path);
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

static enum dicey_error handle_get_plugin_property(
    const struct dicey_server *const server,
    const char *const obj_path,
    const uint8_t opcode,
    struct dicey_packet *const response
) {
    assert(server && obj_path && response);

    // TODO: optimise this by putting a pointer in the registry
    const char *name = dicey_plugin_name_from_path(obj_path);
    assert(name); // if a path exists then is should be valid

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = dicey_message_builder_init(&builder);
    if (err) {
        return err;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_RESPONSE);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_set_path(&builder, obj_path);
    if (err) {
        goto quit;
    }

    err = dicey_message_builder_set_selector(
        &builder,
        (struct dicey_selector) {
            .trait = DICEY_PLUGIN_TRAIT_NAME,
            .elem = opcode == PLUGIN_GET_NAME ? DICEY_PLUGIN_NAME_PROP_NAME : DICEY_PLUGIN_PATH_PROP_NAME,
        }
    );

    if (err) {
        goto quit;
    }

    if (opcode == PLUGIN_GET_NAME) {
        err = dicey_message_builder_set_value(
            &builder,
            (struct dicey_arg) {
                .type = DICEY_TYPE_STR,
                .str = name,
            }
        );
    } else {
        const struct dicey_plugin_data *const plugin = dicey_server_plugin_find_by_name(server, name);
        assert(plugin); // if the path exists then the plugin should exist and have a valid name
        DICEY_UNUSED(plugin);

        err = dicey_message_builder_set_value(
            &builder,
            (struct dicey_arg) {
                .type = DICEY_TYPE_PATH,
                .path = obj_path,
            }
        );
    }

    if (err) {
        goto quit;
    }

    err = dicey_message_builder_build(&builder, response);

quit:
    dicey_message_builder_discard(&builder);

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

static enum dicey_error read_work_response(const struct dicey_value *const value, struct work_response *const out) {
    assert(value && out);

    struct dicey_pair pair = { 0 };
    enum dicey_error err = dicey_value_get_pair(value, &pair);
    if (err) {
        return err;
    }

    uint64_t jid = 0U;
    err = dicey_value_get_u64(&pair.first, &jid);
    if (err) {
        return err;
    }

    *out = (struct work_response) {
        .jid = jid,
        .value = pair.second,
    };

    return DICEY_OK;
}
static enum dicey_error handle_work_response(
    struct dicey_server *server,
    struct dicey_plugin_data *plugin,
    const struct dicey_value *value
) {
    assert(server && plugin && value);

    struct work_response response = { 0 };
    enum dicey_error err = read_work_response(value, &response);
    if (err) {
        return err;
    }

    return dicey_server_plugin_report_work_done(server, plugin, response.jid, &response.value);
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
    DICEY_UNUSED(context);
    DICEY_UNUSED(src_entry);

    assert(context && context->registry && client && value && response);

    struct dicey_server *const server = client->parent;
    assert(server);

    switch (opcode) {
    case PLUGIN_OP_LIST:
        if (!dicey_value_is_unit(value)) {
            return TRACE(DICEY_EINVAL);
        }

        return handle_list_plugins(server, response);

    case PLUGIN_GET_NAME:
    case PLUGIN_GET_PATH:
        return handle_get_plugin_property(server, src_path, opcode, response);
    }

    // internal plugin functions
    struct dicey_plugin_data *const plugin = dicey_client_data_as_plugin(client);

    if (!plugin) {
        return TRACE(DICEY_EACCES); // only plugins can handshake internally for obvious reasons
    }

    switch (opcode) {
    case PLUGIN_OP_HANDSHAKEINTERNAL:
        {
            const enum dicey_error err = handle_handshake(client->parent, plugin, value, response);
            if (err) {
                // uncereomoniously kill the plugin if the handshake fails
                (void) dicey_server_cleanup_id(server, client->info.id);
            }

            return DICEY_OK; // we don't care about the response, the child will be killed anyway
        }

    case PLUGIN_CMD_RESPONSE:
        return handle_work_response(client->parent, plugin, value);
    }

    DICEY_UNREACHABLE();
    return TRACE(DICEY_EINVAL);
}

const struct dicey_registry_builtin_set dicey_registry_plugins_builtins = {
    .traits = plugin_traits,
    .ntraits = DICEY_LENOF(plugin_traits),

    .handler = &handle_plugin_operation,
};
