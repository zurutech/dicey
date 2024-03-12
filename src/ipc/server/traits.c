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

#include "ipc/typedescr.h"

#include "traits.h"

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

    if (elem) {
        // originally strdup'd
        free((char *) elem_cast->signature);
        free(elem_cast);
    }
}

enum dicey_error dicey_trait_add_element(
    struct dicey_trait *const trait,
    const char *const name,
    const struct dicey_element elem
) {
    assert(trait && trait->elems && elem.signature && *elem.signature && elem.type != DICEY_ELEMENT_TYPE_INVALID);

    if (!dicey_typedescr_is_valid(elem.signature)) {
        return DICEY_ESIGNATURE_MALFORMED;
    }

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
        return DICEY_EINVAL;

    case DICEY_HASH_SET_ADDED:
        assert(!old_val);

        break;
    }

    return DICEY_OK;
}

void dicey_trait_delete(struct dicey_trait *const trait) {
    if (trait) {
        assert(trait->elems);

        dicey_hashtable_delete(trait->elems, free_elem);

        free((char *) trait->name); // cast away const, this originated from strdup
        free(trait);
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

struct dicey_trait *dicey_trait_new(const char *const name) {
    assert(name && *name);

    char *const name_copy = strdup(name);
    if (!name_copy) {
        return NULL;
    }

    struct dicey_hashtable *const elems = dicey_hashtable_new();
    if (!elems) {
        free(name_copy);

        return NULL;
    }

    struct dicey_trait *const trait = malloc(sizeof *trait);
    if (!trait) {
        free(name_copy);
        dicey_hashtable_delete(elems, free_elem);

        return NULL;
    }

    *trait = (struct dicey_trait) {
        .name = name_copy,
        .elems = elems,
    };

    return trait;
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
