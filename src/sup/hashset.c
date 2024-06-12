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

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>

// note: the C standard says that "A pointer to an object type may be converted to a pointer to a different object type.
// If the resulting pointer is not correctly aligned68) for the referenced type, the behavior is undefined", so this
// whole file may potentially be full of UB. Still, we only support a handful of CPU architectures and OSes and not even
// Minesweeper would work if you couldn't just cast stuff around willy nilly, so I'll live with the consequences of my
// actions for the sake of a nice API, thank you very much.

static_assert(
    alignof(struct dicey_hashset *) == alignof(struct dicey_hashtable *),
    "Well, you sure bought a weird CPU sir"
);

// a non-null marker, helpful to distinguish non-set (NULL) from set (phony)
static int phony = 0;

enum dicey_hash_set_result dicey_hashset_add(struct dicey_hashset **const set, const char *const key) {
    assert(set);

    struct dicey_hashtable *map = (struct dicey_hashtable *) *set;

    const enum dicey_hash_set_result res = dicey_hashtable_set(&map, key, &phony, &(void *) { NULL });

    *set = (struct dicey_hashset *) map;

    return res;
}

struct dicey_hashset *dicey_hashset_new(void) {
    return (struct dicey_hashset *) dicey_hashtable_new();
}

void dicey_hashset_delete(struct dicey_hashset *const table) {
    dicey_hashtable_delete((struct dicey_hashtable *) table, NULL);
}

struct dicey_hashset_iter dicey_hashset_iter_start(const struct dicey_hashset *const table) {
    return (struct dicey_hashset_iter) {
        ._inner = dicey_hashtable_iter_start((const struct dicey_hashtable *) table),
    };
}

bool dicey_hashset_iter_next(struct dicey_hashset_iter *const iter, const char **const key) {
    assert(iter && key);

    return dicey_hashtable_iter_next(&iter->_inner, key, NULL);
}

bool dicey_hashset_contains(const struct dicey_hashset *const table, const char *const key) {
    return dicey_hashtable_contains((const struct dicey_hashtable *) table, key);
}

bool dicey_hashset_remove(struct dicey_hashset *const table, const char *const key) {
    return dicey_hashtable_remove((struct dicey_hashtable *) table, key);
}

uint32_t dicey_hashset_size(const struct dicey_hashset *const table) {
    return dicey_hashtable_size((const struct dicey_hashtable *) table);
}
