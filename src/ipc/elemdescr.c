// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <dicey/core/errors.h>
#include <dicey/core/type.h>
#include <dicey/core/views.h>

#include "sup/trace.h"
#include "sup/view-ops.h"

#include "elemdescr.h"

#define DICEY_ELEMENT_DESCR_FMT "%s#%s:%s"

char *dicey_element_descriptor_format(const char *const path, const struct dicey_selector sel) {
    struct dicey_view_mut dest = { 0 };

    return dicey_element_descriptor_format_to(&dest, path, sel);
}

char *dicey_element_descriptor_format_to(
    struct dicey_view_mut *const dest,
    const char *const path,
    const struct dicey_selector sel
) {
    assert(dest && path && dicey_selector_is_valid(sel));

    const int required = snprintf(NULL, 0U, DICEY_ELEMENT_DESCR_FMT, path, sel.trait, sel.elem) + 1;
    assert(required > 6); // prevent empty strings from being formatted. at least 3 strings + 2 chars + NUL

    if ((size_t) required > dest->len) {
        char *const new_data = realloc(dest->data, (size_t) required);
        if (!new_data) {
            return NULL;
        }

        *dest = dicey_view_mut_from(new_data, (size_t) required);
    }

    snprintf(dest->data, dest->len, DICEY_ELEMENT_DESCR_FMT, path, sel.trait, sel.elem);

    return dest->data;
}
