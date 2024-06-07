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

#if !defined(GFJABYMEEM_TRAITS_H)
#define GFJABYMEEM_TRAITS_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/errors.h"
#include "../core/hashtable.h"
#include "../core/type.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Enumeration of element types.
 */
enum dicey_element_type {
    DICEY_ELEMENT_TYPE_INVALID,

    DICEY_ELEMENT_TYPE_OPERATION, /**< A callable operation */
    DICEY_ELEMENT_TYPE_PROPERTY,  /**< A property, which may be read-only or read-write */
    DICEY_ELEMENT_TYPE_SIGNAL,    /**< A signal, asynchronously sent by the server */
};

/**
 * @brief Structure that describes an element (operation, property, or signal) inside a trait.
 */
struct dicey_element {
    enum dicey_element_type type; /**< The type of the element (operation, property, or signal) */

    const char *signature; /**< The signature of the element */

    bool readonly; /**< Whether a property is read-only or not. Has no effect on operations or signals. */

    uintptr_t _tag; /**< Internal tag used to store element metadata. Do not use, do not set */
};

/**
 * @brief Structure that describes the entry associated with an element in a trait hashtable.
 */
struct dicey_element_entry {
    struct dicey_selector sel; /**< The element selector. This is guaranteed to be valid until the trait exists */
    const struct dicey_element *element; /**< The element itself */
};

/**
 * @brief Iterator over the elements of a trait structure.
 */
struct dicey_trait_iter {
    struct dicey_hashtable_iter _inner;
};

/**
 * @brief Structure that describes a trait, identified by a name and a set of elements.
 */
struct dicey_trait {
    const char *name; /**< The name of the trait. May only contain ASCII letters, numbers and dots. */

    struct dicey_hashtable *elems; /**< A hashtable of elements, indexed by their names. */
};

/**
 * @brief Create an iterator over the elements of a trait.
 * @param trait The trait to iterate over.
 * @return An iterator over the elements of the trait.
 */
DICEY_EXPORT struct dicey_trait_iter dicey_trait_iter_start(const struct dicey_trait *trait);

/**
 * @brief Get the next element in the trait iterator.
 * @param iter The iterator.
 * @param elem_name Pointer to store the name of the element. Owned by the trait.
 * @param elem Pointer to store the element. Owned by the trait.
 * @return true if not exhausted, false otherwise.
 */
DICEY_EXPORT bool dicey_trait_iter_next(
    struct dicey_trait_iter *iter,
    const char **elem_name,
    struct dicey_element *elem
);

/**
 * @brief Deletes a trait structure.
 * @param trait The trait to delete. No-op if NULL.
 */
DICEY_EXPORT void dicey_trait_delete(struct dicey_trait *trait);

/**
 * @brief Creates a new trait structure.
 * @param name The name of the trait. Will be copied.
 * @return A pointer to the new trait, or NULL on allocation error.
 */
DICEY_EXPORT struct dicey_trait *dicey_trait_new(const char *name);

/**
 * @brief Adds an element to a trait. The contents of the element are deep copied.
 */
DICEY_EXPORT enum dicey_error dicey_trait_add_element(
    struct dicey_trait *trait,
    const char *name,
    struct dicey_element elem
);

/**
 * @brief Check if a trait contains an element with a specific name.
 * @param trait The trait to check.
 * @param name The name of the element.
 * @return true if the element is present, false otherwise.
 */
DICEY_EXPORT bool dicey_trait_contains_element(const struct dicey_trait *trait, const char *name);

/**
 * @brief Get an element from a trait by name.
 * @param trait The trait to get the element from.
 * @param name The name of the element.
 * @return A pointer to the element, or NULL if not found.
 */
DICEY_EXPORT const struct dicey_element *dicey_trait_get_element(const struct dicey_trait *trait, const char *name);

/**
 * @brief Get an element entry from a trait by name.
 * @param trait The trait to get the element from.
 * @param name The name of the element.
 * @param entry Pointer to store the element entry.
 * @return true if the element is present, false otherwise.
 */
DICEY_EXPORT bool dicey_trait_get_element_entry(
    const struct dicey_trait *trait,
    const char *name,
    struct dicey_element_entry *entry
);

#if defined(__cplusplus)
}
#endif

#endif // GFJABYMEEM_TRAITS_H
