// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(XCRGTMFDFE_HASHTABLE_H)
#define XCRGTMFDFE_HASHTABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "dicey_export.h"

struct dicey_hashtable;

struct dicey_hashtable_iter {
    struct dicey_hashtable *_table;
    const void *_current;
};

DICEY_EXPORT struct dicey_hashtable *dicey_hashtable_new(void);

typedef void dicey_hashtable_free_fn(void *value);
DICEY_EXPORT void dicey_hashtable_delete(struct dicey_hashtable *table, dicey_hashtable_free_fn *free_fn);

DICEY_EXPORT struct dicey_hashtable_iter dicey_hashtable_iter_start(struct dicey_hashtable *table);
DICEY_EXPORT bool dicey_hashtable_iter_next(struct dicey_hashtable_iter *iter, const char **key, void **value);

DICEY_EXPORT bool dicey_hashtable_contains(struct dicey_hashtable *table, const char *key);
DICEY_EXPORT void *dicey_hashtable_get(struct dicey_hashtable *table, const char *key);
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

DICEY_EXPORT uint32_t dicey_hashtable_size(struct dicey_hashtable *table);

#endif // XCRGTMFDFE_HASHTABLE_H
