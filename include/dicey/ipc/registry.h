// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(PIIPYFREDI_REGISTRY_H)
#define PIIPYFREDI_REGISTRY_H

#include <stdbool.h>

#include "../core/errors.h"
#include "../core/hashset.h"
#include "../core/hashtable.h"
#include "../core/type.h"

#include "traits.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dicey_object {
    struct dicey_hashset *traits;
};

DICEY_EXPORT bool dicey_object_implements(struct dicey_object *object, const char *trait);

struct dicey_registry {
    // note: While the paths are technically hierarchical, this has zero to no effect on the actual implementation.
    //       The paths are simply used as a way to identify objects and traits, and "directory-style" access is not
    //       of much use ATM. If this ever becomes useful, it's simple to implement - just swap the hashtable for a
    //       sorted tree or something similar.
    struct dicey_hashtable *_paths;

    struct dicey_hashtable *_traits;
};

DICEY_EXPORT void dicey_registry_deinit(struct dicey_registry *registry);
DICEY_EXPORT enum dicey_error dicey_registry_init(struct dicey_registry *registry);

DICEY_EXPORT enum dicey_error dicey_registry_add_object(struct dicey_registry *registry, const char *path, ...);
DICEY_EXPORT enum dicey_error dicey_registry_add_object_with_trait_list(
    struct dicey_registry *registry,
    const char *path,
    const char *const *trait
);

DICEY_EXPORT enum dicey_error dicey_registry_add_trait(struct dicey_registry *registry, const char *name, ...);
DICEY_EXPORT enum dicey_error dicey_registry_add_trait_with_element_list(
    struct dicey_registry *registry,
    const char *name,
    const struct dicey_element_entry *elems,
    size_t count
);

DICEY_EXPORT bool dicey_registry_contains_object(const struct dicey_registry *registry, const char *path);
DICEY_EXPORT bool dicey_registry_contains_trait(const struct dicey_registry *registry, const char *name);
DICEY_EXPORT struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait,
    const char *elem
);
DICEY_EXPORT struct dicey_element *dicey_registry_get_element_from_sel(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel
);
DICEY_EXPORT struct dicey_object *dicey_registry_get_object(const struct dicey_registry *registry, const char *path);
DICEY_EXPORT struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *registry, const char *name);
DICEY_EXPORT enum dicey_error dicey_registry_remove_object(struct dicey_registry *registry, const char *path);

#if defined(__cplusplus)
}
#endif

#endif // PIIPYFREDI_REGISTRY_H
