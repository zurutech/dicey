// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(XCRGTMFDFE_HASHTABLE_H)
#define XCRGTMFDFE_HASHTABLE_H

#include <stdbool.h>
#include <stdint.h>

struct dicey_hashtable;
struct dicey_hashtable_iter {
    struct dicey_hashtable *_table;
    const void *_current;
};

struct dicey_hashtable *dicey_hashtable_new(void);
void dicey_hashtable_delete(struct dicey_hashtable *table);

struct dicey_hashtable_iter dicey_hashtable_iter_start(struct dicey_hashtable *table);
bool dicey_hashtable_iter_next(struct dicey_hashtable_iter *iter, const char **key, void **value);

bool dicey_hashtable_contains(struct dicey_hashtable *table, const char *key);
void *dicey_hashtable_get(struct dicey_hashtable *table, const char *key);
void *dicey_hashtable_remove(struct dicey_hashtable *table, const char *key);

enum dicey_hashtable_set_result {
    DICEY_HASHTABLE_SET_FAILED = 0,
    DICEY_HASHTABLE_SET_ADDED,
    DICEY_HASHTABLE_SET_UPDATED,
};

enum dicey_hashtable_set_result dicey_hashtable_set(
    struct dicey_hashtable **table,
    const char *key,
    void *value,
    void **old_value
);
uint32_t dicey_hashtable_size(struct dicey_hashtable *table);

#endif // XCRGTMFDFE_HASHTABLE_H
