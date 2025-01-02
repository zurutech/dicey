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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>

#include "trace.h"
#include "util.h"

#if defined(DICEY_CC_IS_MSVC)

#define CHECKED_SUM_IMPL(NAME, T)                                                                                      \
    bool dutl_internal_##NAME##_add(T *const res, const T a, const T b) {                                              \
        const T sum = a + b;                                                                                           \
                                                                                                                       \
        if (sum < a) {                                                                                                 \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        *res = sum;                                                                                                    \
                                                                                                                       \
        return true;                                                                                                   \
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
        return TRACE(DICEY_EOVERFLOW);
    }

    return (ptrdiff_t) len + 1U;
}
