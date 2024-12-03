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
#include <string.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/packet.h>
#include <dicey/core/value.h>
#include <dicey/ipc/builtins/introspection.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"
#include "sup/util.h"

#include "ipc/server/builtins/builtins.h"
#include "ipc/server/client-data.h"

#include "introspection.h"

#include "introspection-internal.h"

#define METATRAITS_PREFIX DICEY_REGISTRY_TRAITS_PATH "/"

// this enum represents all the introspection operations that can be performed
// this is the value stored in the _tag field of `dicey_element`, and it's used to dispatch
enum introspection_op {
    INTROSPECTION_OP_INVALID = 0,
    INTROSPECTION_OP_GET_DATA,
    INTROSPECTION_OP_GET_XML,
    INTROSPECTION_OP_REGISTRY_GET_OBJS,
    INTROSPECTION_OP_REGISTRY_GET_TRAITS,
    INTROSPECTION_OP_REGISTRY_ELEMENT_EXISTS,
    INTROSPECTION_OP_REGISTRY_PATH_EXISTS,
    INTROSPECTION_OP_REGISTRY_TRAIT_EXISTS,
    INTROSPECTION_OP_TRAIT_GET_OPERATIONS,
    INTROSPECTION_OP_TRAIT_GET_PROPERTIES,
    INTROSPECTION_OP_TRAIT_GET_SIGNALS,
};

static const struct dicey_default_element introspection_elements[] = {
    {
     .name = DICEY_INTROSPECTION_DATA_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_INTROSPECTION_DATA_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_GET_DATA,
     },
    {
     .name = DICEY_INTROSPECTION_XML_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_INTROSPECTION_XML_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_GET_XML,
     },
};

static const struct dicey_default_element registry_elements[] = {
    {
     .name = DICEY_REGISTRY_OBJECTS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_REGISTRY_OBJECTS_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_REGISTRY_GET_OBJS,
     },
    {
     .name = DICEY_REGISTRY_TRAITS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_REGISTRY_TRAITS_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_REGISTRY_GET_TRAITS,
     },
    {
     .name = DICEY_REGISTRY_ELEMENT_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_ELEMENT_EXISTS_OP_SIG,
     .opcode = INTROSPECTION_OP_REGISTRY_ELEMENT_EXISTS,
     },
    {
     .name = DICEY_REGISTRY_PATH_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_PATH_EXISTS_OP_SIG,
     .opcode = INTROSPECTION_OP_REGISTRY_PATH_EXISTS,
     },
    {
     .name = DICEY_REGISTRY_TRAIT_EXISTS_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_REGISTRY_TRAIT_EXISTS_OP_SIG,
     .opcode = INTROSPECTION_OP_REGISTRY_TRAIT_EXISTS,
     },
};

static const struct dicey_default_element trait_elements[] = {
    {
     .name = DICEY_TRAIT_OPERATIONS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_OPERATIONS_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_TRAIT_GET_OPERATIONS,
     },
    {
     .name = DICEY_TRAIT_PROPERTIES_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_PROPERTIES_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_TRAIT_GET_PROPERTIES,
     },
    {
     .name = DICEY_TRAIT_SIGNALS_PROP_NAME,
     .type = DICEY_ELEMENT_TYPE_PROPERTY,
     .signature = DICEY_TRAIT_SIGNALS_PROP_SIG,
     .readonly = true,
     .opcode = INTROSPECTION_OP_TRAIT_GET_SIGNALS,
     },
};

static const struct dicey_default_object introspection_objects[] = {
    {
     .path = DICEY_REGISTRY_PATH,
     .traits = (const char *const[]) { DICEY_REGISTRY_TRAIT_NAME, NULL },
     },
};

// note that the order here is critical, as `dicey.Trait` must exist before any trait can be created
static const struct dicey_default_trait introspection_traits[] = {
    { .name = DICEY_TRAIT_TRAIT_NAME, .elements = trait_elements, .num_elements = DICEY_LENOF(trait_elements) },
    {
     .name = DICEY_INTROSPECTION_TRAIT_NAME,
     .elements = introspection_elements,
     .num_elements = DICEY_LENOF(introspection_elements),
     },
    { .name = DICEY_REGISTRY_TRAIT_NAME,
     .elements = registry_elements,
     .num_elements = DICEY_LENOF(registry_elements) },
};

static enum dicey_error validate_metatrait_name(const char *const path, const char **const trait_name) {
    assert(path && trait_name);

    const size_t traits_path_len = sizeof(METATRAITS_PREFIX) - 1;
    if (strncmp(path, METATRAITS_PREFIX, traits_path_len) != 0) {
        return TRACE(DICEY_EINVAL);
    }

    const char *last_member = strrchr(path, '/');

    // traits must be at least one character long (note: last_member starts with /)
    if (!last_member || strlen(last_member) < 2) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    *trait_name = last_member + 1U;

    return DICEY_OK;
}

