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

#if !defined(PIIPYFREDI_REGISTRY_H)
#define PIIPYFREDI_REGISTRY_H

#include <stdbool.h>

#include "../core/errors.h"
#include "../core/hashset.h"
#include "../core/hashtable.h"
#include "../core/type.h"
#include "../core/views.h"

#include "traits.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents an object inside of a registry.
 */
struct dicey_object;

/**
 * @brief Returns the paths a given object is aliased at.
 * @param object The object to get the traits of.
 * @return       A pointer to a hashset containing the names of the traits that the object implements.
 */
DICEY_EXPORT const struct dicey_hashset *dicey_object_get_aliases(const struct dicey_object *object);

/**
 * @brief Returns the paths a given object is aliased at.
 * @param object The object to get the traits of.
 * @return       A pointer to a hashset containing the names of the traits that the object implements.
 */
DICEY_EXPORT struct dicey_hashset *dicey_object_get_main_path(const struct dicey_object *object);

/**
 * @brief Returns the traits of the given object.
 * @param object The object to get the traits of.
 * @return       A pointer to a hashset containing the names of the traits that the object implements.
 */
DICEY_EXPORT struct dicey_hashset *dicey_object_get_traits(const struct dicey_object *object);

/**
 * @brief Checks if the object is aliased at a given path.
 * @param object The object to check.
 * @param alias The alias to check for.
 * @return True if the object is aliased at the given path, false otherwise.
 */
DICEY_EXPORT bool dicey_object_has_alias(const struct dicey_object *object, const char *alias);

/**
 * @brief Represents an object entry inside of a registry.
 */
struct dicey_object_entry {
    /**< The path of this object. This string is valid for the entire lifetime of the object */
    /**< Note: if the object is aliased, this will be the concrete path, not the aliased path */
    const char *path;
    const struct dicey_object *object; /**< The object itself. */
};

/**
 * @brief Given a trait name, returns true if the object at `object` implements it.
 * @param object The object to check.
 * @param trait  The name of the trait to check for.
 * @return       True if the object implements the trait, false otherwise.
 */
DICEY_EXPORT bool dicey_object_implements(const struct dicey_object *object, const char *trait);

/**
 * @brief Structure used when adding a new element to a trait.
 */
struct dicey_element_new_entry {
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
struct dicey_registry;

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
    const struct dicey_element_new_entry *elems,
    size_t count
);

/**
 * @brief Creates an object alias in the registry. The alias is a new path that points to the same object as the
 * original path
 * @note  Deleting an object will also delete its aliases.
 * @param registry The registry to create the alias in.
 * @param path     The path to the object to alias.
 * @param alias    The alias to create. This must not already exist in the registry.
 * @return         Error code. Possible values are:
 *                 - OK: the alias was successfully created
 *                 - EEXISTS: this very exact alias already exists in the registry
 *                 - EINVAL: the path or alias is invalid (`alias` exists and points to a different object, `alias` is
 *                           the same as `path`, or `alias` is the path to an object that is not aliased)
 *                 - ENOMEM: memory allocation failed
 *                 - EPATH_MALFORMED: the path or alias is invalid (e.g. not a valid Dicey path)
 *                 - EPATH_NOT_FOUND: the object at the given path does not exist
 */
DICEY_EXPORT enum dicey_error dicey_registry_alias_object(
    struct dicey_registry *registry,
    const char *path,
    const char *alias
);

/**
 * @brief Checks if an element exists at a given path in the registry.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param trait    The name of the trait.
 * @param elem     The name of the element.
 * @return         True if the element exists, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_contains_element(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait_name,
    const char *elem
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
DICEY_EXPORT const struct dicey_element *dicey_registry_get_element(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait_name,
    const char *elem
);

/**
 * @brief Gets the entry of an element from a trait.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param trait    The name of the trait.
 * @param elem     The name of the element.
 * @param entry    A pointer to a `dicey_element_entry` struct to fill with the element's details. Must not be NULL.
 * @return         True if the element exists, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_get_element_entry(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait_name,
    const char *elem,
    struct dicey_object_element_entry *entry
);

/**
 * @brief Gets an element from a trait using a selector.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param sel      A valid selector referencing the element.
 * @return         A pointer to the element, or NULL if the element does not exist.
 */
