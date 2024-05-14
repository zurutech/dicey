// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/hashtable.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200) // borked C11 flex array
#pragma warning(disable : 4996) // strdup
#endif

#define REHASH_THRESHOLD 75U

// if this doesn't hold then the hash table will misbehave due to overflows. AFAIK this is never false in any relevant
// case
static_assert(sizeof(int32_t) <= sizeof(ptrdiff_t), "int32_t must either be or fit into ptrdiff_t");

// Prime list courtesy of Aaron Krowne (http://br.endernet.org/~akrowne/)
// Taken from http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
static const int32_t primes[] = {
    53,       97,       193,      389,       769,       1543,      3079,      6151,       12289,
    24593,    49157,    98317,    196613,    393241,    786433,    1572869,   3145739,    6291469,
    12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741,
    -1 // -1 to indicate end of list
};

struct maybe_owned_str {
    const char *str;
    bool owned;
};

void maybe_free(struct maybe_owned_str str) {
    if (str.owned) {
        free((void *) str.str); // cast away constness, the string was malloc'd
    }
}

struct maybe_owned_str maybe_move(struct maybe_owned_str *const mstr) {
    assert(mstr);

    struct maybe_owned_str ret = { .str = mstr->str, .owned = mstr->owned };

    *mstr = (struct maybe_owned_str) { 0 };

    return ret;
}

const char *maybe_take(struct maybe_owned_str *const str) {
    assert(str);

    if (!str->owned) {
        return strdup(str->str);
    }

    const char *const owned = str->str;

    *str = (struct maybe_owned_str) { 0 };

    return owned;
}

struct table_entry {
    const char *key;
    void *value;

    // offset to the next entry in the bucket. 0 means no next entry, because 0 is always a bucket start
    size_t next;
};

struct dicey_hashtable {
    uint32_t len, cap;
    const int32_t *buckets_no;

    size_t free_cur;
    struct table_entry entries[];
};

static struct table_entry *bucket_find_entry(
    struct dicey_hashtable *const ht,
    const size_t bucket_offs,
    const char *const key,
    struct table_entry **const first_free,
    size_t *bucket_end
) {
    assert(ht && ht->buckets_no && bucket_offs < (uint32_t) *ht->buckets_no);

    struct table_entry *const cells = ht->entries;

    size_t current_offs = bucket_offs;
    for (;;) {
        assert(current_offs < ht->cap);

        struct table_entry *entry = cells + current_offs;

        if (entry->key) {
            if (!strcmp(entry->key, key)) {
                return entry;
            }
        } else if (first_free && !*first_free) {
            // given that we're already recurring through the bucket, we can also find the first free cell and return it
            // this is used by set to avoid recurring through the bucket twice
            *first_free = entry;
        }

        if (!entry->next) {
            if (bucket_end) {
                *bucket_end = current_offs;
            }

            break;
        }

        current_offs = entry->next;
    }

    return NULL;
}

