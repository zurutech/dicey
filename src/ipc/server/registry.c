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
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/views.h>
#include <dicey/ipc/builtins/introspection.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"
#include "sup/view-ops.h"

#include "builtins/builtins.h"

#include "registry-internal.h"

#define METATRAIT_FORMAT DICEY_REGISTRY_TRAITS_PATH "/%s"

static_assert(sizeof(NULL) == sizeof(void *), "NULL is not a pointer");

static void object_ref(struct dicey_object *const object) {
    assert(object && object->refcount > 0);

    ++object->refcount;
}

static void object_deref(void *const ptr) {
    struct dicey_object *const object = ptr;

    assert(!object || object->refcount > 0);

    if (object) {
        if (--object->refcount <= 0) {
            dicey_hashset_delete(object->aliases);
            dicey_hashset_delete(object->traits);

            xmlFree(object->cached_xml);
            free(object);
        }
    }
}

static struct dicey_object *object_new_with(struct dicey_hashset *traits) {
    struct dicey_object *const object = malloc(sizeof *object);
    if (!object) {
        return NULL;
    }

    // add the introspection trait
    if (dicey_hashset_add(&traits, DICEY_INTROSPECTION_TRAIT_NAME) == DICEY_HASH_SET_FAILED) {
        free(object);

        return NULL; // OOM
    }

    *object = (struct dicey_object) {
        .traits = traits,
        .refcount = 1, // the object is created with a refcount of 1
    };

    return object;
}

static struct dicey_object *object_new(void) {
    return object_new_with(NULL);
}

static void trait_free(void *const trait_ptr) {
    if (trait_ptr) {
        struct dicey_trait *const trait = (struct dicey_trait *) trait_ptr;

        dicey_trait_delete(trait);
    }
}

static bool path_is_valid(const char *const path) {
    assert(path);

    if (*path != '/') {
        return false;
    }

    const char *const end = path + strlen(path);
    if (path == end) {
        return false;
    }

    if (end[-1] == '/') {
        return false;
    }

    return true;
}

static enum dicey_error registry_remove_path(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    struct dicey_object *const obj = dicey_hashtable_remove(registry->paths, path);
    if (!obj) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    object_deref(obj);

    return DICEY_OK;
}

static enum dicey_error registry_del_object(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_object *const object = dicey_registry_get_object_mut(registry, path);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    // first, free all the aliases (the set contains strings borrowed from the hashtable)
    struct dicey_hashset_iter iter = dicey_hashset_iter_start(object->aliases);

    const char *alias = NULL;
    while (dicey_hashset_iter_next(&iter, &alias)) {
        assert(alias);

        // remove the alias from the hashtable
        const enum dicey_error err = registry_remove_path(registry, alias);
        DICEY_UNUSED(err);
        assert(!err); // the alias should always exist in the hashtable
    }

    // remove the main path from the hashtable. The object is now purged from the registry
    const bool success = dicey_hashtable_remove(registry->paths, object->main_path);
    DICEY_UNUSED(success);
    assert(success); // the main path should always exist in the hashtable

    // now that no references to the object exist, we can safely free it
    object_deref(object);

    return DICEY_OK;
}

