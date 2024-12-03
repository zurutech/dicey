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
#include <stdint.h>
#include <stdlib.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>
#include <dicey/ipc/builtins/server.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/traits.h>

#include "ipc/elemdescr.h"
#include "ipc/server/builtins/builtins.h"
#include "ipc/server/client-data.h"

#include "sup/trace.h"
#include "sup/util.h"

#include "server.h"

#include "dicey_config.h"

enum server_op {
    SERVER_OP_EVENT_SUBSCRIBE = 0,
    SERVER_OP_EVENT_UNSUBSCRIBE,

#if DICEY_HAS_PLUGINS

    SERVER_OP_PLUGIN_LIST,

#endif // DICEY_HAS_PLUGINS

};

static const struct dicey_default_element em_elements[] = {
    {
     .name = DICEY_EVENTMANAGER_SUBSCRIBE_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_EVENTMANAGER_SUBSCRIBE_OP_SIG,
     .opcode = SERVER_OP_EVENT_SUBSCRIBE,
     },
    {
     .name = DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_SIG,
     .opcode = SERVER_OP_EVENT_UNSUBSCRIBE,
     },
};

#if DICEY_HAS_PLUGINS

static const struct dicey_default_element pm_elements[] = {
    {
     .name = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_PLUGINMANAGER_LISTPLUGINS_OP_SIG,
     .opcode = SERVER_OP_PLUGIN_LIST,
     },
};

static const struct dicey_default_trait server_traits[] = {
    {.name = DICEY_EVENTMANAGER_TRAIT_NAME,   .elements = em_elements, .num_elements = DICEY_LENOF(em_elements)},
    { .name = DICEY_PLUGINMANAGER_TRAIT_NAME, .elements = pm_elements, .num_elements = DICEY_LENOF(pm_elements)},
};

static const char *const server_object_traits[] = {
    DICEY_EVENTMANAGER_TRAIT_NAME,
    DICEY_PLUGINMANAGER_TRAIT_NAME,

    NULL,
};

#else

static const struct dicey_default_trait server_traits[] = {
    {.name = DICEY_EVENTMANAGER_TRAIT_NAME, .elements = em_elements, .num_elements = DICEY_LENOF(em_elements)},
};

static const char *const server_object_traits[] = {
    DICEY_EVENTMANAGER_TRAIT_NAME,
    NULL,
};

#endif // DICEY_HAS_PLUGINS

static const struct dicey_default_object server_objects[] = {
    {
     .path = DICEY_SERVER_PATH,
     .traits = server_object_traits,
     },
};

static enum dicey_error extract_path_sel(
    const struct dicey_value *const value,
    const char **const path,
    struct dicey_selector *const sel
) {
    assert(value && path && sel);

    struct dicey_pair pair = { 0 };

    enum dicey_error err = dicey_value_get_pair(value, &pair);
    if (err) {
        return err;
    }

    err = dicey_value_get_path(&pair.first, path);
    if (err) {
        return err;
    }

    err = dicey_value_get_selector(&pair.second, sel);
    if (err) {
        return err;
    }

    return DICEY_OK;
}

#if DICEY_HAS_PLUGINS

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

#endif // DICEY_HAS_PLUGINS

static enum dicey_error unit_message_for(
    struct dicey_packet *const dest,
    const char *const path,
    const struct dicey_selector sel
) {
    assert(dest && path && dicey_selector_is_valid(sel));

    struct dicey_message_builder builder = { 0 };

    enum dicey_error err = dicey_message_builder_init(&builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_RESPONSE);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&builder, path);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(&builder, sel);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_value(&builder, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
    if (err) {
        goto fail;
    }

    return dicey_message_builder_build(&builder, dest);

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

static enum dicey_error handle_server_operation(
    struct dicey_builtin_context *const context,
    const uint8_t opcode,
    struct dicey_client_data *const client,
    const char *const src_path,
    const struct dicey_element_entry *const src_entry,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    (void) src_path;
    (void) src_entry;

    assert(context && client && value && response);

#if DICEY_HAS_PLUGINS
    // shorcircuit the plugin list operation
    if (opcode == SERVER_OP_PLUGIN_LIST) {
        if (!dicey_value_is_unit(value)) {
            return TRACE(DICEY_EINVAL);
        }

        return handle_list_plugins(client->parent, response);
    }
#endif // DICEY_HAS_PLUGINS

    const char *path = NULL;
    struct dicey_selector sel = { 0 };

    enum dicey_error err = extract_path_sel(value, &path, &sel);
    if (err) {
        return err;
    }

    const struct dicey_element *elem = dicey_registry_get_element(context->registry, path, sel.trait, sel.elem);

    if (!elem) {
        return TRACE(DICEY_EELEMENT_NOT_FOUND);
    }

    if (elem->type != DICEY_ELEMENT_TYPE_SIGNAL) {
        return TRACE(DICEY_EINVAL);
    }

    // do not allocate the same stuff a billion times. use the scratchpad.
    struct dicey_view_mut *const scratchpad = context->scratchpad;
    assert(scratchpad);

    const char *const elemdescr = dicey_element_descriptor_format_to(scratchpad, path, sel);
    if (!elemdescr) {
        return TRACE(DICEY_ENOMEM);
    }

    switch (opcode) {
    case SERVER_OP_EVENT_SUBSCRIBE:
        err = dicey_client_data_subscribe(client, elemdescr);
        break;

    case SERVER_OP_EVENT_UNSUBSCRIBE:
        // do not trace the result of this operation, the ENOENT should be reported to the client without blocking
        // the server
        err = dicey_client_data_unsubscribe(client, elemdescr) ? DICEY_OK : DICEY_ENOENT;

        break;

    default:
        assert(false);
        return TRACE(DICEY_EINVAL);
    }

    if (err) {
        return err;
    }

    return unit_message_for(response, path, sel);
}

const struct dicey_registry_builtin_set dicey_registry_server_builtins = {
    .objects = server_objects,
    .nobjects = DICEY_LENOF(server_objects),

    .traits = server_traits,
    .ntraits = DICEY_LENOF(server_traits),

    .handler = &handle_server_operation,
};