static enum dicey_error value_get_element_info(
    const struct dicey_value *const value,
    const char **const path_dest,
    struct dicey_selector *const sel_dest
) {
    assert(value && path_dest && sel_dest);

    // extract a struct with signature (@%)
    struct dicey_list tuple = { 0 };

    enum dicey_error err = dicey_value_get_tuple(value, &tuple);
    if (err) {
        return err;
    }

    struct dicey_iterator iter = dicey_list_iter(&tuple);

    struct dicey_value path_value = { 0 };

    err = dicey_iterator_next(&iter, &path_value);
    if (err) {
        return err;
    }

    err = dicey_value_get_path(&path_value, path_dest);
    if (err) {
        return err;
    }

    struct dicey_value selector_value = { 0 };

    err = dicey_iterator_next(&iter, &selector_value);
    if (err) {
        return err;
    }

    err = dicey_value_get_selector(&selector_value, sel_dest);
    if (err) {
        return err;
    }

    // catch the case if the tuple has more elements than expected. This should not happen, but if it happens, do
    // not proceed with the operation.
    if (dicey_iterator_has_next(iter)) {
        *path_dest = NULL;
        *sel_dest = (struct dicey_selector) { 0 };

        return TRACE(DICEY_EINVAL);
    }

    return DICEY_OK;
}

static enum dicey_error perform_introspection_op(
    struct dicey_builtin_context *const context,
    const uint8_t opcode,
    struct dicey_client_data *const client,
    const char *const path,
    const struct dicey_element_entry *const entry,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    (void) client;
    (void) entry;
    (void) path;

    assert(context && path && entry && entry->element && response);

    struct dicey_registry *const registry = context->registry;

    // do not validate the path, as it is not necessary for introspection operations. We assume the registry
    // already performed such validations before invoking this function.
    switch (opcode) {
    case INTROSPECTION_OP_INVALID:
        return TRACE(DICEY_EINVAL);

    case INTROSPECTION_OP_GET_DATA:
        return introspection_dump_object(registry, path, response);

    case INTROSPECTION_OP_GET_XML:
        return introspection_dump_xml(registry, path, response);

    case INTROSPECTION_OP_REGISTRY_GET_OBJS:
        return introspection_craft_pathlist(registry, response);

    case INTROSPECTION_OP_REGISTRY_GET_TRAITS:
        return introspection_craft_traitlist(registry, response);

    case INTROSPECTION_OP_REGISTRY_ELEMENT_EXISTS:
        {
            assert(value);

            const char *tpath = NULL;
            struct dicey_selector tsel = { 0 };

            const enum dicey_error err = value_get_element_info(value, &tpath, &tsel);

            return err ? err : introspection_check_element_exists(registry, tpath, tsel, response);
        }

    case INTROSPECTION_OP_REGISTRY_PATH_EXISTS:
        {
            assert(value);

            const char *target = NULL;

            // this operation consumes a path and returns a boolean
            const enum dicey_error err = dicey_value_get_path(value, &target);

            return err ? err : introspection_check_path_exists(registry, target, response);
        }

    case INTROSPECTION_OP_REGISTRY_TRAIT_EXISTS:
        {
            assert(value);

            const char *target = NULL;

            // this operation consumes a path and returns a boolean
            const enum dicey_error err = dicey_value_get_str(value, &target);

            return err ? err : introspection_check_trait_exists(registry, target, response);
        }

    case INTROSPECTION_OP_TRAIT_GET_OPERATIONS:
        {
            const char *tname = NULL;

            const enum dicey_error err = validate_metatrait_name(path, &tname);

            return err ? err
                       : introspection_craft_filtered_elemlist(
                             registry, path, tname, DICEY_ELEMENT_TYPE_OPERATION, response
                         );
        }

    case INTROSPECTION_OP_TRAIT_GET_PROPERTIES:
        {
            const char *tname = NULL;

            const enum dicey_error err = validate_metatrait_name(path, &tname);

            return err ? err
                       : introspection_craft_filtered_elemlist(
                             registry, path, tname, DICEY_ELEMENT_TYPE_PROPERTY, response
                         );
        }

    case INTROSPECTION_OP_TRAIT_GET_SIGNALS:
        {
            const char *tname = NULL;

            const enum dicey_error err = validate_metatrait_name(path, &tname);

            return err ? err
                       : introspection_craft_filtered_elemlist(
                             registry, path, tname, DICEY_ELEMENT_TYPE_SIGNAL, response
                         );
        }

    default:
        assert(false);
        return TRACE(DICEY_EINVAL);
    }
}

const struct dicey_registry_builtin_set dicey_registry_introspection_builtins = {
    .objects = introspection_objects,
    .nobjects = DICEY_LENOF(introspection_objects),

    .traits = introspection_traits,
    .ntraits = DICEY_LENOF(introspection_traits),

    .handler = &perform_introspection_op,
};