static enum dicey_error registry_get_object_entry(
    const struct dicey_registry *const registry,
    struct dicey_object_entry *dest,
    const char *const path
) {
    assert(registry && dest && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_hashtable_entry entry = { 0 };
    struct dicey_object *const obj = dicey_hashtable_get_entry(registry->paths, path, &entry);
    if (!obj) {
        // deliberately not TRACE()ing here because this is a common case and often used for checking if an object
        // exists
        return DICEY_EPATH_NOT_FOUND;
    }

    *dest = (struct dicey_object_entry) {
        .path = entry.key,
        .object = obj,
    };

    return DICEY_OK;
}

static bool registry_trait_exists(const struct dicey_registry *const registry, const char *const trait) {
    assert(registry && trait);

    return dicey_hashtable_contains(registry->traits, trait);
}

static enum dicey_error registry_add_object(
    struct dicey_registry *const registry,
    const char *const path,
    struct dicey_object *const object
) {
    void *old_value = NULL;
    switch (dicey_hashtable_set(&registry->paths, path, object, &old_value)) {
    case DICEY_HASH_SET_FAILED:
        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached
        break;

    case DICEY_HASH_SET_ADDED:
        {
            assert(!old_value);

            // if the object does not have a main path, we set it to the one we just added
            if (!object->main_path) {
                // fetch the entry for the object we just added, in order to get a path owned by the hashtable
                struct dicey_object_entry entry = { 0 };
                const enum dicey_error err = registry_get_object_entry(registry, &entry, path);
                DICEY_UNUSED(err);
                assert(!err); // we just added the object, so it should always exist

                object->main_path = entry.path;
            }

            break;
        }
    }

    return DICEY_OK;
}

static enum dicey_error registry_add_trait(
    struct dicey_registry *const registry,
    const char *const trait_name,
    struct dicey_trait *const trait
) {
    // path of the metaobject that represents this trait under /dicey/registry
    const char *const metapath = dicey_registry_format_metaname(registry, METATRAIT_FORMAT, trait_name);
    if (!metapath) {
        return TRACE(DICEY_ENOMEM);
    }

    void *old_value = NULL;
    switch (dicey_hashtable_set(&registry->traits, trait_name, trait, &old_value)) {
    case DICEY_HASH_SET_FAILED:
        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached
        break;

    case DICEY_HASH_SET_ADDED:
        assert(!old_value);
        break;
    }

    // also add the associated metaobject for this trait
    return dicey_registry_add_object_with(registry, metapath, DICEY_TRAIT_TRAIT_NAME, NULL);
}

char *dicey_metaname_format(const char *const fmt, ...) {
    assert(fmt);

    va_list ap;
    va_start(ap, fmt);

    char *const result = dicey_metaname_vformat_to(NULL, fmt, ap);

    va_end(ap);

    return result;
}

char *dicey_metaname_format_to(struct dicey_view_mut *const buffer, const char *const fmt, ...) {
    assert(buffer && fmt);

    va_list ap;
    va_start(ap, fmt);

    char *const result = dicey_metaname_vformat_to(buffer, fmt, ap);

    va_end(ap);

    return result;
}

char *dicey_metaname_vformat_to(struct dicey_view_mut *const buffer_view, const char *const fmt, va_list ap) {
    assert(buffer_view && fmt);

    va_list ap_copy;
    va_copy(ap_copy, ap);

    char *buffer = buffer_view ? buffer_view->data : NULL;

    const int will_write = vsnprintf(NULL, 0, fmt, ap_copy);
    if (will_write < 0) {
        buffer = NULL;

        goto quit;
    }

    const size_t needed = (size_t) will_write + 1U;

    if (!buffer_view || buffer_view->len < needed) {
        buffer = realloc(buffer, needed);
        if (!buffer) {
            goto quit; // OOM
        }

        if (buffer_view) {
            *buffer_view = dicey_view_mut_from(buffer, needed);
        }
    }

    if (vsnprintf(buffer, needed, fmt, ap) < will_write) {
        buffer = NULL;
    }

quit:
    va_end(ap_copy);
    va_end(ap);

    return buffer;
}

const struct dicey_hashset *dicey_object_get_aliases(const struct dicey_object *const object) {
    assert(object); // always initialized

    return object->aliases; // null is empty
}

struct dicey_hashset *dicey_object_get_main_path(const struct dicey_object *object);

struct dicey_hashset *dicey_object_get_traits(const struct dicey_object *const object) {
    assert(object);

    return object->traits;
}

bool dicey_object_has_alias(const struct dicey_object *const object, const char *const alias) {
    assert(object && alias);

    // check if the alias is in the aliases set
    return dicey_hashset_contains(dicey_object_get_aliases(object), alias);
}

bool dicey_object_implements(const struct dicey_object *const object, const char *const trait) {
    assert(object && object->traits && trait);

    return dicey_hashset_contains(object->traits, trait);
}

struct dicey_element_entry dicey_object_element_entry_to_element_entry(
    const struct dicey_object_element_entry *const entry
) {
    return (struct dicey_element_entry) {
        .sel = entry->sel,
        .element = entry->element,
    };
}

void dicey_registry_deinit(struct dicey_registry *const registry) {
    assert(registry);

    if (registry) {
        dicey_hashtable_delete(registry->paths, &object_deref);
        dicey_hashtable_delete(registry->traits, &trait_free);

        free(registry->buffer.data);

        *registry = (struct dicey_registry) { 0 };
    }
}

enum dicey_error dicey_registry_init(struct dicey_registry *const registry) {
    assert(registry);

    *registry = (struct dicey_registry) { 0 };

    const enum dicey_error err = dicey_registry_populate_builtins(registry);
    if (err) {
        dicey_registry_deinit(registry);
    }

    return err;
}

enum dicey_error dicey_registry_add_object_with(struct dicey_registry *const registry, const char *const path, ...) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    if (dicey_registry_contains_object(registry, path)) {
        return TRACE(DICEY_EEXIST);
    }

    struct dicey_object *const object = object_new();
    if (!object) {
        return TRACE(DICEY_ENOMEM);
    }

    va_list traits;
    va_start(traits, path);

    enum dicey_error err = DICEY_OK;
    while (!err) {
        const char *const trait = va_arg(traits, const char *);
        if (!trait) {
            break;
        }

        if (!registry_trait_exists(registry, trait)) {
            err = TRACE(DICEY_ETRAIT_NOT_FOUND);

            break;
        }

        switch (dicey_hashset_add(&object->traits, trait)) {
        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            err = TRACE(DICEY_EINVAL);
            break;

        case DICEY_HASH_SET_FAILED:
            err = TRACE(DICEY_ENOMEM);
            break;
        }
    }

    va_end(traits);

    if (err) {
        object_deref(object);

        return err;
    }

    err = registry_add_object(registry, path, object);
    if (err) {
        object_deref(object);
    }

    return err;
}

