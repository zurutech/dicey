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

#include <assert.h>
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/builders.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>

#include "sup/trace.h"

#include "packet-args.h"

static bool arglist_copy(
    const struct dicey_arg **const dest,
    const struct dicey_arg *const src,
    const uint16_t nitems
) {
    struct dicey_arg *const list_dup = calloc(nitems, sizeof *list_dup);
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

static enum dicey_error value_list_to_arg(
    const struct dicey_list list,
    struct dicey_arg *const dest,
    const enum dicey_type type
) {
    assert(dest);
    assert(type == DICEY_TYPE_ARRAY || type == DICEY_TYPE_TUPLE);

    dest->type = type;

    size_t cap = 8U;
    uint16_t len = 0U;

    struct dicey_arg *elems = calloc(cap, sizeof(*elems));
    if (!elems) {
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_iterator iter = dicey_list_iter(&list);
    struct dicey_value value = { 0 };

    while (dicey_iterator_next(&iter, &value)) {
        if (len == UINT16_MAX) {
            dicey_arg_free_list(elems, len);

            return TRACE(DICEY_EOVERFLOW); // too many items
        }

        if (len == cap) {
            cap *= 2;

            struct dicey_arg *const new_elems = realloc(elems, cap * sizeof *new_elems);
            if (!new_elems) {
                dicey_arg_free_list(elems, len);

                return TRACE(DICEY_ENOMEM);
            }

            elems = new_elems;
        }

        const enum dicey_error err = dicey_arg_from_borrowed_value(&elems[len], &value);
        if (err) {
            dicey_arg_free_list(elems, len);

            return err;
        }

        ++len;
    }

    if (type == DICEY_TYPE_ARRAY) {
        dest->array.type = (enum dicey_type) dicey_list_type(&list);
        dest->array.elems = elems;
        dest->array.nitems = len;
    } else {
        dest->tuple.elems = elems;
        dest->tuple.nitems = len;
    }

    return DICEY_OK;
}

static enum dicey_error value_pair_to_arg(struct dicey_arg *const dest, const struct dicey_pair pair) {
    struct dicey_arg *const first = calloc(1, sizeof(*first));
    if (!first) {
        return TRACE(DICEY_ENOMEM);
    }

    const enum dicey_error first_err = dicey_arg_from_borrowed_value(first, &pair.first);
    if (first_err) {
        free(first);
        return first_err;
    }

    struct dicey_arg *const second = calloc(1, sizeof(*second));
    if (!second) {
        dicey_arg_free(first);
        return TRACE(DICEY_ENOMEM);
    }

    const enum dicey_error second_err = dicey_arg_from_borrowed_value(second, &pair.second);
    if (second_err) {
        dicey_arg_free(first);
        free(second);
        return second_err;
    }

    dest->pair.first = first;
    dest->pair.second = second;

    return DICEY_OK;
}

struct dicey_arg *dicey_arg_dup(struct dicey_arg *dest, const struct dicey_arg *src) {
    assert(src);

    if (!dest) {
        dest = calloc(1U, sizeof *dest);
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

void dicey_arg_free_list(const struct dicey_arg *const arglist, const size_t nitems) {
    if (!arglist || !nitems) {
        return;
    }

    const struct dicey_arg *const end = arglist + nitems;
    for (const struct dicey_arg *it = arglist; it != end; ++it) {
        dicey_arg_free_contents(it);
    }

    // these are guaranteed to come from malloc - so I have no problems casting them back to mutable.
    // free is just a bad API
    free((void *) arglist);
}

enum dicey_error dicey_arg_from_borrowed_value(struct dicey_arg *const dest, const struct dicey_value *const value) {
    assert(dest && value);

    dest->type = dicey_value_get_type(value);

    switch (dest->type) {
    case DICEY_TYPE_INVALID:
        return TRACE(DICEY_EINVAL);

    case DICEY_TYPE_UNIT:
        return DICEY_OK;

    case DICEY_TYPE_BOOL:
        {
            bool bool_value = false;

            const enum dicey_error err = dicey_value_get_bool(value, &bool_value);
            if (err) {
                return err;
            }

            dest->boolean = (dicey_bool) bool_value;

            return DICEY_OK;
        }

    case DICEY_TYPE_BYTE:
        return dicey_value_get_byte(value, &dest->byte);

    case DICEY_TYPE_FLOAT:
        return dicey_value_get_float(value, &dest->floating);

    case DICEY_TYPE_INT16:
        return dicey_value_get_i16(value, &dest->i16);

    case DICEY_TYPE_INT32:
        return dicey_value_get_i32(value, &dest->i32);

    case DICEY_TYPE_INT64:
        return dicey_value_get_i64(value, &dest->i64);

    case DICEY_TYPE_UINT16:
        return dicey_value_get_u16(value, &dest->u16);

    case DICEY_TYPE_UINT32:
        return dicey_value_get_u32(value, &dest->u32);

    case DICEY_TYPE_UINT64:
        return dicey_value_get_u64(value, &dest->u64);

    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_TUPLE:
        {
            struct dicey_list list = { 0 };
            const enum dicey_error err = dicey_value_get_list(value, &list);
            if (err) {
                return err;
            }

            return value_list_to_arg(list, dest, dest->type);
        }

    case DICEY_TYPE_PAIR:
        {
            struct dicey_pair pair = { 0 };
            const enum dicey_error err = dicey_value_get_pair(value, &pair);
            if (err) {
                return err;
            }

            return value_pair_to_arg(dest, pair);
        }

    case DICEY_TYPE_BYTES:
        {
            const uint8_t *data = NULL;
            size_t len = 0;
            const enum dicey_error err = dicey_value_get_bytes(value, &data, &len);
            if (err) {
                return err;
            }

            if (len > UINT32_MAX) {
                return TRACE(DICEY_EOVERFLOW);
            }

            dest->bytes = (struct dicey_bytes_arg) { .data = data, .len = (uint32_t) len };

            return DICEY_OK;
        }

    case DICEY_TYPE_STR:
        return dicey_value_get_str(value, &dest->str);

    case DICEY_TYPE_UUID:
        return dicey_value_get_uuid(value, &dest->uuid);

    case DICEY_TYPE_PATH:
        return dicey_value_get_path(value, &dest->path);

    case DICEY_TYPE_SELECTOR:
        return dicey_value_get_selector(value, &dest->selector);

    case DICEY_TYPE_ERROR:
        {
            struct dicey_errmsg errmsg = { 0 };
            const enum dicey_error err = dicey_value_get_error(value, &errmsg);
            if (err) {
                return err;
            }

            dest->error = (struct dicey_error_arg) { .code = errmsg.code, .message = errmsg.message };

            return DICEY_OK;
        }

    default:
        // this should never happen, but just in case
        return TRACE(DICEY_EINVAL);
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
        dest = calloc(1U, sizeof *dest);
        if (!dest) {
            return NULL;
        }
    }

    *dest = *src;
    *src = (struct dicey_arg) { 0 };

    return dest;
}
