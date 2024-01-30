// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(KWHQWHOQKQ_TYPES_H)
#define KWHQWHOQKQ_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct dicey_view {
    size_t      len;
    const void *data;
};

struct dicey_view_mut {
    size_t len;
    void  *data;
};

#if defined(__cplusplus)
}
#endif

#endif // KWHQWHOQKQ_TYPES_H
