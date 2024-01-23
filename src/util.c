#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>

#include "util.h"

#if defined(_MSC_VER)

#define CHECKED_SUM_IMPL(NAME, T) \
    bool dutl_internal_##NAME##_add(T *const res, const T a, const T b) { \
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

CHECKED_SUM_IMPL(i8, int8_t)
CHECKED_SUM_IMPL(i16, int16_t)
CHECKED_SUM_IMPL(i32, int32_t)
CHECKED_SUM_IMPL(i64, int64_t)

CHECKED_SUM_IMPL(u8, uint8_t)
CHECKED_SUM_IMPL(u16, uint16_t)
CHECKED_SUM_IMPL(u32, uint32_t)
CHECKED_SUM_IMPL(u64, uint64_t)

#endif

ptrdiff_t dutl_zstring_size(const char *const str) {
    const size_t len = strlen(str);

    if (len > UINT32_MAX) {
        return DICEY_EOVERFLOW;
    }

    return (ptrdiff_t) len + 1U;
}
