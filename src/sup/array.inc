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

// this file defines a basic dynamic array implementation
// this struct supports iteration, deletion and appending
// just define ARRAY_TYPE_NAME and ARRAY_VALUE_TYPE before including this file
// note: ARRAY_TYPE_NAME will be defined as a struct, so in the end expect the definition of `struct ARRAY_TYPE_NAME`
// ARRAY_EXPORT will not put static in front of the functions, allowing you to re-export them if you so desire

#if !defined(ARRAY_TYPE_NAME)
#error "ARRAY_TYPE_NAME must be defined"
#endif // !defined(ARRAY_TYPE_NAME)

#if !defined(ARRAY_VALUE_TYPE)
#error "ARRAY_VALUE_TYPE must be defined"
#endif // !defined(ARRAY_VALUE_TYPE)

// note: this just doesn't set static to the external functions
#if defined(ARRAY_EXPORT)
#define ARRAY_VISIBILITY
#else
#define ARRAY_VISIBILITY static
#endif // defined(ARRAY_EXPORT)

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(ARRAY_BASE_CAP)
#define ARRAY_BASE_CAP 128U
#endif // !defined(BASE_CAP)

// temporary support macro
#define ARRAY_VALUE_TYPE_ALIAS ARRAY_TYPE_NAME ## _value_type
typedef ARRAY_VALUE_TYPE ARRAY_VALUE_TYPE_ALIAS;

struct ARRAY_TYPE_NAME {
    size_t len, cap;
    ARRAY_VALUE_TYPE_ALIAS data[];
};

#define ARRAY_GLUE_IDENTS_IMPL(A, B) A ## _ ## B
#define ARRAY_GLUE_IDENTS(A, B) ARRAY_GLUE_IDENTS_IMPL(A, B)
#define ARRAY_FUNCTION(NAME) ARRAY_GLUE_IDENTS(ARRAY_TYPE_NAME, NAME)
#define ARRAY_EXPORTED_FUNCTION(NAME) ARRAY_FUNCTION(NAME)
#define ARRAY_EXPORTED_FUNCTION_DECL(NAME, RET, ...) ARRAY_VISIBILITY RET ARRAY_EXPORTED_FUNCTION(NAME)(__VA_ARGS__)
#define ARRAY_INTERNAL_FUNCTION(NAME) ARRAY_FUNCTION(ARRAY_GLUE_IDENTS(internal, NAME))
#define ARRAY_INTERNAL_FUNCTION_DECL(NAME, RET, ...) static RET ARRAY_INTERNAL_FUNCTION(NAME)(__VA_ARGS__)

ARRAY_INTERNAL_FUNCTION_DECL(grow_if_needed, bool, struct ARRAY_TYPE_NAME **const list_ptr) {
    assert(list_ptr);

    struct ARRAY_TYPE_NAME *list = *list_ptr;

    const size_t len = list ? list->len : 0U;
    const size_t old_cap = list ? list->cap : 0U;

    if (len < old_cap) {
        return true;
    }

    const size_t new_cap = old_cap ? old_cap * 3U / 2U : ARRAY_BASE_CAP;

    if (new_cap < old_cap) { // overflow
        return false;
    }

    const size_t new_size = sizeof *list + new_cap * sizeof *list->data;

    list = list ? realloc(list, new_size) : calloc(1U, new_size);
    if (!list) {
        free(*list_ptr);

        return false;
    }

    list->cap = new_cap;

    *list_ptr = list;

    return true;
}

ARRAY_EXPORTED_FUNCTION_DECL(append, bool,  struct ARRAY_TYPE_NAME **const list_ptr, ARRAY_VALUE_TYPE_ALIAS value) {
    assert(list_ptr);

    if (!ARRAY_INTERNAL_FUNCTION(grow_if_needed)(list_ptr)) {
        return false;
    }

    struct ARRAY_TYPE_NAME *const list = *list_ptr;

    list->data[list->len++] = value;

    return true;
}

ARRAY_EXPORTED_FUNCTION_DECL(begin, const ARRAY_VALUE_TYPE_ALIAS *, const struct ARRAY_TYPE_NAME *const list) {
    return list ? list->data : NULL;
}

ARRAY_EXPORTED_FUNCTION_DECL(clear, void, struct ARRAY_TYPE_NAME *const list) {
    if (list) {
        list->len = 0U;
    }
}

ARRAY_EXPORTED_FUNCTION_DECL(end, const ARRAY_VALUE_TYPE_ALIAS *, const struct ARRAY_TYPE_NAME *list) {
    return list ? list->data + list->len : NULL;
}

ARRAY_EXPORTED_FUNCTION_DECL(erase, void, struct ARRAY_TYPE_NAME *const list, const size_t entry) {
    assert(list && entry < list->len && list->len);

    if (entry + 1 < list->len) {
        const ptrdiff_t len_after = list->len - entry - 1;

        memmove(&list->data[entry], &list->data[entry + 1], len_after * sizeof *list->data);
    }

    --list->len;
}

// undef this for future usage
#undef ARRAY_EXPORTED_FUNCTION
#undef ARRAY_EXPORTED_FUNCTION_DECL
#undef ARRAY_FUNCTION
#undef ARRAY_GLUE_IDENTS_IMPL
#undef ARRAY_GLUE_IDENTS
#undef ARRAY_INTERNAL_FUNCTION
#undef ARRAY_INTERNAL_FUNCTION_DECL
#undef ARRAY_VALUE_TYPE_ALIAS
#undef ARRAY_VISIBILITY
