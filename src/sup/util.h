// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(IVJHUOXLEC_UTIL_H)
#define IVJHUOXLEC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/views.h>

#define DICEY_UNUSED(X) ((void) (X))

#if defined(__GNUC__) || defined(__clang__)
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
