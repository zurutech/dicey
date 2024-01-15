#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "util.h"

#define CHECKED_SUM_IMPL(NAME, T) \
    bool dutl_##NAME##_add(T *const res, const T a, const T b) { \
        const T sum = a + b; \
        \
        if (sum < a) { \
            return false; \
        } \
        \
        *res = sum; \
        \
        return true; \
    }

CHECKED_SUM_IMPL(size, size_t)
CHECKED_SUM_IMPL(ssize, ptrdiff_t)
CHECKED_SUM_IMPL(u32, uint32_t)

ptrdiff_t dutl_zstring_size(const char *const str) {
    const size_t len = strlen(str);

    if (len > UINT32_MAX) {
        return DICEY_EOVERFLOW;
    }

    return (ptrdiff_t) len + 1U;
}