enum dicey_error dicey_registry_add_object_with_trait_list(
    struct dicey_registry *const registry,
    const char *const path,
    const char *const *traits
) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    if (dicey_registry_contains_object(registry, path)) {
        return TRACE(DICEY_EEXIST);
    }

    struct dicey_object *const object = object_new();
    if (!object) {
        return TRACE(DICEY_ENOMEM);
    }

    for (; *traits; ++traits) {
        const char *const trait = *traits;
        if (!registry_trait_exists(registry, trait)) {
            object_deref(object);

            return TRACE(DICEY_ETRAIT_NOT_FOUND);
        }

        switch (dicey_hashset_add(&object->traits, trait)) {
        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            object_deref(object);

            return TRACE(DICEY_EINVAL);

        case DICEY_HASH_SET_FAILED:
            object_deref(object);

            return TRACE(DICEY_ENOMEM);
        }
    }

    const enum dicey_error err = registry_add_object(registry, path, object);
    if (err) {
        object_deref(object);
    }

    return err;
}

enum dicey_error dicey_registry_add_object_with_trait_set(
    struct dicey_registry *const registry,
    const char *const path,
    struct dicey_hashset *set
) {
    assert(registry && path && set);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    if (dicey_registry_contains_object(registry, path)) {
        return TRACE(DICEY_EEXIST);
    }

    struct dicey_hashset_iter iter = { 0 };
    const char *trait = NULL;

    while (dicey_hashset_iter_next(&iter, &trait)) {
        if (!registry_trait_exists(registry, trait)) {
            return TRACE(DICEY_ETRAIT_NOT_FOUND);
        }
    }

    struct dicey_object *const node = object_new_with(set);
    if (!node) {
        return TRACE(DICEY_ENOMEM);
    }

    const enum dicey_error err = registry_add_object(registry, path, node);
    if (err) {
        object_deref(node);
    }

    return err;
}

enum dicey_error dicey_registry_add_trait(struct dicey_registry *const registry, struct dicey_trait *const trait) {
    assert(registry && trait);

    if (dicey_registry_contains_trait(registry, trait->name)) {
        return TRACE(DICEY_EEXIST);
    }

    return registry_add_trait(registry, trait->name, trait);
}

enum dicey_error dicey_registry_add_trait_with(struct dicey_registry *const registry, const char *const name, ...) {
    if (dicey_registry_contains_trait(registry, name)) {
        return TRACE(DICEY_EEXIST);
    }

    struct dicey_trait *const trait = dicey_trait_new(name);
    if (!trait) {
        return TRACE(DICEY_ENOMEM);
    }

    va_list ap;
    va_start(ap, name);

    for (;;) {
        const char *const elem_name = va_arg(ap, const char *);
        if (!elem_name) {
            break;
        }

        enum dicey_error add_err = dicey_trait_add_element(trait, elem_name, va_arg(ap, struct dicey_element));
        if (add_err) {
            dicey_trait_delete(trait);

            va_end(ap);

            return add_err;
        }
    }

    va_end(ap);

    const enum dicey_error err = registry_add_trait(registry, name, trait);
    if (err) {
        dicey_trait_delete(trait);
    }

    return DICEY_OK;
}

