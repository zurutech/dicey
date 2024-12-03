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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <libxml/xmlstring.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/type.h>
#include <dicey/ipc/builtins/introspection.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"

#include "../../registry-internal.h"

#include "introspection-internal.h"

static enum dicey_error craft_bool_response(
    const bool value,
    const char *const path,
    const char *const trait,
    const char *const elem,
    struct dicey_packet *const dest
) {
    assert(path && dest && trait && elem);

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = introspection_init_builder(&builder, path, trait, elem);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_value(
        &builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_BOOL,
            .boolean = value,
        }
    );

    if (err) {
        goto fail;
    }

    return dicey_message_builder_build(&builder, dest);

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

static enum dicey_error populate_element_kind(
    const enum dicey_element_type type,
    struct dicey_value_builder *const value
) {
    assert(value);

    return dicey_value_builder_set(
        value,
        (struct dicey_arg) {
            .type = DICEY_TYPE_BYTE,
            .byte = (dicey_byte) type,
        }
    );
}

// (cs)
static enum dicey_error populate_element_struct(
    const struct dicey_element *const element,
    struct dicey_value_builder *const value
) {
    assert(element && value);

    enum dicey_error err = dicey_value_builder_tuple_start(value);
    if (err) {
        return err;
    }

    struct dicey_value_builder kind_builder = { 0 };
    err = dicey_value_builder_next(value, &kind_builder);
    if (err) {
        return err;
    }

    err = populate_element_kind(element->type, &kind_builder);
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
            .str = element->signature,
        }
    );

    if (err) {
        return err;
    }

    if (element->type == DICEY_ELEMENT_TYPE_PROPERTY) {
        struct dicey_value_builder ro_builder = { 0 };
        err = dicey_value_builder_next(value, &ro_builder);
        if (err) {
            return err;
        }

        err = dicey_value_builder_set(
            &ro_builder,
            (struct dicey_arg) {
                .type = DICEY_TYPE_BOOL,
                .boolean = element->readonly,
            }
        );

        if (err) {
            return err;
        }
    }

    return dicey_value_builder_tuple_end(value);
}

static enum dicey_error populate_element_entry(
    const char *const name,
    const struct dicey_element *const element,
    struct dicey_value_builder *const value
) {
    assert(name && element && value);

    enum dicey_error err = dicey_value_builder_pair_start(value);
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

    struct dicey_value_builder elem_builder = { 0 };
    err = dicey_value_builder_next(value, &elem_builder);
    if (err) {
        return err;
    }

    err = populate_element_struct(element, &elem_builder);
    if (err) {
        return err;
    }

    return dicey_value_builder_pair_end(value);
}

static enum dicey_error populate_trait_element_list(
    struct dicey_trait_iter iter,
    struct dicey_value_builder *const value
) {
    assert(value);

    enum dicey_error err = dicey_value_builder_array_start(value, DICEY_TYPE_PAIR);
    if (err) {
        return err;
    }

    const char *elem_name = NULL;
    struct dicey_element elem = { 0 };
    while (dicey_trait_iter_next(&iter, &elem_name, &elem)) {
        struct dicey_value_builder trait_elem_builder = { 0 };
        err = dicey_value_builder_next(value, &trait_elem_builder);

        if (err) {
            return err;
        }

        err = populate_element_entry(elem_name, &elem, &trait_elem_builder);
        if (err) {
            return err;
        }
    }

    return dicey_value_builder_array_end(value);
}

static enum dicey_error populate_trait_entry(
    const struct dicey_trait *const trait,
    const char *const name,
    struct dicey_value_builder *const value
) {
    assert(trait && name && value);

    enum dicey_error err = dicey_value_builder_pair_start(value);
    if (err) {
        return err;
    }

    struct dicey_value_builder trait_name_builder = { 0 };
    err = dicey_value_builder_next(value, &trait_name_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(
        &trait_name_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_STR,
            .str = name,
        }
    );

    if (err) {
        return err;
    }

    struct dicey_value_builder trait_elist_builder = { 0 };
    err = dicey_value_builder_next(value, &trait_elist_builder);
    if (err) {
        return err;
    }

    err = populate_trait_element_list(dicey_trait_iter_start(trait), &trait_elist_builder);
    if (err) {
        return err;
    }

    return dicey_value_builder_pair_end(value);
}

