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

#if !defined(IVJHUOXLEC_UTIL_H)
#define IVJHUOXLEC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/views.h>

#include "dicey_config.h"

#if defined(DICEY_CC_IS_GCC) || defined(DICEY_CC_IS_CLANG)

// if we have GNU extensions, we can use the kernel version with type checking using typeof and GCC's expression
// statements

#define DICEY_CONTAINEROF(PTR, TYPE, MEMBER)                                                                           \
    (__extension__({                                                                                                   \
        const __typeof__(((TYPE *) NULL)->MEMBER) *const _ptr_to_member_with_unique_name = (PTR);                      \
        (TYPE *) ((char *) _ptr_to_member_with_unique_name - offsetof(TYPE, MEMBER));                                  \
    }))

// if we have GNU extensions, we can check the format string and the arguments using GCC's format attribute

#if defined(DICEY_IS_MINGW) && defined(DICEY_CC_IS_GCC) // clang on Windows doesn't support ms_printf
#define DICEY_FORMAT(FPOS, VAPOS) __attribute__((format(ms_printf, FPOS, VAPOS)))
#else
#define DICEY_FORMAT(FPOS, VAPOS) __attribute__((format(printf, FPOS, VAPOS)))
#endif // DICEY_IS_WINDOWS

#else
#define DICEY_CONTAINEROF(PTR, TYPE, MEMBER) ((TYPE *) ((char *) (PTR) -offsetof(TYPE, MEMBER)))
#define DICEY_FORMAT(FPOS, VAPOS) // do not bother with SAL on MSVC
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define DICEY_FALLTHROUGH [[fallthrough]]
#elif defined(__has_attribute)
#if __has_attribute(fallthrough)
#define DICEY_FALLTHROUGH __attribute__((fallthrough))
#endif
#endif

#if !defined(DICEY_FALLTHROUGH)
#define DICEY_FALLTHROUGH // for msvc and friends
#endif

#define DICEY_LENOF(ARR) (sizeof(ARR) / sizeof(*(ARR)))

#define DICEY_UNUSED(X) ((void) (X))

#if defined(NDEBUG) && defined(__has_builtin)
#if __has_builtin(__builtin_unreachable)
#define DICEY_UNREACHABLE(X) __builtin_unreachable()
#else
#define DICEY_UNREACHABLE(X) // do nothing, we're in release mode and we don't have a builtin
#endif
#else
#define DICEY_UNREACHABLE(X) assert(!"Unreachable code reached")
#endif

/**
 * @brief Get a pointer to a member of a struct
 * @note This macro is designed to be used to write or read a member of a struct from a byte array. The returned pointer
 *       is meant to be used in conjunction with the memcpy, memcmp or memmove functions and should never be
 * dereferenced.
 * @param STRUCT The type of the struct
 * @param NAME The name of the member
 * @param BASE A pointer to the base of the struct
 * @return A uint8_t* pointer to the member, which can be used with memcmp, memcpy or memmove
 */
#define MEMBER_PTR(STRUCT, NAME, BASE) ((uint8_t *) (BASE) + offsetof(STRUCT, NAME))

#if defined(DICEY_CC_IS_GCC) || defined(DICEY_CC_IS_CLANG)
#define SAFE_ADD(DEST, A, B) (!__builtin_add_overflow((A), (B), (DEST)))
#else

// MSVC intrinsics are a mess. Take the slower path with hand-written code.

bool dutl_internal_i8_add(int8_t *res, int8_t a, int8_t b);
bool dutl_internal_i16_add(int16_t *res, int16_t a, int16_t b);
bool dutl_internal_i32_add(int32_t *res, int32_t a, int32_t b);
bool dutl_internal_i64_add(int64_t *res, int64_t a, int64_t b);

bool dutl_internal_u8_add(uint8_t *res, uint8_t a, uint8_t b);
bool dutl_internal_u16_add(uint16_t *res, uint16_t a, uint16_t b);
bool dutl_internal_u32_add(uint32_t *res, uint32_t a, uint32_t b);
bool dutl_internal_u64_add(uint64_t *res, uint64_t a, uint64_t b);

#define SAFE_ADD(DEST, A, B)                                                                                           \
    _Generic((A), int8_t                                                                                               \
             : dutl_internal_i8_add, int16_t                                                                           \
             : dutl_internal_i16_add, int32_t                                                                          \
             : dutl_internal_i32_add, int64_t                                                                          \
             : dutl_internal_i64_add, uint8_t                                                                          \
             : dutl_internal_u8_add, uint16_t                                                                          \
             : dutl_internal_u16_add, uint32_t                                                                         \
             : dutl_internal_u32_add, uint64_t                                                                         \
             : dutl_internal_u64_add)((DEST), (A), (B))
#endif

#define dutl_checked_add(DEST, A, B) ((bool) SAFE_ADD((DEST), (A), (B)))

#if defined(__cplusplus)
extern "C" {
#endif

ptrdiff_t dutl_zstring_size(const char *str);

#if defined(__cplusplus)
}
#endif

#endif // IVJHUOXLEC_UTIL_H