DICEY_EXPORT const struct dicey_element *dicey_registry_get_element_from_sel(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel
);

/**
 * @brief Gets the entry of an element from a trait using a selector.
 * @param registry The registry to get the element from.
 * @param path     The path to the object.
 * @param sel      A valid selector referencing the element.
 * @param entry    A pointer to a `dicey_element_entry` struct to fill with the element's details. Must not be NULL.
 * @return         True if the element exists, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_get_element_entry_from_sel(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel,
    struct dicey_object_element_entry *entry
);

/**
 * @brief Resolves the main path of an object at a given path.
 * @note  If the path is not an alias, this will return the same path but from the registry allocation scope.
 * @param registry The registry to resolve the path in.
 * @param path The path to resolve.
 * @return     The main path of the object at the given path, or NULL if the path is invalid or the object does not
 *             exist.
 */
DICEY_EXPORT const char *dicey_registry_get_main_path(const struct dicey_registry *registry, const char *path);

/**
 * @brief Gets the object at a given path.
 * @param registry The registry to get the object from.
 * @param path     The path to the object.
 * @return         A pointer to the object, or NULL if the object does not exist.
 */
DICEY_EXPORT const struct dicey_object *dicey_registry_get_object(
    const struct dicey_registry *registry,
    const char *path
);

/**
 * @brief Gets an entry representing the object at a given path.
 * @param registry The registry to get the object from.
 * @param path     The path to the object.
 * @param entry    A pointer to a `dicey_object_entry` struct to fill with the object's details. Must not be NULL.
 * @return         True if the object exists, false otherwise.
 */
DICEY_EXPORT bool dicey_registry_get_object_entry(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_object_entry *entry
);

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

/**
 * @brief Removes an object alias from the registry. This won't touch the original object.
 * @note  Deleting an object main path will also delete its aliases.
 * @note  The object original path can't be unaliased, so this function will fail if the path is the same as the
 *        original path of the object.
 * @param registry The registry to remove the alias from.
 * @param alias    The alias to remove.
 * @return         Error code. Possible values are:
 *                 - OK: the alias was successfully removed
 *                 - EPATH_NOT_FOUND: the alias does not exist
 *                 - EPATH_NOT_ALIAS: the path is not an alias
 *                 - EPATH_MALFORMED: the path is malformed (e.g. not a valid Dicey path)
 */
DICEY_EXPORT enum dicey_error dicey_registry_unalias_object(struct dicey_registry *registry, const char *alias);

/**
 * @brief Represents an event that can occur during a registry walk.
 */
enum dicey_registry_walk_event {
    DICEY_REGISTRY_WALK_EVENT_OBJECT_END,   /**< Reached the end of an object. */
    DICEY_REGISTRY_WALK_EVENT_OBJECT_START, /**< Started walking over an object. */
    DICEY_REGISTRY_WALK_EVENT_TRAIT_END,    /**< Reached the end of a trait. */
    DICEY_REGISTRY_WALK_EVENT_TRAIT_START,  /**< Started walking over a trait. */
    DICEY_REGISTRY_WALK_EVENT_ELEMENT,      /**< Encountered an element. */
};

/**
 * @brief Callback called when walking over the registry, calling a callback for each object, trait, and element
 * encountered.
 * @param registry  The registry being walked over.
 * @param event     The event that occurred.
 * @param path      The path of the object being walked over.
 * @param sel       The selector of the element being walked over. (may be invalid or lack an element)
 * @param trait     The trait being walked over. If NULL, the trait field of `sel` is also NULL.
 * @param element   The element being walked over. If NULL, the element field of `sel` is also NULL.
 * @param user_data User data passed to the walk function.
 * @return          True to continue walking, false to stop.
 */
typedef bool dicey_registry_walk_fn(
    const struct dicey_registry *registry,
    enum dicey_registry_walk_event event,
    const char *path,
    const struct dicey_selector sel,
    const struct dicey_trait *trait,
    const struct dicey_element *element,
    void *user_data
);

/**
 * @brief Walks over the elements of a given object.
 */
DICEY_EXPORT enum dicey_error dicey_registry_walk_object_elements(
    const struct dicey_registry *registry,
    const char *path,
    dicey_registry_walk_fn *callback,
    void *user_data
);

#if defined(__cplusplus)
}
#endif

#endif // PIIPYFREDI_REGISTRY_H
