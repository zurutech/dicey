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

#if !defined(XCRGTMFDFE_HASHTABLE_H)
#define XCRGTMFDFE_HASHTABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dicey_hashtable;

struct dicey_hashtable_iter {
    const struct dicey_hashtable *_table;
    const void *_current;
};

struct dicey_hashtable_entry {
    const char *key;
    void *value;
};

/**
 * @brief Creates a new hashtable.
 * @return A pointer to the new hashtable, or NULL if memory allocation fails.
 */
DICEY_EXPORT struct dicey_hashtable *dicey_hashtable_new(void);

/**
 * @brief A function that frees the memory of a value in a hashtable.
 * @param value The value to free. Only NULL when NULL is a valid value in the hashtable.
 */
typedef void dicey_hashtable_free_fn(void *value);

/**
 * @brief Deletes a hashtable and frees its memory. `free_fn` is called for each value in the hashtable.
 * @note
 * @param table The hashtable to delete.
 * @param free_fn A function to free the memory of each value in the hashtable.
 */
DICEY_EXPORT void dicey_hashtable_delete(struct dicey_hashtable *table, dicey_hashtable_free_fn *free_fn);

/**
 * @brief Starts an iterator for a hashtable.
 * @param table The hashtable to iterate over.
 * @return An iterator for the hashtable.
 */
DICEY_EXPORT struct dicey_hashtable_iter dicey_hashtable_iter_start(const struct dicey_hashtable *table);

/**
 * @brief Advances an iterator to the next element in a hashtable.
 * @param iter The iterator to advance.
 * @param key A pointer to a variable that will receive the key of the next element.
 * @param value A pointer to a variable that will receive the value of the next element.
 * @return `true` if the iterator was advanced, `false` if there are no more elements.
 */
DICEY_EXPORT bool dicey_hashtable_iter_next(struct dicey_hashtable_iter *iter, const char **key, void **value);

/**
 * @brief Checks if a hashtable contains a key.
 * @param table The hashtable to check.
 * @param key The key to check for.
 * @return `true` if the key is in the hashtable, `false` otherwise.
 */
DICEY_EXPORT bool dicey_hashtable_contains(const struct dicey_hashtable *table, const char *key);

/**
 * @brief Gets the value associated with a key in a hashtable.
 * @param table The hashtable to get the value from.
 * @param key The key to get the value for.
 * @return The value associated with the key, or NULL if the key is not in the hashtable.
 */
DICEY_EXPORT void *dicey_hashtable_get(const struct dicey_hashtable *table, const char *key);

/**
 * @brief Gets the entry associated with a key in a hashtable.
 * @param table The hashtable to get the entry from.
 * @param key The key to get the entry for.
 * @param entry A pointer to a variable that will receive the entry.
 * @return The value associated with the key, or NULL if the key is not in the hashtable.
 */
DICEY_EXPORT void *dicey_hashtable_get_entry(
    const struct dicey_hashtable *table,
    const char *key,
    struct dicey_hashtable_entry *entry
);

/**
 * @brief Removes a key-value pair from a hashtable.
 * @param table The hashtable to remove the key-value pair from.
 * @param key The key to remove.
 * @return The value associated with the key, or NULL if the key was not in the hashtable.
 */
DICEY_EXPORT void *dicey_hashtable_remove(struct dicey_hashtable *table, const char *key);

/**
 * @brief The result of a set operation on a hashtable.
 */
enum dicey_hash_set_result {
    DICEY_HASH_SET_FAILED = 0, /**< The operation failed. */
    DICEY_HASH_SET_ADDED,      /**< A new key-value pair was added. */
    DICEY_HASH_SET_UPDATED,    /**< An existing key-value pair was updated. */
};

/**
 * @brief Sets a key-value pair in a hashtable.
 * @param table A pointer to the hashtable.
 * @param key The key to set.
 * @param value The value to set.
 * @param old_value A pointer to a variable that will receive the old value if the key already exists.
 * @return A `dicey_hash_set_result` indicating the result of the operation.
 */
DICEY_EXPORT enum dicey_hash_set_result dicey_hashtable_set(
    struct dicey_hashtable **table,
    const char *key,
    void *value,
    void **old_value
);

/**
 * @brief Gets the number of elements in a hashtable.
 * @param table The hashtable.
 * @return The number of elements in the hashtable.
 */
DICEY_EXPORT uint32_t dicey_hashtable_size(const struct dicey_hashtable *table);

#if defined(__cplusplus)
}
#endif

#endif // XCRGTMFDFE_HASHTABLE_H
