// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/builders.h>

#include "packet-args.h"

static bool arglist_copy(
    const struct dicey_arg **const dest,
    const struct dicey_arg *const src,
    const uint16_t nitems
) {
    struct dicey_arg *const list_dup = calloc(sizeof *list_dup, nitems);
    if (!list_dup) {
        return false;
    }

    // clone the contents, if any
    for (size_t i = 0U; i < nitems; ++i) {
        if (!dicey_arg_dup(list_dup + i, src + i)) {
            return false;
        }
    }

    *dest = list_dup;

    return true;
}

struct dicey_arg *dicey_arg_dup(struct dicey_arg *dest, const struct dicey_arg *src) {
    assert(src);

    if (!dest) {
        dest = calloc(sizeof *dest, 1U);
        if (!dest) {
            return NULL;
        }
    }

    *dest = *src;

    switch (src->type) {
    default:
        break;

    case DICEY_TYPE_ARRAY:
        if (!arglist_copy(&dest->array.elems, src->array.elems, src->array.nitems)) {
            return NULL;
        }

        break;

    case DICEY_TYPE_TUPLE:
        if (!arglist_copy(&dest->tuple.elems, dest->tuple.elems, src->tuple.nitems)) {
            return NULL;
        }

        break;

    case DICEY_TYPE_PAIR:
        {
            struct dicey_arg *const first = dicey_arg_dup(NULL, src->pair.first), *const second = dicey_arg_dup(
                                                                                      NULL, src->pair.second
                                                                                  );

            if (!first || !second) {
                dicey_arg_free(first);
                dicey_arg_free(second);

                return NULL;
            }

            dest->pair = (struct dicey_pair_arg) { .first = first, .second = second };
        }
    }

    return dest;
}

void dicey_arg_free(const struct dicey_arg *const arg) {
    if (!arg) {
        return;
    }

    dicey_arg_free_contents(arg);

    // these are guaranteed to come from malloc - so I have no problems casting them back to mutable.
    // free is just a bad API
    free((void *) arg);
}

void dicey_arg_free_contents(const struct dicey_arg *const arg) {
    if (!arg) {
        return;
    }

    const struct dicey_arg *list = NULL, *end = NULL;
    dicey_arg_get_list(arg, &list, &end);

    if (list) {
        assert(end);

        for (const struct dicey_arg *item = list; item != end; ++item) {
            dicey_arg_free_contents(item);
        }

        free((void *) list);
    } else if (arg->type == DICEY_TYPE_PAIR) {
        dicey_arg_free(arg->pair.first);
        dicey_arg_free(arg->pair.second);
    }
}

void dicey_arg_get_list(
    const struct dicey_arg *const arg,
    const struct dicey_arg **const list,
    const struct dicey_arg **const end
) {
    assert(arg && list && end);

    switch (arg->type) {
    default:
        *list = *end = NULL;

        break;

    case DICEY_TYPE_ARRAY:
        *list = arg->array.elems;
        *end = *list + arg->array.nitems;
        break;

    case DICEY_TYPE_TUPLE:
        *list = arg->tuple.elems;
        *end = *list + arg->tuple.nitems;
        break;
    }
}

struct dicey_arg *dicey_arg_move(struct dicey_arg *dest, struct dicey_arg *src) {
    assert(src);

    if (!dest) {
        dest = calloc(sizeof *dest, 1U);
        if (!dest) {
            return NULL;
        }
    }

    *dest = *src;
    *src = (struct dicey_arg) { 0 };

    return dest;
}
