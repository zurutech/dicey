// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include "dicey/core/errors.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "traits.h"

static void object_free(void *const ptr) {
    struct dicey_object *const object = ptr;

    if (object) {
        dicey_hashset_delete(object->traits);

        free(object);
    }
}

static struct dicey_object *object_new(void) {
    struct dicey_object *const object = malloc(sizeof *object);
    if (!object) {
        return NULL;
    }

    struct dicey_hashset *const traits = dicey_hashset_new();
    if (!traits) {
        free(object);
        return NULL;
    }

    *object = (struct dicey_object) {
        .traits = traits,
    };

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

    if (!*path || *path != '/') {
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

static enum dicey_error registry_get_object(
    const struct dicey_registry *const registry,
    struct dicey_object **const dest,
    const char *const path
) {
    assert(registry && registry->_paths && dest && path);

    if (!path_is_valid(path)) {
        return DICEY_EINVAL;
    }

    struct dicey_object *const obj = dicey_hashtable_get(registry->_paths, path);
    if (!obj) {
        return DICEY_EPATH_NOT_FOUND;
    }

    *dest = obj;

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
        return DICEY_ENOMEM;

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
    const char *const path,
    struct dicey_trait *const trait
) {
    void *old_value = NULL;
    switch (dicey_hashtable_set(&registry->_traits, path, trait, &old_value)) {
    case DICEY_HASH_SET_FAILED:
        return DICEY_ENOMEM;

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached
        break;

    case DICEY_HASH_SET_ADDED:
        assert(!old_value);
        break;
    }

    return DICEY_OK;
}

bool dicey_object_implements(struct dicey_object *object, const char *trait) {
    assert(object && object->traits && trait);

    return dicey_hashset_contains(object->traits, trait);
}

void dicey_registry_deinit(struct dicey_registry *const registry) {
    assert(registry);

    if (registry) {
        dicey_hashtable_delete(registry->_paths, &object_free);
        dicey_hashtable_delete(registry->_traits, &trait_free);
    }
}

enum dicey_error dicey_registry_init(struct dicey_registry *const registry) {
    assert(registry);

    struct dicey_hashtable *const paths = dicey_hashtable_new();
    if (!paths) {
        return DICEY_ENOMEM;
    }

    struct dicey_hashtable *const traits = dicey_hashtable_new();
    if (!traits) {
        dicey_hashtable_delete(paths, &object_free);

        return DICEY_ENOMEM;
    }

    *registry = (struct dicey_registry) {
        ._paths = paths,
        ._traits = traits,
    };

    return DICEY_OK;
}

enum dicey_error dicey_registry_add_object(struct dicey_registry *const registry, const char *const path, ...) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return DICEY_EINVAL;
    }

    if (dicey_registry_contains_object(registry, path)) {
        return DICEY_EEXIST;
    }

    struct dicey_object *const object = object_new();
    if (!object) {
        return DICEY_ENOMEM;
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
            err = DICEY_ETRAIT_NOT_FOUND;

            break;
        }

        switch (dicey_hashset_add(&object->traits, trait)) {
        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            err = DICEY_EINVAL;
            break;

        case DICEY_HASH_SET_FAILED:
            err = DICEY_ENOMEM;
            break;
        }
    }

    va_end(traits);

    if (!err) {
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
        return DICEY_EINVAL;
    }

    if (dicey_registry_contains_object(registry, path)) {
        return DICEY_EEXIST;
    }

    struct dicey_object *const object = object_new();
    if (!object) {
        return DICEY_ENOMEM;
    }

    for (; traits; ++traits) {
        const char *const trait = *traits;
        if (!registry_trait_exists(registry, trait)) {
            object_free(object);

            return DICEY_ETRAIT_NOT_FOUND;
        }

        switch (dicey_hashset_add(&object->traits, trait)) {
        case DICEY_HASH_SET_ADDED:
            break;

        case DICEY_HASH_SET_UPDATED:
            object_free(object);

            return DICEY_EINVAL;

        case DICEY_HASH_SET_FAILED:
            object_free(object);

            return DICEY_ENOMEM;
        }
    }

    const enum dicey_error err = registry_add_object(registry, path, object);
    if (err) {
        object_free(object);
    }

    return err;
}

enum dicey_error dicey_registry_add_trait(struct dicey_registry *const registry, const char *const name, ...) {
    struct dicey_trait *const trait = dicey_trait_new(name);
    if (!trait) {
        return DICEY_ENOMEM;
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
    struct dicey_registry *registry,
    const char *name,
    const struct dicey_element_entry *elems,
    size_t count
) {
    assert(name && ((elems != NULL) == (count != 0)));

    struct dicey_trait *const trait = dicey_trait_new(name);
    if (!trait) {
        return DICEY_ENOMEM;
    }

    const struct dicey_element_entry *const end = elems + count;
    for (const struct dicey_element_entry *entry = elems; entry < end; ++entry) {
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

bool dicey_registry_contains_object(const struct dicey_registry *const registry, const char *const path) {
    return dicey_registry_get_object(registry, path);
}

bool dicey_registry_contains_trait(const struct dicey_registry *const registry, const char *const name) {
    return registry_trait_exists(registry, name);
}

struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *const registry,
    const char *const path,
    const char *const trait_name,
    const char *const elem
) {
    assert(registry && path && trait_name && elem);

    struct dicey_object *object = NULL;
    enum dicey_error err = registry_get_object(registry, &object, path);
    if (err != DICEY_OK) {
        return NULL;
    }

    assert(object);

    if (!dicey_object_implements(object, trait_name)) {
        return NULL;
    }

    const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
    if (!trait) {
        return NULL;
    }

    return dicey_trait_get_element(trait, elem);
}

struct dicey_element *dicey_registry_get_element_from_sel(
    const struct dicey_registry *const registry,
    const char *const path,
    const struct dicey_selector sel
) {
    return dicey_registry_get_element(registry, path, sel.trait, sel.elem);
}

struct dicey_object *dicey_registry_get_object(const struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    struct dicey_object *object = NULL;
    enum dicey_error err = registry_get_object(registry, &object, path);
    if (err != DICEY_OK) {
        return NULL;
    }

    assert(object);

    return object;
}

struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *const registry, const char *const name) {
    assert(registry && name);

    return dicey_hashtable_get(registry->_traits, name);
}

enum dicey_error dicey_registry_remove_object(struct dicey_registry *const registry, const char *const path) {
    assert(registry && path);

    if (!path_is_valid(path)) {
        return DICEY_EINVAL;
    }

    struct dicey_object *const object = dicey_hashtable_remove(registry->_paths, path);
    if (!object) {
        return DICEY_EPATH_NOT_FOUND;
    }

    object_free(object);

    return DICEY_OK;
}
