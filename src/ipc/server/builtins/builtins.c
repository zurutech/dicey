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

#include "dicey_config.h"

#include <assert.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "introspection/introspection.h"
#include "server/server.h"

#if DICEY_HAS_PLUGINS

#include "plugins/plugins.h"

#endif // DICEY_HAS_PLUGINS

#include "builtins.h"

#define BASE_OF(X) ((size_t) (((X) &0xFF00U) >> 8U))
#define OPCODE_OF(X) ((uint8_t) ((X) &0xFFU))

#define TAGGED(TAG, OPID) ((uintptr_t) ((TAG) << 8U | ((OPID) &0xFFU)))

static const struct dicey_registry_builtin_set *default_builtins[] = {
    &dicey_registry_introspection_builtins,
    &dicey_registry_server_builtins,

#if DICEY_HAS_PLUGINS
// &dicey_registry_plugins_builtins,
#endif // DICEY_HAS_PLUGINS

};

static enum dicey_error populate_objects(
    struct dicey_registry *const registry,
    const struct dicey_default_object *const objects,
    size_t nobjects
) {
    assert(registry && objects);

    const struct dicey_default_object *const end = objects + nobjects;
    for (const struct dicey_default_object *obj_def = objects; obj_def < end; ++obj_def) {
        const enum dicey_error err =
            dicey_registry_add_object_with_trait_list(registry, obj_def->path, obj_def->traits);
        if (err) {
            return err;
        }
    }

    return DICEY_OK;
}

static enum dicey_error populate_traits(
    struct dicey_registry *const registry,
    const size_t tag,
    const struct dicey_default_trait *const traits,
    const size_t ntraits
) {
    assert(registry);

    const struct dicey_default_trait *const end = traits + ntraits;

    for (const struct dicey_default_trait *trait_def = traits; trait_def < end; ++trait_def) {
        struct dicey_trait *const trait = dicey_trait_new(trait_def->name);
        if (!trait) {
            return TRACE(DICEY_ENOMEM);
        }

        const struct dicey_default_element *const tend = trait_def->elements + trait_def->num_elements;
        for (const struct dicey_default_element *elem_def = trait_def->elements; elem_def < tend; ++elem_def) {
            // assert that all opcodes are within the allowed 8-bit range
            assert(elem_def->opcode <= 0xFF);

            enum dicey_error err = dicey_trait_add_element(
                trait,
                elem_def->name,
                (struct dicey_element) {
                    .type = elem_def->type,
                    .signature = elem_def->signature,
                    .readonly = elem_def->readonly,
                    // use tag to identify that this is a builtin operation with a specific opcode
                    ._tag = TAGGED(tag, elem_def->opcode),
                }
            );

            if (err) {
                dicey_trait_delete(trait);
                return err;
            }
        }

        const enum dicey_error err = dicey_registry_add_trait(registry, trait);
        if (err) {
            dicey_trait_delete(trait);
            return err;
        }
    }

    return DICEY_OK;
}

static enum dicey_error populate_registry_with(
    struct dicey_registry *const registry,
    const struct dicey_registry_builtin_set *const set,
    const size_t tag
) {
    assert(registry && set);

    const enum dicey_error err = populate_traits(registry, tag, set->traits, set->ntraits);
    if (err) {
        return err;
    }

    return populate_objects(registry, set->objects, set->nobjects);
}

bool dicey_registry_get_builtin_info_for(
    const struct dicey_element_entry *elem,
    struct dicey_registry_builtin_info *target
) {
    if (!elem) {
        return false;
    }

    // the tag is used to identify which elements are builtins
    const uintptr_t tag = elem->element->_tag;

    if (!tag) {
        return false;
    }

    // every builtin set has a base tag, so we can use this to match the element back to its set
    // the tag is the index of the builtin set in the default_builtins array
    const size_t ix = BASE_OF(tag);
    assert(ix < DICEY_LENOF(default_builtins));

    const struct dicey_registry_builtin_set *const set = default_builtins[ix];
    assert(set && set->handler);

    *target = (struct dicey_registry_builtin_info) {
        .handler = set->handler,
        .opcode = OPCODE_OF(tag),
    };

    return true;
}

enum dicey_error dicey_registry_populate_builtins(struct dicey_registry *const registry) {
    assert(registry);

    for (size_t i = 0U; i < DICEY_LENOF(default_builtins); ++i) {
        const struct dicey_registry_builtin_set *const set = default_builtins[i];

        const enum dicey_error err = populate_registry_with(registry, set, i);
        if (err) {
            return err;
        }
    }

    return DICEY_OK;
}