static enum dicey_error populate_object_traitlist(
    const struct dicey_registry *const registry,
    const struct dicey_hashset *const trait_list,
    struct dicey_value_builder *const dest
) {
    assert(registry && trait_list && dest);

    enum dicey_error err = dicey_value_builder_array_start(dest, DICEY_TYPE_PAIR);
    if (err) {
        return err;
    }

    struct dicey_hashset_iter iter = dicey_hashset_iter_start(trait_list);

    const char *trait_name = NULL;
    while (dicey_hashset_iter_next(&iter, &trait_name)) {
        assert(trait_name);

        const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
        assert(trait); // can't ever be null

        struct dicey_value_builder trait_builder = { 0 };
        err = dicey_value_builder_next(dest, &trait_builder);
        if (err) {
            return err;
        }

        err = populate_trait_entry(trait, trait_name, &trait_builder);
        if (err) {
            return err;
        }
    }

    return dicey_value_builder_array_end(dest);
}

enum dicey_error introspection_check_element_exists(
    const struct dicey_registry *const registry,
    const char *const path,
    const struct dicey_selector sel,
    struct dicey_packet *const dest
) {
    assert(registry && path && dicey_selector_is_valid(sel) && dest);

    const bool exists = dicey_registry_contains_element(registry, path, sel.trait, sel.elem);

    return craft_bool_response(
        exists, DICEY_REGISTRY_PATH, DICEY_REGISTRY_TRAIT_NAME, DICEY_REGISTRY_ELEMENT_EXISTS_OP_NAME, dest
    );
}

enum dicey_error introspection_check_path_exists(
    const struct dicey_registry *const registry,
    const char *const path,
    struct dicey_packet *const dest
) {
    assert(registry && path && dest);

    const bool exists = dicey_registry_contains_object(registry, path);

    return craft_bool_response(
        exists, DICEY_REGISTRY_PATH, DICEY_REGISTRY_TRAIT_NAME, DICEY_REGISTRY_PATH_EXISTS_OP_NAME, dest
    );
}

enum dicey_error introspection_check_trait_exists(
    const struct dicey_registry *const registry,
    const char *const trait,
    struct dicey_packet *const dest
) {
    assert(registry && trait && dest);

    const bool exists = dicey_registry_contains_trait(registry, trait);

    return craft_bool_response(
        exists, DICEY_REGISTRY_PATH, DICEY_REGISTRY_TRAIT_NAME, DICEY_REGISTRY_TRAIT_EXISTS_OP_NAME, dest
    );
}

