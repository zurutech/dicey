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
#include <stdbool.h>
#include <stddef.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/ipc/builtins/introspection.h>

#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"

#include "introspection-internal.h"

static const char *prop_for(const enum dicey_element_type op_kind) {
    switch (op_kind) {
    case DICEY_ELEMENT_TYPE_OPERATION:
        return DICEY_TRAIT_OPERATIONS_PROP_NAME;

    case DICEY_ELEMENT_TYPE_PROPERTY:
        return DICEY_TRAIT_PROPERTIES_PROP_NAME;

    case DICEY_ELEMENT_TYPE_SIGNAL:
        return DICEY_TRAIT_SIGNALS_PROP_NAME;

    default:
        assert(false); // should never be reached
        return NULL;
    }
}

static enum dicey_error populate_element_entry(
    const char *const name,
    const struct dicey_element *const elem,
    struct dicey_value_builder *const value
) {
    assert(name && elem && value);

    enum dicey_error err = dicey_value_builder_tuple_start(value);
    if (err) {
        return err;
    }

    struct dicey_value_builder name_builder = { 0 };
    err = dicey_value_builder_next(value, &name_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(
        &name_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_STR,
            .str = name,
        }
    );

    if (err) {
        return err;
    }

    struct dicey_value_builder sig_builder = { 0 };
    err = dicey_value_builder_next(value, &sig_builder);

    if (err) {
        return err;
    }

    err = dicey_value_builder_set(
        &sig_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_STR,
            .str = elem->signature,
        }
    );

    if (err) {
        return err;
    }

    if (elem->type == DICEY_ELEMENT_TYPE_PROPERTY) {
        struct dicey_value_builder readonly_builder = { 0 };

        err = dicey_value_builder_next(value, &readonly_builder);
        if (err) {
            return err;
        }

        err = dicey_value_builder_set(
            &readonly_builder,
            (struct dicey_arg) {
                .type = DICEY_TYPE_BOOL,
                .boolean = elem->flags & DICEY_ELEMENT_READONLY,
            }
        );

        if (err) {
            return err;
        }
    }

    return dicey_value_builder_tuple_end(value);
}

enum dicey_error introspection_craft_filtered_elemlist(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const enum dicey_element_type op_kind,
    struct dicey_packet *const dest
) {
    assert(registry && path && trait_name && dest);

    const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
    if (!trait) {
        return TRACE(DICEY_ENOENT);
    }

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = introspection_init_builder(&builder, path, DICEY_TRAIT_TRAIT_NAME, prop_for(op_kind));
    if (err) {
        return err;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_array_start(&value_builder, DICEY_TYPE_TUPLE);
    if (err) {
        return err;
    }

    struct dicey_trait_iter iter = dicey_trait_iter_start(trait);
    struct dicey_element elem = { 0 };

    const char *element_name = NULL;
    while (dicey_trait_iter_next(&iter, &element_name, &elem)) {
        struct dicey_value_builder elem_builder = { 0 };

        err = dicey_value_builder_next(&value_builder, &elem_builder);
        if (err) {
            return err;
        }

        if (elem.type == op_kind) {
            err = populate_element_entry(element_name, &elem, &elem_builder);
            if (err) {
                return err;
            }
        }
    }

    err = dicey_value_builder_array_end(&value_builder);
    if (err) {
        return err;
    }

    err = dicey_message_builder_value_end(&builder, &value_builder);
    if (err) {
        return err;
    }

    return dicey_message_builder_build(&builder, dest);
}
