// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(HHQPUVHYDW_ERRORS_H)
#define HHQPUVHYDW_ERRORS_H

#include <stdbool.h>
#include <stddef.h>

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif
/**
 * @brief Enumeration of error codes used in the dicey library.
 */
enum dicey_error {
    DICEY_OK = 0x0000, /**< No error occurred. */

    /**< Resource temporarily unavailable, or not enough data for a given operation */
    DICEY_EAGAIN = -0x0101,

    DICEY_ENOMEM = -0x0102,       /**< Out of memory. */
    DICEY_EINVAL = -0x0103,       /**< Invalid argument(s). */
    DICEY_ENODATA = -0x0104,      /**< No data available. */
    DICEY_EBADMSG = -0x0105,      /**< Bad message format. */
    DICEY_EOVERFLOW = -0x0106,    /**< Value or buffer overflow. */
    DICEY_ECONNREFUSED = -0x0107, /**< Connection refused. */
    DICEY_ETIMEDOUT = -0x0108,    /**< Operation timed out. */
    DICEY_ECANCELLED = -0x0109,   /**< Operation cancelled. */
    DICEY_EALREADY = -0x010A,     /**< Operation already in progress. */

    DICEY_EPATH_TOO_LONG = -0x020B,  /**< A path is too long. */
    DICEY_ETUPLE_TOO_LONG = -0x020C, /**< Tuple too long. (currently unused) */
    DICEY_EARRAY_TOO_LONG = -0x020D, /**< Array too long. (currently unused) */

    DICEY_EVALUE_TYPE_MISMATCH = -0x030E, /**< Value type mismatch. */

    DICEY_ENOT_SUPPORTED = -0x040F,  /**< Operation not supported. */
    DICEY_ECLIENT_TOO_OLD = -0x0410, /**< Client is too old. */
    DICEY_ESERVER_TOO_OLD = -0x0411, /**< Client is too old. */

    DICEY_EUV_UNKNOWN = -0x0512 /**< Unknown libuv error. */
};

/**
 * @brief Structure that describes an error code.
 */
struct dicey_error_def {
    enum dicey_error errnum; /**< The error code. */
    const char *name;        /**< The name of the error code in PascalCase. */
    const char *message;     /**< The error message. */
};

/**
 * @brief Get the error information for a specific error code.
 * @note The returned pointer is statically allocated.
 * @param errnum The error code.
 * @return A pointer to the dicey_error_def structure containing the error information, or NULL if the error is invalid.
 */
DICEY_EXPORT const struct dicey_error_def *dicey_error_info(enum dicey_error errnum);

/**
 * @brief Get all the error definitions in the dicey library as a list.
 * @note The list is sorted by error code and is statically allocated.
 *
 * @param defs Pointer to an array of dicey_error_def structures.
 * @param count Pointer to a variable to store the number of error definitions.
 */
DICEY_EXPORT void dicey_error_infos(const struct dicey_error_def **defs, size_t *count);

/**
 * @brief Check if a specific error code is valid.
 * @param errnum The error code.
 * @return true if the error code is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_error_is_valid(enum dicey_error errnum);

/**
 * @brief Get the error message for a specific error code.
 *
 *
 * @param errnum The error code.
 * @return The error message as a null-terminated string, or NULL if the error is invalid.
 */
DICEY_EXPORT const char *dicey_error_msg(enum dicey_error errnum);

/**
 * @brief Get the name of a specific error code.
 * @note The returned pointer is statically allocated.
 * @param errnum The error code.
 * @return The name of the error code as a null-terminated string, or NULL if the error is invalid.
 */
DICEY_EXPORT const char *dicey_error_name(enum dicey_error errnum);

#if defined(__cplusplus)
}
#endif

#endif // HHQPUVHYDW_ERRORS_H
