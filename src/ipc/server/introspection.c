// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stddef.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/traits.h>

#include "dicey/ipc/registry.h"
#include "sup/trace.h"

#include "introspection.h"

#define LEN_OF(ARR) (sizeof(ARR) / sizeof(ARR)[0])

struct default_element {
    const char *name;
    enum dicey_element_type type;
    const char *signature;
    bool readonly;
};

struct default_object {
    const char *path;
    const char **traits;
};

struct default_trait {
    const char *name;
    const struct default_element *elements;
    size_t num_elements;
};

static const struct default_element introspection_elements[] = {
    {.name = DICEY_INTROSPECTION_XML_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_INTROSPECTION_XML_PROP_SIG,
     .readonly = true},
};

static const struct default_element registry_elements[] = {
    { .name = DICEY_REGISTRY_OBJECTS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_REGISTRY_OBJECTS_PROP_SIG,
     .readonly = true },
    { .name = DICEY_REGISTRY_TRAITS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_REGISTRY_TRAITS_PROP_SIG,
     .readonly = true },
    { .name = DICEY_REGISTRY_ELEMENT_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_ELEMENT_EXISTS_OP_SIG },
    { .name = DICEY_REGISTRY_PATH_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_PATH_EXISTS_OP_SIG },
    { .name = DICEY_REGISTRY_TRAIT_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_TRAIT_EXISTS_OP_SIG },
};

static const struct default_element trait_elements[] = {
    {.name = DICEY_TRAIT_PROPERTIES_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_PROPERTIES_PROP_SIG,
     .readonly = true},
    { .name = DICEY_TRAIT_SIGNALS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_SIGNALS_PROP_SIG,
     .readonly = true},
    { .name = DICEY_TRAIT_OPERATIONS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_OPERATIONS_PROP_SIG,
     .readonly = true},
};

static const struct default_object default_objects[] = {
    {
     .path = DICEY_REGISTRY_PATH,
     .traits = (const char *[]) { DICEY_REGISTRY_TRAIT_NAME, NULL },
     },
};

static const struct default_trait default_traits[] = {
    {.name = DICEY_INTROSPECTION_TRAIT_NAME,
     .elements = introspection_elements,
     .num_elements = LEN_OF(introspection_elements)                                                                  },
    { .name = DICEY_REGISTRY_TRAIT_NAME,     .elements = registry_elements, .num_elements = LEN_OF(registry_elements)},
    { .name = DICEY_TRAIT_TRAIT_NAME,        .elements = trait_elements,    .num_elements = LEN_OF(trait_elements)   },
};

static enum dicey_error introspection_populate_default_objects(struct dicey_registry *const registry) {
    assert(registry);

    const struct default_object *const end = default_objects + LEN_OF(default_objects);
    for (const struct default_object *obj_def = default_objects; obj_def < end; ++obj_def) {
        const enum dicey_error err =
            dicey_registry_add_object_with_trait_list(registry, obj_def->path, obj_def->traits);
        if (err != DICEY_OK) {
            return err;
        }
    }

    return DICEY_OK;
}

static enum dicey_error introspection_populate_default_traits(struct dicey_registry *const registry) {
    assert(registry);

    const struct default_trait *const end = default_traits + LEN_OF(default_traits);

    for (const struct default_trait *trait_def = default_traits; trait_def < end; ++trait_def) {
        struct dicey_trait *const trait = dicey_trait_new(trait_def->name);
        if (!trait) {
            return TRACE(DICEY_ENOMEM);
        }

        const struct default_element *const tend = trait_def->elements + trait_def->num_elements;
        for (const struct default_element *elem_def = trait_def->elements; elem_def < tend; ++elem_def) {
            enum dicey_error err = dicey_trait_add_element(
                trait,
                elem_def->name,
                (struct dicey_element
                ) { .type = elem_def->type, .signature = elem_def->signature, .readonly = elem_def->readonly }
            );

            if (err != DICEY_OK) {
                dicey_trait_delete(trait);
                return err;
            }
        }

        const enum dicey_error err = dicey_registry_add_trait(registry, trait);
        if (err != DICEY_OK) {
            dicey_trait_delete(trait);
            return err;
        }
    }

    return DICEY_OK;
}

enum dicey_error dicey_registry_populate_defaults(struct dicey_registry *const registry) {
    assert(registry);

    enum dicey_error err = introspection_populate_default_traits(registry);
    if (err) {
        return err;
    }

    return introspection_populate_default_objects(registry);
}
