// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _CRT_NONSTDC_NO_DEPRECATE 1
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/hashtable.h>
#include <dicey/ipc/traits.h>

static struct dicey_element *elem_dup(const struct dicey_element *const elem) {
    if (!elem) {
        return NULL;
    }

    struct dicey_element *const elem_copy = calloc(1U, sizeof *elem_copy);
    if (!elem_copy) {
        return NULL;
    }

    *elem_copy = *elem;

    elem_copy->signature = strdup(elem->signature);
    if (!elem_copy->signature) {
        free(elem_copy);

        return NULL;
    }

    return elem_copy;
}

static void free_elem(void *const elem) {
    struct dicey_element *const elem_cast = (struct dicey_element *) elem;

    // originally strdup'd
    free((char *) elem_cast->signature);
}

enum dicey_error dicey_trait_add_element(
    struct dicey_trait *const trait,
    const char *const name,
    const struct dicey_element elem
) {
    assert(trait && trait->elems && elem.signature && *elem.signature && elem.type != DICEY_ELEMENT_TYPE_INVALID);

    // todo: optimise this by implementing an "add-or-fail" function in hashtable
    if (dicey_hashtable_contains(trait->elems, name)) {
        return DICEY_EINVAL;
    }

    struct dicey_element *const elem_val = elem_dup(&elem);
    if (!elem_val) {
        return DICEY_ENOMEM;
    }

    void *old_val = NULL;
    switch (dicey_hashtable_set(&trait->elems, name, elem_val, &old_val)) {
    case DICEY_HASH_SET_FAILED:
        free(elem_val);

        return DICEY_ENOMEM;

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached

    case DICEY_HASH_SET_ADDED:
        assert(!old_val);

        return DICEY_OK;
    }
}

void dicey_trait_deinit(struct dicey_trait *const trait) {
    if (trait) {
        assert(trait->elems);

        dicey_hashtable_delete(trait->elems, free_elem);

        free((char *) trait->name); // cast away const, this originated from strdup

        *trait = (struct dicey_trait) { 0 };
    }
}

bool dicey_trait_contains_element(const struct dicey_trait *const trait, const char *const name) {
    assert(trait && name && *name);

    return dicey_trait_get_element(trait, name);
}

struct dicey_element *dicey_trait_get_element(const struct dicey_trait *const trait, const char *const name) {
    assert(trait && name && *name);

    return dicey_hashtable_get(trait->elems, name);
}

bool dicey_trait_init(struct dicey_trait *const trait, const char *const name) {
    assert(trait && name && *name);

    char *const name_copy = strdup(name);
    if (!name_copy) {
        return false;
    }

    struct dicey_hashtable *const elems = dicey_hashtable_new();
    if (!elems) {
        free(name_copy);

        return false;
    }

    *trait = (struct dicey_trait) {
        .name = name_copy,
        .elems = elems,
    };

    return true;
}

enum dicey_error dicey_trait_init_with(struct dicey_trait *const trait, const char *const name, ...) {
    struct dicey_trait *const new_trait = calloc(1U, sizeof *new_trait);
    if (!new_trait) {
        return DICEY_ENOMEM;
    }

    const enum dicey_error err = dicey_trait_init(new_trait, name);
    if (err) {
        free(new_trait);

        return err;
    }

    va_list ap;
    va_start(ap, name);

    for (;;) {
        const char *const name = va_arg(ap, const char *);
        if (!name) {
            break;
        }

        const enum dicey_error err = dicey_trait_add_element(new_trait, name, va_arg(ap, struct dicey_element));
        if (err) {
            dicey_trait_deinit(new_trait);
            free(new_trait);

            return err;
        }
    }

    va_end(ap);

    *trait = *new_trait;

    return DICEY_OK;
}

enum dicey_error dicey_trait_init_with_list(
    struct dicey_trait *const trait,
    const char *const name,
    const struct dicey_element_entry *const elems,
    const size_t count
) {
    assert(trait && name && ((bool) elems == (bool) count));

    struct dicey_trait *const new_trait = calloc(1U, sizeof *new_trait);
    if (!new_trait) {
        return DICEY_ENOMEM;
    }

    const enum dicey_error err = dicey_trait_init(new_trait, name);
    if (err) {
        free(new_trait);

        return err;
    }

    const struct dicey_element_entry *const end = elems + count;
    for (const struct dicey_element_entry *entry = elems; entry < end; ++entry) {
        const enum dicey_error err = dicey_trait_add_element(
            new_trait,
            entry->name,
            (struct dicey_element) {
                .signature = entry->signature,
                .type = entry->type,
            }
        );

        if (err) {
            dicey_trait_deinit(new_trait);
            free(new_trait);

            return err;
        }
    }

    *trait = *new_trait;

    return DICEY_OK;
}

struct dicey_trait_iter dicey_trait_iter_start(struct dicey_trait *const trait) {
    return (struct dicey_trait_iter) {
        ._inner = dicey_hashtable_iter_start(trait ? trait->elems : NULL),
    };
}

bool dicey_trait_iter_next(
    struct dicey_trait_iter *const iter,
    const char **const elem_name,
    struct dicey_element *const elem
) {
    assert(iter && elem_name && elem);

    void *elem_name_void;
    if (dicey_hashtable_iter_next(&iter->_inner, elem_name, &elem_name_void)) {
        assert(*elem_name && elem_name_void);

        *elem = *(struct dicey_element *) elem_name_void;

        return true;
    }

    return false;
}
