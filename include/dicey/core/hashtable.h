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

DICEY_EXPORT struct dicey_hashtable *dicey_hashtable_new(void);

typedef void dicey_hashtable_free_fn(void *value);
DICEY_EXPORT void dicey_hashtable_delete(struct dicey_hashtable *table, dicey_hashtable_free_fn *free_fn);

DICEY_EXPORT struct dicey_hashtable_iter dicey_hashtable_iter_start(const struct dicey_hashtable *table);
DICEY_EXPORT bool dicey_hashtable_iter_next(struct dicey_hashtable_iter *iter, const char **key, void **value);

DICEY_EXPORT bool dicey_hashtable_contains(const struct dicey_hashtable *table, const char *key);
DICEY_EXPORT void *dicey_hashtable_get(const struct dicey_hashtable *table, const char *key);

DICEY_EXPORT void *dicey_hashtable_get_entry(
    const struct dicey_hashtable *table,
    const char *key,
    struct dicey_hashtable_entry *entry
);

DICEY_EXPORT void *dicey_hashtable_remove(struct dicey_hashtable *table, const char *key);

enum dicey_hash_set_result {
    DICEY_HASH_SET_FAILED = 0,
    DICEY_HASH_SET_ADDED,
    DICEY_HASH_SET_UPDATED,
};

DICEY_EXPORT enum dicey_hash_set_result dicey_hashtable_set(
    struct dicey_hashtable **table,
    const char *key,
    void *value,
    void **old_value
);

DICEY_EXPORT uint32_t dicey_hashtable_size(const struct dicey_hashtable *table);

#if defined(__cplusplus)
}
#endif

#endif // XCRGTMFDFE_HASHTABLE_H