static uint32_t djb2(const char *str) {
    uint32_t hash = 5381;

    for (;;) {
        const int32_t c = (uint8_t) *str++;

        if (!c) {
            break;
        }

        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/**
 * @brief Find the entry containing a given key in the hash table. For simplicity, the first free hole in the bucket is
 *        optionally determined (if any), and the bucket end.
 */
static struct table_entry *hash_get_entry_for_set(
    struct dicey_hashtable *const ht,
    const char *const key,
    struct table_entry **const first_free,
    size_t *const bucket_end
) {
    assert(ht && ht->buckets_no && *ht->buckets_no > 0);

    if (!key) {
        return NULL;
    }

    const uint32_t buckets_no = (uint32_t) *ht->buckets_no;
    assert(buckets_no <= ht->cap);

    const uint32_t hash = djb2(key);
    const uint32_t bucket = hash % buckets_no;

    return bucket_find_entry(ht, (ptrdiff_t) bucket, key, first_free, bucket_end);
}

static struct table_entry *hash_get_entry(struct dicey_hashtable *const ht, const char *const key) {
    return hash_get_entry_for_set(ht, key, NULL, NULL);
}

static struct dicey_hashtable *hash_new(const int32_t *const primes_list, const size_t extra_cap) {
    assert(primes_list);

    if (*primes_list <= 0) {
        return NULL;
    }

    const size_t cap = (size_t) *primes_list + extra_cap;
    struct dicey_hashtable *table = calloc(1, sizeof(struct dicey_hashtable) + cap * sizeof(struct table_entry));
    if (!table) {
        return NULL;
    }

    *table = (struct dicey_hashtable) {
        .cap = (uint32_t) *primes_list,
        .buckets_no = primes_list,
        .free_cur = *primes_list,
    };

    return table;
}

static enum dicey_hash_set_result hash_set(
    struct dicey_hashtable **table_ptr,
    const struct maybe_owned_str key,
    void *const value,
    void **const old_value
);

static bool hash_rehash_and_set_new(
    struct dicey_hashtable **const table_ptr,
    const struct maybe_owned_str key,
    void *const value
) {
    assert(table_ptr && *table_ptr);

    struct dicey_hashtable *const table = *table_ptr;

    assert(table->buckets_no && *table->buckets_no > 0);

    // we accept a few reallocations when we rehash for the sake of simplicity
    // TODO: add a magical "extra cap" value
    struct dicey_hashtable *new_table = hash_new(table->buckets_no + 1, 0);
    if (!new_table) {
        return false;
    }

    // steal the old table's entries
    const struct table_entry *const end = table->entries + table->cap;
    for (struct table_entry *it = table->entries; it < end; ++it) {
        if (!it->key) {
            continue;
        }

        struct maybe_owned_str stolen_key = { .str = it->key, .owned = true };

        const enum dicey_hash_set_result res = hash_set(&new_table, stolen_key, it->value, NULL);

        // this is impossible, keys are unique
        assert(res != DICEY_HASH_SET_UPDATED);

        if (!res) {
            // free the new table - without deleting anything. The keys are all still in place in the old one.
            // Everything is fine. The old table is still valid and the new one is not.
            free(new_table);
            maybe_free(key);

            return false;
        }
    }

    // set the last value
    void *old_value;
    const enum dicey_hash_set_result res = hash_set(&new_table, key, value, &old_value);
    if (!res) {
        free(new_table);
        maybe_free(key);

        return false;
    }

    // free the old table without deleting the keys
    free(table);

    *table_ptr = new_table;

    return true;
}

static bool hash_grow(struct dicey_hashtable **const table_ptr) {
    assert(table_ptr && *table_ptr);

    struct dicey_hashtable *table = *table_ptr;
    assert(table->buckets_no && *table->buckets_no > 0);

    const uint32_t old_cap = table->cap;
    const uint32_t buckets_no = (uint32_t) *table->buckets_no;

    assert(old_cap >= buckets_no);

    const uint32_t extra_cap = old_cap - buckets_no;
    const uint32_t new_cap = ((uint32_t) *primes + extra_cap) * 3 / 2;

    table = realloc(table, sizeof(struct dicey_hashtable) + new_cap * sizeof(struct table_entry));
    if (!table) {
        return false;
    }

    // zero the new values. _technically_ UB, but works everywhere
    memset(table->entries + old_cap, 0, (new_cap - old_cap) * sizeof(struct table_entry));
    table->cap = new_cap;

    *table_ptr = table;

    return true;
}

static bool hash_bucket_append(
    struct dicey_hashtable **const table_ptr,
    const size_t last_item,
    struct maybe_owned_str key,
    void *const value
) {
    assert(table_ptr && *table_ptr && key.str);

    struct dicey_hashtable *table = *table_ptr;

    assert(last_item < table->cap);

    if (table->free_cur >= table->cap) {
        if (!hash_grow(table_ptr)) {
            return false;
        }

        table = *table_ptr;
    }

    assert(table->free_cur < table->cap);

    const char *const new_key = maybe_take(&key);
    if (!new_key) {
        return false;
    }

    const size_t new_entry_offs = table->free_cur++;
    struct table_entry *new_entry = table->entries + new_entry_offs;
    *new_entry = (struct table_entry) {
        .key = (char *) new_key,
        .value = value,
    };

    struct table_entry *const last = table->entries + last_item;
    last->next = new_entry_offs;

    return true;
}

static uint32_t load_factor(const uint32_t len, const uint32_t buckets_no) {
    assert(len * 100 >= len); // check for overflow

    return len * 100 / buckets_no;
}

enum dicey_hash_set_result hash_set(
    struct dicey_hashtable **table_ptr,
    struct maybe_owned_str key,
    void *const value,
    void **const old_value
) {
    assert(table_ptr && *table_ptr && key.str && old_value);

    struct dicey_hashtable *table = *table_ptr;

    struct table_entry *first_free = NULL;

    size_t bucket_end = 0;
    struct table_entry *const existing = hash_get_entry_for_set(table, key.str, &first_free, &bucket_end);

    enum dicey_hash_set_result res = DICEY_HASH_SET_FAILED;

    if (existing) {
        *old_value = existing->value;
        existing->value = value;

        res = DICEY_HASH_SET_UPDATED;

        goto finish;
    }

    // the value doesn't exist - set the old value to NULL
    *old_value = NULL;

    const uint32_t new_len = table->len + 1;
    assert(table->buckets_no && *table->buckets_no > 0);

    if (load_factor(new_len, (uint32_t) *table->buckets_no) >= REHASH_THRESHOLD) {
        if (!hash_rehash_and_set_new(&table, maybe_move(&key), value)) {
            res = DICEY_HASH_SET_FAILED;
            goto finish;
        }

        assert(table);

        *table_ptr = table;

        res = DICEY_HASH_SET_ADDED;

        goto finish;
    }

    if (first_free) {
        first_free->key = maybe_take(&key);
        if (!first_free->key) {
            return DICEY_HASH_SET_FAILED;
        }

        first_free->value = value;

        table->len = new_len;

        res = DICEY_HASH_SET_ADDED;

        goto finish;
    }

    // if we reached this point then the bucket has no holes and we need to add a new entry at the end
    assert(bucket_end);
    res = hash_bucket_append(table_ptr, bucket_end, maybe_move(&key), value) ? DICEY_HASH_SET_ADDED
                                                                             : DICEY_HASH_SET_FAILED;

finish:
    maybe_free(key);

    return res;
}

struct dicey_hashtable *dicey_hashtable_new(void) {
    assert(*primes > 0);

    return hash_new(primes, 0U);
}

void dicey_hashtable_delete(struct dicey_hashtable *const table, dicey_hashtable_free_fn *const free_fn) {
    if (!table) {
        return;
    }

    const struct table_entry *const end = table->entries + table->cap;

    for (struct table_entry *entry = table->entries; entry < end; ++entry) {
        free((void *) entry->key); // all these strings are malloc'd

        if (free_fn) {
            // values are not owned by the table
            free_fn(entry->value);
        }
    }

    free(table);
}

struct dicey_hashtable_iter dicey_hashtable_iter_start(const struct dicey_hashtable *const table) {
    return (struct dicey_hashtable_iter) {
        ._table = table,
        ._current = table ? table->entries : NULL,
    };
}

bool dicey_hashtable_iter_next(struct dicey_hashtable_iter *const iter, const char **const key, void **const value) {
    assert(iter);

    const struct dicey_hashtable *const ht = iter->_table;
    const struct table_entry *cell = iter->_current;

    if (!ht || !cell) {
        goto iter_end;
    }

    const struct table_entry *const end = ht->entries + ht->cap;
    if (cell >= end) {
        goto iter_end;
    }

    // skip empty cells
    while (!cell->key) {
        if (++cell >= end) {
            goto iter_end;
        }
    }

    if (key) {
        *key = cell->key;
    }

    if (value) {
        *value = cell->value;
    }

    iter->_current = cell + 1;

    return true;

iter_end:
    iter->_current = NULL;

    return false;
}

bool dicey_hashtable_contains(const struct dicey_hashtable *const table, const char *const key) {
    return dicey_hashtable_get(table, key);
}

void *dicey_hashtable_get(const struct dicey_hashtable *const table, const char *const key) {
    return dicey_hashtable_get_entry(table, key, &(struct dicey_hashtable_entry) { 0 });
}

void *dicey_hashtable_get_entry(
    const struct dicey_hashtable *const table,
    const char *const key,
    struct dicey_hashtable_entry *const entry
) {
    assert(entry);

    // we know as a fact we aren't going to edit the table and that we are the only one able to allocate new tables
    // we can thus cast away constness without ever incurring in UB
    const struct table_entry *const table_entry = hash_get_entry((struct dicey_hashtable *) table, key);
    if (!table_entry) {
        return NULL;
    }

    *entry = (struct dicey_hashtable_entry) {
        .key = table_entry->key,
        .value = table_entry->value,
    };

    return table_entry->value;
}

void *dicey_hashtable_remove(struct dicey_hashtable *table, const char *const key) {
    // remove is simple: just set the key to NULL and return the value
    // the entry will be reused when a new key is added, or skipped if the table is rehashed
    // during search the entry will be skipped if the key is NULL

    struct table_entry *const entry = hash_get_entry(table, key);
    if (!entry) {
        return NULL;
    }

    void *const value = entry->value;

    free((void *) entry->key); // the key is malloc'd

    entry->key = NULL;
    entry->value = NULL;

    // leave the next offset as is, this cell is now a free cell and will be reused when a new key is added

    return value;
}

enum dicey_hash_set_result dicey_hashtable_set(
    struct dicey_hashtable **table_ptr,
    const char *const key,
    void *const value,
    void **const old_value
) {
    return hash_set(table_ptr, (struct maybe_owned_str) { .str = (char *) key, .owned = false }, value, old_value);
}

uint32_t dicey_hashtable_size(struct dicey_hashtable *const table) {
    return table ? table->len : 0;
}
