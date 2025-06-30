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

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/typedescr.h>
#include <dicey/ipc/traits.h>

#include "sup/trace.h"

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

const char *dicey_element_type_name(const enum dicey_element_type type) {
    switch (type) {
    case DICEY_ELEMENT_TYPE_OPERATION:
        return "operation";

    case DICEY_ELEMENT_TYPE_PROPERTY:
        return "property";

    case DICEY_ELEMENT_TYPE_SIGNAL:
        return "signal";

    default:
        assert(false); // should never be reached
        return ">>invalid<<";
    }
}

enum dicey_error dicey_trait_add_element(
    struct dicey_trait *const trait,
    const char *const name,
    const struct dicey_element elem
) {
    assert(trait && elem.signature && *elem.signature && elem.type != DICEY_ELEMENT_TYPE_INVALID);

    struct dicey_typedescr descr = { 0 };

    if (!dicey_typedescr_parse(elem.signature, &descr)) {
        return TRACE(DICEY_ESIGNATURE_MALFORMED);
    }

    const bool is_op = elem.type == DICEY_ELEMENT_TYPE_OPERATION,
               is_func_sig = descr.kind == DICEY_TYPEDESCR_FUNCTIONAL;

    // if the element is an operation, the signature must be a function signature
    // conversely, if the element is a property or signal, the signature must be a value signature
    if (is_op != is_func_sig) {
        return TRACE(DICEY_ESIGNATURE_MISMATCH);
    }

    // todo: optimise this by implementing an "add-or-fail" function in hashtable
    if (dicey_hashtable_contains(trait->elems, name)) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_element *const elem_val = elem_dup(&elem);
    if (!elem_val) {
        return TRACE(DICEY_ENOMEM);
    }

    void *old_val = NULL;
    switch (dicey_hashtable_set(&trait->elems, name, elem_val, &old_val)) {
    case DICEY_HASH_SET_FAILED:
        free(elem_val);

        return TRACE(DICEY_ENOMEM);

    case DICEY_HASH_SET_UPDATED:
        assert(false); // should never be reached
        return TRACE(DICEY_EINVAL);

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

const struct dicey_element *dicey_trait_get_element(const struct dicey_trait *const trait, const char *const name) {
    assert(trait && name && *name);

    return dicey_hashtable_get(trait->elems, name);
}

bool dicey_trait_get_element_entry(
    const struct dicey_trait *const trait,
    const char *const name,
    struct dicey_element_entry *const entry
) {
    assert(trait && name && *name && entry);

    struct dicey_hashtable_entry table_entry = { 0 };

    if (!dicey_hashtable_get_entry(trait->elems, name, &table_entry)) {
        return false;
    }

    *entry = (struct dicey_element_entry) {
        .sel = {
            .trait = trait->name,
            .elem = table_entry.key,
        },
        .element = table_entry.value,
    };

    return true;
}

struct dicey_trait *dicey_trait_new(const char *const name) {
    assert(name && *name);

    char *const name_copy = strdup(name);
    if (!name_copy) {
        return NULL;
    }

    struct dicey_trait *const trait = malloc(sizeof *trait);
    if (!trait) {
        free(name_copy);

        return NULL;
    }

    *trait = (struct dicey_trait) {
        .name = name_copy,
    };

    return trait;
}

struct dicey_trait_iter dicey_trait_iter_start(const struct dicey_trait *const trait) {
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
