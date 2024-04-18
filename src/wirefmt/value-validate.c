// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <dicey/core/type.h>
#include <dicey/core/typedescr.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>

#include "sup/assume.h"
#include "sup/util.h"
#include "sup/view-ops.h"

static bool is_compatible(const enum dicey_type value, const uint16_t type) {
    assert(dicey_type_is_valid(value));

    return type == DICEY_VARIANT_ID || value == (enum dicey_type) type;
}

static uint16_t take_elem(struct dicey_view *const sig) {
    assert(sig);

    uint8_t elem = 0U;

    if (!dicey_view_read(
            sig,
            (struct dicey_view_mut) {
                .data = &elem,
                .len = sizeof elem,
            }
        )) {
        return DICEY_TYPE_INVALID;
    }

    return elem;
}

static int skip_char(struct dicey_view *const sig) {
    assert(sig);

    uint8_t byte = 0U;

    const ptrdiff_t read = dicey_view_read(
        sig,
        (struct dicey_view_mut) {
            .data = &byte,
            .len = sizeof byte,
        }
    );

    assert(read == sizeof byte);

    DICEY_UNUSED(read);

    return byte;
}

static bool checksig(struct dicey_view *const sig, const struct dicey_value *const value) {
    assert(value && sig && dicey_view_is_valid(*sig));

    const uint16_t elem_ty = take_elem(sig);

    if (!is_compatible(dicey_value_get_type(value), elem_ty)) {
        return false;
    }

    switch (elem_ty) {
    case DICEY_TYPE_ARRAY:
        {
            struct dicey_list list = { 0 };
            DICEY_ASSUME(dicey_value_get_array(value, &list));

            const uint16_t array_ty = take_elem(sig);
            assert(array_ty != DICEY_TYPE_INVALID); // the signature is assumed valid

            if (!is_compatible(dicey_list_type(&list), array_ty)) {
                return false;
            }

            const int cpar = skip_char(sig);
            assert(cpar == ']'); // TODO: export this as a constant, this requires a new header though

            return true;
        }

    case DICEY_TYPE_TUPLE:
        {
            struct dicey_list list = { 0 };
            DICEY_ASSUME(dicey_value_get_tuple(value, &list));

            struct dicey_iterator iter = dicey_list_iter(&list);
            while (dicey_iterator_has_next(iter)) {
                struct dicey_value elem = { 0 };
                DICEY_ASSUME(dicey_iterator_next(&iter, &elem));

                if (!checksig(sig, &elem)) {
                    return false;
                }
            }

            const int cpar = skip_char(sig);
            assert(cpar == ')'); // TODO: export this as a constant, this requires a new header though

            return true;
        }

    case DICEY_TYPE_PAIR:
        {
            struct dicey_pair pair = { 0 };
            DICEY_ASSUME(dicey_value_get_pair(value, &pair));

            if (!checksig(sig, &pair.first)) {
                return false;
            }

            if (!checksig(sig, &pair.second)) {
                return false;
            }

            const int cpar = skip_char(sig);
            assert(cpar == '}'); // TODO: export this as a constant, this requires a new header though

            return true;
        }

    default:
        // ok, we are compatibile and not a composite type, we are done
        return true;
    }
}

bool dicey_value_can_be_returned_from(const struct dicey_value *value, const char *sigstr) {
    assert(value && sigstr);

    if (dicey_value_get_type(value) == DICEY_TYPE_ERROR) {
        return true; // errors can be returned by any operation or property
    }

    struct dicey_typedescr descr = { 0 };
    if (!dicey_typedescr_parse(sigstr, &descr)) {
        return false;
    }

    struct dicey_view sig = { 0 };

    switch (descr.kind) {
    case DICEY_TYPEDESCR_INVALID:
        assert(false);

        return false;

    case DICEY_TYPEDESCR_VALUE:
        sig = dicey_view_from_str(descr.value);
        break;

    case DICEY_TYPEDESCR_FUNCTIONAL:
        sig = descr.op.output;

        break;
    }

    return checksig(&sig, value);
}

bool dicey_value_is_compatible_with(const struct dicey_value *const value, const char *const sigstr) {
    assert(value && sigstr);

    struct dicey_typedescr descr = { 0 };
    if (!dicey_typedescr_parse(sigstr, &descr)) {
        return false;
    }

    struct dicey_view sig = { 0 };

    switch (descr.kind) {
    case DICEY_TYPEDESCR_INVALID:
        assert(false);

        return false;

    case DICEY_TYPEDESCR_VALUE:
        sig = dicey_view_from_str(descr.value);
        break;

    case DICEY_TYPEDESCR_FUNCTIONAL:
        sig = descr.op.input;

        break;
    }

    return checksig(&sig, value);
}
