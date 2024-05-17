// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>

#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/views.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"
#include "sup/view-ops.h"

#include "introspection/introspection.h"

#include "registry-internal.h"

#define METATRAIT_FORMAT DICEY_REGISTRY_TRAITS_PATH "/%s"

static_assert(sizeof(NULL) == sizeof(void *), "NULL is not a pointer");

static const char *metatrait_name_for(struct dicey_registry *const registry, const char *const trait_name) {
    assert(registry && trait_name);

    const int will_write = snprintf(NULL, 0, METATRAIT_FORMAT, trait_name);
    if (will_write < 0) {
        return NULL; // probably very bad
    }

    const size_t needed = (size_t) will_write + 1U;

    char *buffer = registry->_buffer.data;

    if (registry->_buffer.len < needed) {
        buffer = realloc(buffer, needed);
        if (!buffer) {
            return NULL;
        }

        registry->_buffer = dicey_view_mut_from(buffer, needed);
    }

    if (snprintf(buffer, needed, METATRAIT_FORMAT, trait_name) < will_write) {
        return NULL; // probably very bad
    }

    return buffer;
}

static void object_free(void *const ptr) {
    struct dicey_object *const object = ptr;

    if (object) {
        dicey_hashset_delete(object->traits);

        xmlFree(object->_cached_xml);
        free(object);
    }
}

static struct dicey_object *object_new_with(struct dicey_hashset *traits) {
    assert(traits);

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
    };

    return object;
}

static struct dicey_object *object_new(void) {
    struct dicey_hashset *const traits = dicey_hashset_new();
    if (!traits) {
        return NULL;
    }

    struct dicey_object *const object = object_new_with(traits);
    if (!object) {
        dicey_hashset_delete(traits);

        return NULL;
    }

    return object;
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

static enum dicey_error registry_del_object(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_object *const object = dicey_hashtable_remove(registry->_paths, path);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    object_free(object);

    return DICEY_OK;
}

static enum dicey_error registry_get_object_entry(
    const struct dicey_registry *const registry,
    struct dicey_object_entry *dest,
    const char *const path
) {
    assert(registry && registry->_paths && dest && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_hashtable_entry entry = { 0 };
    struct dicey_object *const obj = dicey_hashtable_get_entry(registry->_paths, path, &entry);
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

    return dicey_hashtable_contains(registry->_traits, trait);
}

static enum dicey_error registry_add_object(
    struct dicey_registry *const registry,
    const char *const path,
    struct dicey_object *const object
) {
    void *old_value = NULL;
    switch (dicey_hashtable_set(&registry->_paths, path, object, &old_value)) {
    case DICEY_HASH_SET_FAILED:
        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached
        break;

    case DICEY_HASH_SET_ADDED:
        assert(!old_value);
        break;
    }

    return DICEY_OK;
}

static enum dicey_error registry_add_trait(
    struct dicey_registry *const registry,
    const char *const trait_name,
    struct dicey_trait *const trait
) {
    // path of the metaobject that represents this trait under /dicey/registry
    const char *const metapath = metatrait_name_for(registry, trait_name);
    if (!metapath) {
        return TRACE(DICEY_ENOMEM);
    }

    void *old_value = NULL;
    switch (dicey_hashtable_set(&registry->_traits, trait_name, trait, &old_value)) {
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

bool dicey_object_implements(const struct dicey_object *const object, const char *const trait) {
    assert(object && object->traits && trait);

    return dicey_hashset_contains(object->traits, trait);
}

void dicey_registry_deinit(struct dicey_registry *const registry) {
    assert(registry);

    if (registry) {
        dicey_hashtable_delete(registry->_paths, &object_free);
        dicey_hashtable_delete(registry->_traits, &trait_free);

        free(registry->_buffer.data);

        *registry = (struct dicey_registry) { 0 };
    }
}

enum dicey_error dicey_registry_init(struct dicey_registry *const registry) {
    assert(registry);

    struct dicey_hashtable *const paths = dicey_hashtable_new();
    if (!paths) {
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_hashtable *const traits = dicey_hashtable_new();
    if (!traits) {
        dicey_hashtable_delete(paths, &object_free);

        return TRACE(DICEY_ENOMEM);
    }

    *registry = (struct dicey_registry) {
        ._paths = paths,
        ._traits = traits,
        ._buffer = dicey_view_mut_from(NULL, 0),
    };

    const enum dicey_error err = dicey_registry_populate_defaults(registry);
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
        object_free(object);

        return err;
    }

    err = registry_add_object(registry, path, object);
    if (err) {
        object_free(object);
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
            object_free(object);

            return TRACE(DICEY_ETRAIT_NOT_FOUND);
        }

        switch (dicey_hashset_add(&object->traits, trait)) {
        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            object_free(object);

            return TRACE(DICEY_EINVAL);

        case DICEY_HASH_SET_FAILED:
            object_free(object);

            return TRACE(DICEY_ENOMEM);
        }
    }

    const enum dicey_error err = registry_add_object(registry, path, object);
    if (err) {
        object_free(object);
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

    struct dicey_object *const object = object_new_with(set);
    if (!object) {
        return TRACE(DICEY_ENOMEM);
    }

    const enum dicey_error err = registry_add_object(registry, path, object);
    if (err) {
        object_free(object);
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

const struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem
) {
    struct dicey_element_entry entry = { 0 };

    return dicey_registry_get_element_entry(registry, path, trait_name, elem, &entry) ? entry.element : NULL;
}

bool dicey_registry_get_element_entry(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem,
    struct dicey_element_entry *const entry
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

    return dicey_trait_get_element_entry(trait, elem, entry);
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
    struct dicey_element_entry *entry
) {
    return dicey_registry_get_element_entry(registry, path, sel.trait, sel.elem, entry);
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

struct dicey_object *dicey_registry_get_object_mut(
    const struct dicey_registry *const registry,
    const char *const path
) {
    assert(registry && path);

    return dicey_hashtable_get(registry->_paths, path);
}

struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *const registry, const char *const name) {
    assert(registry && name);

    return dicey_hashtable_get(registry->_traits, name);
}

enum dicey_error dicey_registry_remove_object(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return TRACE(DICEY_EPATH_MALFORMED);
    }

    struct dicey_object *const object = dicey_hashtable_remove(registry->_paths, path);
    if (!object) {
        return TRACE(DICEY_EPATH_NOT_FOUND);
    }

    object_free(object);

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
