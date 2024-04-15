// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(WQWGWWXACS_TYPEDESCR_H)
#define WQWGWWXACS_TYPEDESCR_H

#include <stdbool.h>

#include "views.h"

#include "dicey_export.h"

enum dicey_typedescr_kind {
    DICEY_TYPEDESCR_INVALID,

    DICEY_TYPEDESCR_VALUE,
    DICEY_TYPEDESCR_FUNCTIONAL,
};

struct dicey_typedescr_op {
    struct dicey_view input;
    struct dicey_view output;
};

struct dicey_typedescr {
    enum dicey_typedescr_kind kind;

    union {
        const char *value;
        struct dicey_typedescr_op op;
    };
};

DICEY_EXPORT bool dicey_typedescr_is_valid(const char *typedescr);
DICEY_EXPORT bool dicey_typedescr_parse(const char *typedescr, struct dicey_typedescr *descr);

#endif // WQWGWWXACS_TYPEDESCR_H