enum dicey_error introspection_craft_pathlist(
    const struct dicey_registry *const registry,
    struct dicey_packet *const dest
) {
    assert(registry && dest);

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = introspection_init_builder(
        &builder, DICEY_REGISTRY_PATH, DICEY_REGISTRY_TRAIT_NAME, DICEY_REGISTRY_OBJECTS_PROP_NAME
    );

    if (err) {
        goto fail;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        goto fail;
    }

    err = dicey_value_builder_array_start(&value_builder, DICEY_TYPE_PATH);
    if (err) {
        goto fail;
    }

    // TODO: implement an actual API to iterate over the registry's paths. We can do this here because this is a private
    // registry function
    struct dicey_hashtable_iter iter = dicey_hashtable_iter_start(registry->_paths);

    const char *path = NULL;
    while (dicey_hashtable_iter_next(&iter, &path, NULL)) {
        assert(path);

        struct dicey_value_builder element = { 0 };
        err = dicey_value_builder_next(&value_builder, &element);
        if (err) {
            goto fail;
        }

        err = dicey_value_builder_set(
            &element,
            (struct dicey_arg) {
                .type = DICEY_TYPE_PATH,
                .str = path,
            }
        );

        if (err) {
            goto fail;
        }
    }

    err = dicey_value_builder_array_end(&value_builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_value_end(&builder, &value_builder);
    if (err) {
        goto fail;
    }

    return dicey_message_builder_build(&builder, dest);

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

enum dicey_error introspection_craft_traitlist(
    const struct dicey_registry *const registry,
    struct dicey_packet *const dest
) {
    assert(registry && dest);

    struct dicey_hashtable_iter iter = dicey_hashtable_iter_start(registry->_traits);

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err = introspection_init_builder(
        &builder, DICEY_REGISTRY_PATH, DICEY_REGISTRY_TRAIT_NAME, DICEY_REGISTRY_TRAITS_PROP_NAME
    );
    if (err) {
        return err;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_array_start(&value_builder, DICEY_TYPE_STR);
    if (err) {
        return err;
    }

    const char *trait_name = NULL;
    while (dicey_hashtable_iter_next(&iter, &trait_name, NULL)) {
        assert(trait_name);

        struct dicey_value_builder trait_builder = { 0 };
        err = dicey_value_builder_next(&value_builder, &trait_builder);
        if (err) {
            return err;
        }

        err = dicey_value_builder_set(
            &trait_builder,
            (struct dicey_arg) {
                .type = DICEY_TYPE_STR,
                .str = trait_name,
            }
        );

        if (err) {
            return err;
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

enum dicey_error introspection_dump_object(
    struct dicey_registry *const registry,
    const char *const path,
    struct dicey_packet *const dest
) {
    assert(registry && path && dest);

    const struct dicey_object *obj = dicey_registry_get_object(registry, path);
    if (!obj) {
        return TRACE(DICEY_ENOENT);
    }

    struct dicey_message_builder builder = { 0 };
    enum dicey_error err =
        introspection_init_builder(&builder, path, DICEY_INTROSPECTION_TRAIT_NAME, DICEY_INTROSPECTION_DATA_PROP_NAME);
    if (err) {
        return err;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        return err;
    }

    err = populate_object_traitlist(registry, obj->traits, &value_builder);
    if (err) {
        return err;
    }

    err = dicey_message_builder_value_end(&builder, &value_builder);
    if (err) {
        return err;
    }

    return dicey_message_builder_build(&builder, dest);
}

enum dicey_error introspection_dump_xml(
    struct dicey_registry *const registry,
    const char *const path,
    struct dicey_packet *const dest
) {
    assert(registry && path && dest);

    struct dicey_object *const obj = dicey_registry_get_object_mut(registry, path);
    if (!obj) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    // ensure the object's XML is populated and get its serialised form
    // note: DO NOT FREE THIS! It's cached internally per-object inside of dicey_object, and will be freed when the
    // object is destroyed
    const xmlChar *xml = NULL;
    enum dicey_error err = introspection_object_populate_xml(registry, path, obj, &xml);
    if (err) {
        return err;
    }

    struct dicey_message_builder builder = { 0 };
    err = introspection_init_builder(&builder, path, DICEY_INTROSPECTION_TRAIT_NAME, DICEY_INTROSPECTION_XML_PROP_NAME);
    if (err) {
        return err;
    }

    struct dicey_value_builder value_builder = { 0 };
    err = dicey_message_builder_value_start(&builder, &value_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(
        &value_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_STR,
            // Look, I know this looks bad, but it's not my fault. It's libxml2 that's an obsolete pile of crap and
            // it does all sorts of weird things with its types. I'm just trying to make it work here.
            .str = (const char *) xml,
        }
    );

    if (err) {
        return err;
    }

    err = dicey_message_builder_value_end(&builder, &value_builder);
    if (err) {
        return err;
    }

    return dicey_message_builder_build(&builder, dest);
}
