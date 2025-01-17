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
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <dicey/core/type.h>
#include <dicey/core/typedescr.h>
#include <dicey/core/views.h>

#include "sup/view-ops.h"

#define ARRAY_END ']'
#define PAIR_END '}'
#define TUPLE_END ')'

static bool is_valid_type(const int ch) {
    return dicey_type_is_valid(ch) || ch == DICEY_VARIANT_ID;
}

static bool match_exact(const char **const cur, const char *const end, const char *const tag) {
    assert(cur && *cur && end && tag);

    for (const char *tag_cur = tag; *tag_cur; ++tag_cur) {
        if (*cur == end || **cur != *tag_cur) {
            return false;
        }

        ++*cur;
    }

    return true;
}

static void skip_whitespace(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    while (*cur != end && isspace((unsigned short) **cur)) {
        ++*cur;
    }
}

static int take_one(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    if (*cur == end) {
        return '\0';
    }

    return *(*cur)++;
}

static bool parse_arrow(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    skip_whitespace(cur, end);

    if (!match_exact(cur, end, "->")) {
        return false;
    }

    skip_whitespace(cur, end);

    return true;
}

static bool parse_type(const char **begin, const char *end);

static bool parse_array(const char **const cur, const char *const end) {
    assert(cur && end);

    if (!parse_type(cur, end)) {
        return false;
    }

    return take_one(cur, end) == ARRAY_END;
}

static bool parse_pair(const char **const cur, const char *const end) {
    assert(cur && end);

    if (!parse_type(cur, end)) {
        return false;
    }

    if (!parse_type(cur, end)) {
        return false;
    }

    return take_one(cur, end) == PAIR_END;
}

static bool parse_tuple(const char **const cur, const char *const end) {
    assert(cur && end);

    while (parse_type(cur, end)) {
        if (*cur == end) {
            return false;
        }

        if (**cur == TUPLE_END) {
            take_one(cur, end);

            return true;
        }
    }

    return false;
}

static bool parse_type(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    if (*cur == end) {
        return cur;
    }

    const int first = take_one(cur, end);

    switch (first) {
    default:
        return is_valid_type(first);

    case DICEY_TYPE_ARRAY:
        return parse_array(cur, end);

    case DICEY_TYPE_PAIR:
        return parse_pair(cur, end);

    case DICEY_TYPE_TUPLE:
        return parse_tuple(cur, end);
    }
}

bool dicey_typedescr_in_view(struct dicey_view *view) {
    assert(view);

    const char *const beg = view->data;
    const char *cur = beg;

    if (!parse_type(&cur, cur + view->len)) {
        return false;
    }

    assert(cur >= beg);

    const size_t bytes_read = cur - beg;
    assert(bytes_read <= view->len);

    *view = dicey_view_from(cur, view->len - bytes_read);

    return true;
}

bool dicey_typedescr_is_valid(const char *const typedescr) {
    return dicey_typedescr_parse(typedescr, &(struct dicey_typedescr) { 0 });
}

bool dicey_typedescr_parse(const char *typedescr, struct dicey_typedescr *const descr) {
    assert(typedescr && descr);

    const char *const root = typedescr;
    const char *const end = typedescr + strlen(typedescr);

    if (!parse_type(&typedescr, end)) {
        return false;
    }

    if (typedescr == end) {
        *descr = (struct dicey_typedescr) {
            .kind = DICEY_TYPEDESCR_VALUE,
            .value = root,
        };

        return true;
    }

    const struct dicey_view input = dicey_view_from(root, typedescr - root);

    if (!parse_arrow(&typedescr, end)) {
        return false;
    }

    if (typedescr == end) {
        return false;
    }

    const char *const output_start = typedescr;

    if (!parse_type(&typedescr, end) || typedescr != end) {
        return false;
    }

    const struct dicey_view output = dicey_view_from(output_start, end - output_start);

    *descr = (struct dicey_typedescr){
        .kind = DICEY_TYPEDESCR_FUNCTIONAL,
        .op = {
            .input = input,
            .output = output,
        },
    };

    return true;
}
