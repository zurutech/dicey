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

#if !defined(KVOFDWUXXQ_HASHSET_H)
#define KVOFDWUXXQ_HASHSET_H

#include "hashtable.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dicey_hashset;

struct dicey_hashset_iter {
    struct dicey_hashtable_iter _inner;
};

/**
 * @brief Creates a new hashset.
 * @return A pointer to the new hashset, or NULL if memory allocation fails.
 */
DICEY_EXPORT struct dicey_hashset *dicey_hashset_new(void);

/**
 * @brief Deletes a hashset and frees its memory.
 * @param table The hashset to delete.
 */
DICEY_EXPORT void dicey_hashset_delete(struct dicey_hashset *table);

/**
 * @brief Starts an iterator for a hashset.
 * @param table The hashset to iterate over.
 * @return An iterator for the hashset.
 */
DICEY_EXPORT struct dicey_hashset_iter dicey_hashset_iter_start(const struct dicey_hashset *table);

/**
 * @brief Advances an iterator to the next element in a hashset.
 * @param iter The iterator to advance.
 * @param key A pointer to a variable that will receive the key of the next element.
 * @return `true` if the iterator was advanced, `false` if there are no more elements.
 */
DICEY_EXPORT bool dicey_hashset_iter_next(struct dicey_hashset_iter *iter, const char **key);

/**
 * @brief Checks if a hashset contains a key.
 * @param table The hashset to check.
 * @param key The key to check for.
 * @return `true` if the key is in the hashset, `false` otherwise.
 */
DICEY_EXPORT bool dicey_hashset_contains(const struct dicey_hashset *table, const char *key);

/**
 * @brief Removes a key from a hashset.
 * @param table The hashset to remove the key from.
 * @param key The key to remove.
 * @return `true` if the key was removed, `false` if the key was not in the hashset.
 */
DICEY_EXPORT bool dicey_hashset_remove(struct dicey_hashset *table, const char *key);

/**
 * @brief Adds a key to a hashset.
 * @param set A pointer to the hashset.
 * @param key The key to add.
 * @return A `dicey_hash_set_result` indicating the result of the operation.
 */
DICEY_EXPORT enum dicey_hash_set_result dicey_hashset_add(struct dicey_hashset **set, const char *key);

/**
 * @brief Gets the number of elements in a hashset.
 * @param table The hashset.
 * @return The number of elements in the hashset.
 */
DICEY_EXPORT uint32_t dicey_hashset_size(const struct dicey_hashset *table);

#if defined(__cplusplus)
}
#endif

#endif // KVOFDWUXXQ_HASHSET_H