enum dicey_error dicey_registry_add_trait_with_element_list(
    struct dicey_registry *const registry,
    const char *const name,
    const struct dicey_element_new_entry *const elems,
    const size_t count
) {
    assert(name && ((elems != NULL) == (count != 0)));

    struct dicey_trait *const trait = dicey_trait_new(name);
    if (!trait) {
        return TRACE(DICEY_ENOMEM);
    }

    const struct dicey_element_new_entry *const end = elems + count;
    for (const struct dicey_element_new_entry *entry = elems; entry < end; ++entry) {
        const enum dicey_error add_err = dicey_trait_add_element(
            trait,
            entry->name,
            (struct dicey_element) {
                .signature = entry->signature,
                .type = entry->type,
            }
        );

        if (add_err) {
            dicey_trait_delete(trait);

            return add_err;
        }
    }

    const enum dicey_error err = registry_add_trait(registry, name, trait);
    if (err) {
        dicey_trait_delete(trait);
    }

    return err;
}

enum dicey_error dicey_registry_alias_object(
    struct dicey_registry *const registry,
    const char *const path,
    const char *const alias
) {
    assert(registry && path && alias);

    if (!path_is_valid(path) || !path_is_valid(alias)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    if (!strcmp(path, alias)) {
        // aliasing an object to itself is not allowed
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_object *const object = dicey_hashtable_get(registry->paths, path);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    const char *const existing = dicey_registry_get_main_path(registry, alias);
    if (existing) {
        if (strcmp(existing, alias) && !strcmp(existing, object->main_path)) {
            // `alias` is already an alias that points to the target object - return DICEY_EEXIST
            return TRACE(DICEY_EEXIST);
        } else {
            // Return DICEY_EINVAL because the alias is already registered to a different object
            // or `alias` points to an object directly (may be the same)
            return TRACE(DICEY_EINVAL);
        }
    }

    object_ref(object); // we will add an object alias to the registry, so we need to increment the refcount

    enum dicey_error err = registry_add_object(registry, alias, object);
    if (err) {
        return err;
    }

    // not very efficient, we fetch the stuff again to get the string key owned by the registry
    struct dicey_object_entry alias_entry = { 0 };
    err = dicey_registry_get_object_entry(registry, alias, &alias_entry) ? DICEY_OK : DICEY_EPATH_NOT_FOUND;

    // note: path must be the alias path, because we're requested that, not the main path
    assert(!err && alias_entry.object == object && !strcmp(alias_entry.path, alias));

    // register the alias in the object's aliases
    enum dicey_hash_set_result res = dicey_hashset_add(&object->aliases, alias_entry.path);
    switch (res) {
    case DICEY_HASH_SET_ADDED:
        break;

    case DICEY_HASH_SET_UPDATED:
        // this should never happen, because we just checked that the alias does not exist
        assert(false);
        return TRACE(DICEY_EINVAL);

    case DICEY_HASH_SET_FAILED:
        return dicey_registry_remove_object(registry, alias);
    }

    return DICEY_OK;
}

bool dicey_registry_contains_element(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem
) {
    return dicey_registry_get_element(registry, path, trait_name, elem);
}

bool dicey_registry_contains_object(const struct dicey_registry *const registry, const char *const path) {
    return dicey_registry_get_object(registry, path);
}

bool dicey_registry_contains_trait(const struct dicey_registry *const registry, const char *const name) {
    return registry_trait_exists(registry, name);
}

enum dicey_error dicey_registry_delete_object(struct dicey_registry *const registry, const char *const name) {
    assert(registry && name);

    return registry_del_object(registry, name);
}

const char *dicey_registry_format_metaname(struct dicey_registry *registry, const char *const fmt, ...) {
    assert(registry && fmt);

    va_list ap;
    va_start(ap, fmt);

    const char *const result = dicey_metaname_vformat_to(&registry->buffer, fmt, ap);

    va_end(ap);

    return result;
}

const struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem
) {
    struct dicey_object_element_entry entry = { 0 };

    return dicey_registry_get_element_entry(registry, path, trait_name, elem, &entry) ? entry.element : NULL;
}

bool dicey_registry_get_element_entry(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem,
    struct dicey_object_element_entry *entry
) {
    assert(registry && path && trait_name && elem && entry);

    struct dicey_object_entry obj_entry = { 0 };
    enum dicey_error err = registry_get_object_entry(registry, &obj_entry, path);
    if (err) {
        return false;
    }

    assert(obj_entry.object && !strcmp(obj_entry.path, path));

    if (!dicey_object_implements(obj_entry.object, trait_name)) {
        return false;
    }

    const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
    if (!trait) {
        return false;
    }

    struct dicey_element_entry elem_entry = { 0 };
    bool res = dicey_trait_get_element_entry(trait, elem, &elem_entry);
    if (!res) {
        return false; // element does not exist
    }

    assert(elem_entry.element);

    *entry = (struct dicey_object_element_entry) {
        .main_path = obj_entry.object->main_path,
        .sel = elem_entry.sel,
        .element = elem_entry.element,
    };

    return true;
}

const struct dicey_element *dicey_registry_get_element_from_sel(
    const struct dicey_registry *const registry,
    const char *const path,
    const struct dicey_selector sel
) {
    return dicey_registry_get_element(registry, path, sel.trait, sel.elem);
}

bool dicey_registry_get_element_entry_from_sel(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel,
    struct dicey_object_element_entry *entry
) {
    return dicey_registry_get_element_entry(registry, path, sel.trait, sel.elem, entry);
}

const char *dicey_registry_get_main_path(const struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    const struct dicey_object *const object = dicey_registry_get_object(registry, path);

    return object ? object->main_path : NULL;
}

const struct dicey_object *dicey_registry_get_object(
    const struct dicey_registry *const registry,
    const char *const path
) {
    struct dicey_object_entry entry = { 0 };

    return dicey_registry_get_object_entry(registry, path, &entry) ? entry.object : NULL;
}

bool dicey_registry_get_object_entry(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_object_entry *entry
) {
    assert(registry && path && entry);

    enum dicey_error err = registry_get_object_entry(registry, entry, path);
    if (err) {
        return false;
    }

    assert(entry->object && !strcmp(entry->path, path));

    return true;
}

struct dicey_object *dicey_registry_get_object_mut(const struct dicey_registry *const registry, const char *path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return NULL;
    }

    return dicey_hashtable_get(registry->paths, path);
}

struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *const registry, const char *const name) {
    assert(registry && name);

    return dicey_hashtable_get(registry->traits, name);
}

enum dicey_error dicey_registry_remove_object(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_object *const object = dicey_hashtable_remove(registry->paths, path);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    object_deref(object);

    return DICEY_OK;
}

enum dicey_error dicey_registry_unalias_object(struct dicey_registry *const registry, const char *const alias) {
    assert(registry && alias);

    if (!path_is_valid(alias)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_object *const object = dicey_registry_get_object_mut(registry, alias);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    // remove the alias from the object's aliases
    const bool success = dicey_hashset_remove(object->aliases, alias);
    if (!success) {
        return TRACE(DICEY_EPATH_NOT_ALIAS); // alias does not exist
    }

    enum dicey_error err = registry_remove_path(registry, alias);
    if (err) {
        assert(false); // should never happen, it means the registry broke spectacularly its internal invariants
        return err;
    }

    return DICEY_OK;
}

enum dicey_error dicey_registry_walk_object_elements(
    const struct dicey_registry *const registry,
    const char *const path,
    dicey_registry_walk_fn *const callback,
    void *const user_data
) {
    assert(registry && path && callback);

    const struct dicey_object *const obj = dicey_registry_get_object(registry, path);
    if (!obj) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    if (!callback(
            registry, DICEY_REGISTRY_WALK_EVENT_OBJECT_START, path, (struct dicey_selector) { 0 }, NULL, NULL, user_data
        )) {
        return DICEY_OK;
    }

    struct dicey_hashset_iter iter = dicey_hashset_iter_start(obj->traits);
    const char *trait_name = NULL;

    while (dicey_hashset_iter_next(&iter, &trait_name)) {
        const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
        assert(trait);

        if (!callback(
                registry,
                DICEY_REGISTRY_WALK_EVENT_TRAIT_START,
                path,
                (struct dicey_selector) {
                    .trait = trait_name,
                },
                trait,
                NULL,
                user_data
            )) {
            return DICEY_OK;
        }

        struct dicey_trait_iter trait_iter = dicey_trait_iter_start(trait);
        const char *elem_name = NULL;
        struct dicey_element elem = { 0 };

        while (dicey_trait_iter_next(&trait_iter, &elem_name, &elem)) {
            const struct dicey_selector sel = {
                .trait = trait_name,
                .elem = elem_name,
            };

            if (!callback(registry, DICEY_REGISTRY_WALK_EVENT_ELEMENT, path, sel, trait, &elem, user_data)) {
                return DICEY_OK;
            }
        }

        if (!callback(
                registry,
                DICEY_REGISTRY_WALK_EVENT_TRAIT_END,
                path,
                (struct dicey_selector) {
                    .trait = trait_name,
                },
                trait,
                NULL,
                user_data
            )) {
            return DICEY_OK;
        }
    }

    callback(
        registry, DICEY_REGISTRY_WALK_EVENT_OBJECT_END, path, (struct dicey_selector) { 0 }, NULL, NULL, user_data
    );

    return DICEY_OK;
}
