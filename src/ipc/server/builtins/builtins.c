// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"

#include "introspection/introspection.h"
#include "server/server.h"

#include "builtins.h"

static const struct dicey_registry_builtin_set *default_builtins[] = {
    &dicey_registry_introspection_builtins,
    &dicey_registry_server_builtins,
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
    const struct dicey_default_trait *const traits,
    size_t ntraits
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
            enum dicey_error err = dicey_trait_add_element(
                trait,
                elem_def->name,
                (struct dicey_element) {
                    .type = elem_def->type,
                    .signature = elem_def->signature,
                    .readonly = elem_def->readonly,
                    ._tag =
                        elem_def->tag, // use tag to identify that this is a builtin operation with a specific opcode
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
    const struct dicey_registry_builtin_set *const set
) {
    assert(registry && set);

    const enum dicey_error err = populate_traits(registry, set->traits, set->ntraits);
    if (err) {
        return err;
    }

    return populate_objects(registry, set->objects, set->nobjects);
}

enum dicey_error dicey_registry_populate_builtins(struct dicey_registry *const registry) {
    assert(registry);

    const struct dicey_registry_builtin_set *const *const end = default_builtins + DICEY_LENOF(default_builtins);
    for (const struct dicey_registry_builtin_set *const *set = default_builtins; set < end; ++set) {
        const enum dicey_error err = populate_registry_with(registry, *set);
        if (err) {
            return err;
        }
    }

    return DICEY_OK;
}
