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

/**
 * @brief Represents an object inside of a registry.
 */
struct dicey_object {
    struct dicey_hashset *traits; /**< A set containing the names of traits that this object implements. */
};

/**
 * @brief Given a trait name, returns true if the object at `object` implements it.
 * @param object The object to check.
 * @param trait  The name of the trait to check for.
 * @return       True if the object implements the trait, false otherwise.
 */
DICEY_EXPORT bool dicey_object_implements(struct dicey_object *object, const char *trait);

/**
 * @brief Represents a trait element inside of a trait definition.
 */
struct dicey_element_entry {
    enum dicey_element_type
        type; /**< The type of the element, which can be one among Operation, Property, or Signal. */

    const char *name;      /**< The name of the element. Conventionally only composed of ASCII letters,  */
    const char *signature; /**< The signature of the element. */
};

/**
 * @brief Represents the Dicey object registry, which contains definitions of both objects and traits.
 *        The registry is used by the server to determine whether a given request is valid or not, depending on whether
 *        the object and trait exist and the object implements the trait.
 */
struct dicey_registry {
    // note: While the paths are technically hierarchical, this has zero to no effect on the actual implementation.
    //       The paths are simply used as a way to identify objects and traits, and "directory-style" access is not
    //       of much use ATM. If this ever becomes useful, it's simple to implement - just swap the hashtable for a
    //       sorted tree or something similar.
    struct dicey_hashtable *_paths;

    struct dicey_hashtable *_traits;
};

/**
 * @brief Deinitialises a `dicey_registry`, releasing any resources it may own and resetting it to an empty state.
 * @param registry The registry to deinitialise.
 */
DICEY_EXPORT void dicey_registry_deinit(struct dicey_registry *registry);

/**
 * @brief Initialises a `dicey_registry`, preparing it for use.
 * @param registry The registry to initialise.
 * @return         Error code. Possible values are:
 *                 - OK: the registry was successfully initialised
 *                 - ENOMEM: memory allocation failed
 */
DICEY_EXPORT enum dicey_error dicey_registry_init(struct dicey_registry *registry);

/**
 * @brief Adds an object to the registry at a given path, with a variable number of traits.
 * @note All the values this function takes are not required to be valid after the function returns. The function will
 *       copy the values it needs.
 * @param registry The registry to add the object to.
 * @param path     The path at which to add the object.
 * @param ...      A list of `const char*` trait names that the object implements. This list must be terminated with a
 *                 NULL pointer.
 * @return         Error code. Possible values are:
 *                 - OK: the object was successfully added to the registry
 *                 - EEXISTS: an object already exists at the given path
 *                 - ENOMEM: memory allocation failed
 *                 - EINVAL: one or more of the arguments is invalid (e.g. a trait name is duplicated)
 *                 - ETRAIT_NOT_FOUND: one (or more) of the specified traits does not exist
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_object_with(struct dicey_registry *registry, const char *path, ...);

/**
 * @brief Adds an object to the registry at a given path, with a list of traits.
 * @note All the values this function takes are not required to be valid after the function returns. The function will
 *       copy the values it needs.
 * @param registry The registry to add the object to.
 * @param path     The path at which to add the object.
 * @param trait    A list of `const char*` trait names that the object implements, terminated by a NULL pointer.
 * @return         Error code. Possible values are:
 *                 - OK: the object was successfully added to the registry
 *                 - EEXISTS: an object already exists at the given path
 *                 - ENOMEM: memory allocation failed
 *                 - EINVAL: one or more of the arguments is invalid (e.g. a trait name is duplicated)
 *                 - ETRAIT_NOT_FOUND: one (or more) of the specified traits does not exist
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_object_with_trait_list(
    struct dicey_registry *registry,
    const char *path,
    const char *const *trait
);

/**
 * @brief Adds an object to the registry at a given path, with a set of traits (which must all be valid).
 * @note  The ownership of the set is transferred to the registry.
 * @param registry The registry to add the object to.
 * @param path     The path at which to add the object.
 * @param set      A set of `const char*` trait names that the object implements. The set must be valid and only contain
 *                 valid trait names (i.e. traits that exist in the registry). The ownership of the set will be
 *                 transferred to the registry.
 * @return         Error code. Possible values are:
 *                 - OK: the object was successfully added to the registry
 *                 - EEXISTS: an object already exists at the given path
 *                 - ENOMEM: memory allocation failed
 *                 - EINVAL: one or more of the arguments is invalid (e.g. a trait name is duplicated)
 *                 - ETRAIT_NOT_FOUND: one (or more) of the specified traits does not exist
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_object_with_trait_set(
    struct dicey_registry *registry,
    const char *path,
    struct dicey_hashset *set
);

/**
 * @brief Adds a trait to the registry. The trait must be valid and not already exist in the registry.
 * @note  The ownership of the trait is transferred to the registry.
 * @param registry The registry to add the trait to.
 * @param trait    The trait to add.
 * @return         Error code. Possible values are:
 *                 - OK: the trait was successfully added to the registry
 *                 - EEXISTS: a trait with the same name already exists
 *                 - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_trait(struct dicey_registry *registry, struct dicey_trait *trait);

/**
 * @brief Adds a trait to the registry with a variable number of elements.
 * @note  All the values this function takes are not required to be valid after the function returns. The function will
 *        copy the values it needs.
 * @param registry The registry to add the trait to.
 * @param name     The name of the trait.
 * @param ...      A list of alternating `const char*` and `struct dicey_element` respectively representing the name and
 *                 details of an element this trait contains.
 *                 This list must be terminated with a NULL pointer.
 * @return         Error code. Possible values are:
 *                 - OK: the trait was successfully added to the registry
 *                 - EEXISTS: a trait with the same name already exists
 *                 - ENOMEM: memory allocation failed
 *                 - EINVAL: one or more of the arguments is invalid (e.g. an element name is duplicated)
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_trait_with(struct dicey_registry *registry, const char *name, ...);

/**
 * @brief Adds a trait to the registry with a list of elements.
 * @param registry The registry to add the trait to.
 * @param name     The name of the trait.
 * @param elems    A list of `struct dicey_element_entry` elements that the trait contains.
 * @param count    The number of elements in the list.
 * @return         Error code. Possible values are:
 *                 - OK: the trait was successfully added to the registry
 *                 - EEXISTS: a trait with the same name already exists
 *                 - ENOMEM: memory allocation failed
 *                 - EINVAL: one or more of the arguments is invalid (e.g. an element name is duplicated)
 */
