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

    DICEY_ENOENT = -0x0102,       /**< No such file or directory. */
    DICEY_ENOTDIR = -0x0103,      /**< Not a directory. */
    DICEY_ENOMEM = -0x0104,       /**< Out of memory. */
    DICEY_EINVAL = -0x0105,       /**< Invalid argument(s). */
    DICEY_ENODATA = -0x0106,      /**< No data available. */
    DICEY_EBADMSG = -0x0107,      /**< Bad message format. */
    DICEY_EOVERFLOW = -0x0108,    /**< Value or buffer overflow. */
    DICEY_ECONNREFUSED = -0x0109, /**< Connection refused. */
    DICEY_ETIMEDOUT = -0x010A,    /**< Operation timed out. */
    DICEY_ECANCELLED = -0x010B,   /**< Operation cancelled. */
    DICEY_EALREADY = -0x010C,     /**< Operation already in progress. */
    DICEY_EPIPE = -0x010D,        /**< Broken pipe. */
    DICEY_ECONNRESET = -0x010E,   /**< Connection reset. */
    DICEY_EEXIST = -0x010F,       /**< Object exists. */
    DICEY_EADDRINUSE = -0x0110,   /**< Address in use. */

    DICEY_EPATH_TOO_LONG = -0x0211,  /**< A path is too long. */
    DICEY_ETUPLE_TOO_LONG = -0x0212, /**< Tuple too long. (currently unused) */
    DICEY_EARRAY_TOO_LONG = -0x0213, /**< Array too long. (currently unused) */

    DICEY_EVALUE_TYPE_MISMATCH = -0x0314, /**< Value type mismatch. */

    DICEY_ENOT_SUPPORTED = -0x0415,       /**< Operation not supported. */
    DICEY_ECLIENT_TOO_OLD = -0x0416,      /**< Client is too old. */
    DICEY_ESERVER_TOO_OLD = -0x0417,      /**< Client is too old. */
    DICEY_EPATH_DELETED = -0x0418,        /**< Path has been deleted. */
    DICEY_EPATH_NOT_FOUND = -0x0419,      /**< Path not found. */
    DICEY_EPATH_MALFORMED = -0x041A,      /**< Path is malformed */
    DICEY_ETRAIT_NOT_FOUND = -0x041B,     /**< Trait not found. */
    DICEY_EELEMENT_NOT_FOUND = -0x041C,   /**< Element not found. */
    DICEY_ESIGNATURE_MALFORMED = -0x041D, /**< Signature is malformed. */
    DICEY_ESIGNATURE_MISMATCH = -0x041E,  /**< Signature is mismatched. */
    DICEY_EPROPERTY_READ_ONLY = -0x041F,  /**< Property is read-only. */
    DICEY_EPEER_NOT_FOUND = -0x0420,      /**< Peer not found. */
    DICEY_ESEQNUM_MISMATCH = -0x0421,     /**< Sequence number mismatch. */
    DICEY_EUUID_NOT_VALID = -0x0422,      /**< UUID is not valid. */

    DICEY_EUV_UNKNOWN = -0x0523 /**< Unknown libuv error. */
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
