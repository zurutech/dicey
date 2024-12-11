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

#if !defined(PTDFNAAZWS_TYPE_H)
#define PTDFNAAZWS_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dicey_config.h"
#include "dicey_export.h"

#include "errors.h"
#include "views.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents a boolean value.
 */
typedef uint8_t dicey_bool;

/**
 * @brief Represents a byte value.
 */
typedef uint8_t dicey_byte;

/**
 * @brief Represents a signed 16-bit integer.
 */
typedef int16_t dicey_i16;

/**
 * @brief Represents a signed 32-bit integer.
 */
typedef int32_t dicey_i32;

/**
 * @brief Represents a signed 64-bit integer.
 */
typedef int64_t dicey_i64;

/**
 * @brief Represents an unsigned 16-bit integer.
 */
typedef uint16_t dicey_u16;

/**
 * @brief Represents an unsigned 32-bit integer.
 */
typedef uint32_t dicey_u32;

/**
 * @brief Represents an unsigned 64-bit integer.
 */
typedef uint64_t dicey_u64;

/**
 * @brief Represents an error code with an (optional) message.
 */
struct dicey_errmsg {
    int16_t code;        /**< The error code. */
    const char *message; /**< The error message (may be NULL). */
};

/**
 * @brief Represents a double precision floating-point value.
 */
typedef double dicey_float;

/**
 * @brief Represents a selector, i.e. a (trait:element) pair.
 */
struct dicey_selector {
    const char *trait; /**< The trait of the selector. */
    const char *elem;  /**< The element of the selector. */
};

/**
 * @brief Checks if a selector is valid.
 * @param selector The selector to check.
 * @return true if the selector is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_selector_is_valid(struct dicey_selector selector);

/**
 * @brief Gets the size of a selector.
 * @param sel The selector.
 * @return When > 0, size(element) + size(trait) + 2 (for the two null-terminators)
 *         When < 0, The returned value is a valid member of the dicey_error enum representing the error.
 *         Possible errors:
 *         - EOVERFLOW: size(element) + size(trait) + 2 overflows ptrdiff_t.
 */
DICEY_EXPORT ptrdiff_t dicey_selector_size(struct dicey_selector sel);

// __uint128_t is only available on GCC and Clang on 64-bit platforms
#if (defined(DICEY_CC_IS_GCC) || defined(DICEY_CC_IS_CLANG)) && defined(DICEY_HAS_64BIT_POINTERS)
#define DICEY_UUID_SIZE sizeof(__uint128_t)
#else
#define DICEY_UUID_SIZE (sizeof(uint64_t) * 2U)
#endif

/**
 * @brief Represents a UUID.
 * @note The UUID is represented as a 128-bit big-endian unsigned integer.
 */
struct dicey_uuid {
    uint8_t bytes[DICEY_UUID_SIZE];
};

/**
 * @brief Reads a UUID from a view.
 * @param uuid The dicey_uuid structure to fill.
 * @param bytes The bytes containing the UUID.
 * @param len The length of the bytes array.
 * @return The result of the operation. Possible values:
 *        - OK: Success.
 *        - EUUID_NOT_VALID: The view is not DICEY_UUID_SIZE bytes.
 */
DICEY_EXPORT enum dicey_error dicey_uuid_from_bytes(struct dicey_uuid *uuid, const uint8_t *bytes, size_t len);

/**
 * @brief Parses a string as an UUID with format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" or
 * "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx".
 * @param uuid The dicey_uuid structure to fill.
 * @param str The UUID to parse. Can be either a 32-character string or a 36-character string with dashes.
 * @return The result of the operation. Possible values:
 *        - OK: Success.
 *        - EUUID_NOT_VALID: The UUID is not valid.
 */
DICEY_EXPORT enum dicey_error dicey_uuid_from_string(struct dicey_uuid *uuid, const char *str);

/**
 * @brief Identifies the type a Dicey value may represent
 */
enum dicey_type {
    DICEY_TYPE_INVALID = 0, /**< Invalid type. */

    DICEY_TYPE_UNIT = '$', /**< Unit type. */

    DICEY_TYPE_BOOL = 'b', /**< Boolean type. */
    DICEY_TYPE_BYTE = 'c', /**< Byte type. */

    DICEY_TYPE_FLOAT = 'f', /**< Floating-point type. */

    DICEY_TYPE_INT16 = 'n', /**< 16-bit signed integer type. */
    DICEY_TYPE_INT32 = 'i', /**< 32-bit signed integer type. */
    DICEY_TYPE_INT64 = 'x', /**< 64-bit signed integer type. */

    DICEY_TYPE_UINT16 = 'q', /**< 16-bit unsigned integer type. */
    DICEY_TYPE_UINT32 = 'u', /**< 32-bit unsigned integer type. */
    DICEY_TYPE_UINT64 = 't', /**< 64-bit unsigned integer type. */

    DICEY_TYPE_ARRAY = '[', /**< Array type. */
    DICEY_TYPE_TUPLE = '(', /**< Tuple type. */
    DICEY_TYPE_PAIR = '{',  /**< Pair type. */

    DICEY_TYPE_BYTES = 'y', /**< Bytes type. */
    DICEY_TYPE_STR = 's',   /**< String type. */

    DICEY_TYPE_UUID = '#', /**< 128-bit UUID type. Big-endian */

    DICEY_TYPE_PATH = '@',     /**< Path type. */
    DICEY_TYPE_SELECTOR = '%', /**< Selector type. */

    DICEY_TYPE_ERROR = 'e', /**< Error type. */
};

/**
 * @brief Checks if a type is a container type (i.e. it contains other values recursively).
 * @note The only supported container types are: array, tuple, pair.
 * @param type The type to check.
 * @return true if the type is a container type, false otherwise.
 */
DICEY_EXPORT bool dicey_type_is_container(enum dicey_type type);

/**
 * @brief Checks if a type is valid.
 * @param type The type to check.
 * @return true if the type is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_type_is_valid(enum dicey_type type);

/**
 * @brief Gets the name of a type.
 * @param type The type (which must be a valid enum value).
 * @return The name of the type.
 */
DICEY_EXPORT const char *dicey_type_name(enum dicey_type type);

/**
 * @brief Represents the ID of a variant. This is not a real type, but a special tag that can be used to identify a
 *        variant in a list context.
 */
#define DICEY_VARIANT_ID ((uint16_t) 'v')

#if defined(__cplusplus)
}
#endif

#endif // PTDFNAAZWS_TYPE_H
