// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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

DICEY_EXPORT struct dicey_hashset *dicey_hashset_new(void);

DICEY_EXPORT void dicey_hashset_delete(struct dicey_hashset *table);

DICEY_EXPORT struct dicey_hashset_iter dicey_hashset_iter_start(struct dicey_hashset *table);
DICEY_EXPORT bool dicey_hashset_iter_next(struct dicey_hashset_iter *iter, const char **key);

DICEY_EXPORT bool dicey_hashset_contains(struct dicey_hashset *table, const char *key);
DICEY_EXPORT bool dicey_hashset_remove(struct dicey_hashset *table, const char *key);

DICEY_EXPORT enum dicey_hash_set_result dicey_hashset_add(struct dicey_hashset **set, const char *key);

DICEY_EXPORT uint32_t dicey_hashset_size(struct dicey_hashset *table);

#if defined(__cplusplus)
}
#endif

#endif // KVOFDWUXXQ_HASHSET_H
