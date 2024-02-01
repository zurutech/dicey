// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(KWHQWHOQKQ_TYPES_H)
#define KWHQWHOQKQ_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents an immutable view of data.
 */
struct dicey_view {
    size_t      len;  /**< The length of the view. */
    const void *data; /**< A pointer to the data. */
};

/**
 * @brief Represents a mutable view of data.
 */
struct dicey_view_mut {
    size_t len;  /**< The length of the view. */
    void  *data; /**< A pointer to the data. */
};

#if defined(__cplusplus)
}
#endif

#endif // KWHQWHOQKQ_TYPES_H