DICEY_EXPORT enum dicey_error dicey_registry_add_trait_with_element_list(
    struct dicey_registry *registry,
    const char *name,
    const struct dicey_element_entry *elems,
    size_t count
);

/**
 * @brief Checks if an object exists at a given path in the registry.
 * @param registry The registry to check.
 * @param path     The path to check.
 * @return         True if an object exists at the given path, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_contains_object(const struct dicey_registry *registry, const char *path);

/**
 * @brief Checks if a trait named `name` exists in the registry.
 * @param registry The registry to check.
 * @param name     The name of the trait to check.
 * @return         True if the trait exists, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_contains_trait(const struct dicey_registry *registry, const char *name);

/**
 * @brief Deletes a trait from the registry.
 * @note  The ownership of the trait is transferred to the registry.
 * @param registry The registry to add the trait to.
 * @param trait    The trait to add.
 * @return         Error code. Possible values are:
 *                 - OK: the trait was successfully added to the registry
 *                 - EEXISTS: a trait with the same name already exists
 *                 - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_registry_delete_object(struct dicey_registry *registry, const char *const name);

/**
 * @brief Gets an element from a trait.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param trait    The name of the trait.
 * @param elem     The name of the element.
 * @return         A pointer to the element, or NULL if the element does not exist.
 */
DICEY_EXPORT struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait,
    const char *elem
);

/**
 * @brief Gets an element from a trait using a selector.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param sel      A valid selector referencing the element.
 * @return         A pointer to the element, or NULL if the element does not exist.
 */
DICEY_EXPORT struct dicey_element *dicey_registry_get_element_from_sel(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel
);

/**
 * @brief Gets the object at a given path.
 * @param registry The registry to get the object from.
 * @param path     The path to the object.
 * @return         A pointer to the object, or NULL if the object does not exist.
 */
DICEY_EXPORT struct dicey_object *dicey_registry_get_object(const struct dicey_registry *registry, const char *path);

/**
 * @brief Gets the trait with the given name.
 * @param registry The registry to get the trait from.
 * @param name     The name of the trait.
 * @return         A pointer to the trait, or NULL if the trait does not exist.
 */
DICEY_EXPORT struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *registry, const char *name);

/**
 * @brief Removes an object from the registry.
 * @param registry The registry to remove the object from.
 * @param path     The path to the object.
 * @return         Error code. Possible values are:
 *                 - OK: the object was successfully removed from the registry
 *                 - EINVAL: the path is malformed
 *                 - EPATH_NOT_FOUND: no object exists at the given path
 */
DICEY_EXPORT enum dicey_error dicey_registry_remove_object(struct dicey_registry *registry, const char *path);

#if defined(__cplusplus)
}
#endif

#endif // PIIPYFREDI_REGISTRY_H
