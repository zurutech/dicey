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

#if !defined(WQWGWWXACS_TYPEDESCR_H)
#define WQWGWWXACS_TYPEDESCR_H

#include <stdbool.h>

#include "views.h"

#include "dicey_export.h"

/**
 * Dicey Signature Format specification
 *
 * A Dicey signature is a string that describes the type of a value or operation. The format is inspired by the D-Bus
 * signature format, with the aim of being human-readable, easy to parse, and easy to spot from an hex dump.
 *
 * Each Dicey type is represented by a single ASCII character, as defined below (note: case sensitive):
 *
 * - '$': Unit (no value)
 * - 'b': Boolean (0 or 1)
 * - 'c': Byte (8-bit unsigned integer)
 * - 'f': Floating-point (64 bit IEEE 754)
 * - 'n': 16-bit signed integer (little-endian, two's complement)
 * - 'i': 32-bit signed integer (little-endian, two's complement)
 * - 'x': 64-bit signed integer (little-endian, two's complement)
 * - 'q': 16-bit unsigned integer (little-endian)
 * - 'u': 32-bit unsigned integer (little-endian)
 * - 't': 64-bit unsigned integer (little-endian)
 * - '[': Array of a single type. The type is specified after the '[' character, like `[i]`. The trailing ']' is
 *        only for cleanliness, and is not part of the type.
 * - '(': Tuple of multiple types. The types are specified after the '(' character, like `(i@b)`. The trailing ')' is
 *        only for cleanliness, and is not part of the type.
 * - '{': Pair of two types. The types are specified after the '{' character, like `{@b}`. The trailing '}' is only for
 *        cleanliness, and is not part of the type.
 * - 'y': Byte array (variable length)
 * - 's': String (variable length, null-terminated)
 * - '@': Path (null-terminated string representing a path)
 * - '%': Selector (two null-terminated strings, element and trait)
 * - 'e': Error, i.e. a struct comprising an error code (u16) and an optional error message (string, null-terminated)
 *
 * Furthermore, signatures support the special variant type `v`. In Dicey in general all types are variant, and it's up
 * to signatures to actually enforce a specific type. `v` stands in as a placeholder for those times where any type is
 * acceptable.
 *
 * Function-like types can be represented by using the `->` separator. For example, `i->s` represents a function-like
 * element (operation in Dicey jargon) that takes an integer and returns a string. All spaces around arrows are ignored,
 * so `i -> s`, `i->s`, and `i               -> s` are all equivalent.
 * Note that this rule is only valid for function-like types. For all other types, spaces are not allowed.
 *
 * The "error" ('e') type is a special type that can be always returned by any operation or property access, even if the
 * signature itself does not annoverate `e` as a possible return type. In this, all return values somewhat operate like
 * as if returning a union, where the actual return type can either be the signature's return type or `e`.
 * This also means that it is not recommended to explicitly return `e` as a standalone type in signatures such as `s ->
 e`, and returning the `$` (unit) type should be preferred instead (i.e. `s -> $`).
 *
 * More in detail, the grammar for a Dicey signature can be summarized as follows:
 *
 * typedescr = value | operation
 * operation = value, [whitespace], '->', [whitespace], value
 * value = unit | bool | byte | float | i16 | i32 | i64 | u16 | u32 | u64 | array | tuple | pair | bytes | str | path
            | selector | error | variant
 * array = '[', value, ']'
 * tuple = '(', value, {value}, ')'
 * pair = '{', value, value, '}'
 * unit = '$'
 * bool = 'b'
 * byte = 'c'
 * float = 'f'
 * i16 = 'n'
 * i32 = 'i'
 * i64 = 'x'
 * u16 = 'q'
 * u32 = 'u'
 * u64 = 't'
 * bytes = 'y'
 * str = 's'
 * path = '@'
 * selector = '%'
 * error = 'e'
 * variant = 'v'
 * whitespace = ? one or more ASCII whitespace characters ?
 *
 * Examples:
 * - `i`: A 32-bit signed integer property
 * - `i->s`: A function-like element that takes a 32-bit signed integer and returns a string
 * - `(%@)`: A property that is a pair of a selector and a path
 * - `[{sv}] -> y`: A function-like element that takes an array of pairs of strings and variants and returns a byte
 array.
 *                  A notable detail is that `[{sv}]` is for all intents and purposes akin to a JSON object.
 */

/**
 * @brief Represents the kind of a typedescr. At the moment, only two kinds are supported (value and operation).
 */
enum dicey_typedescr_kind {
    DICEY_TYPEDESCR_INVALID,

    DICEY_TYPEDESCR_VALUE,      /**< A plain value type, used by properties */
    DICEY_TYPEDESCR_FUNCTIONAL, /**< A functional type, used by operations */
};

/**
 * @brief Represents a parsed typedescr for a function-like type (operation).
 * @note  The input and output types are stored as views of the original string, which must be kept alive.
 */
struct dicey_typedescr_op {
    struct dicey_view input;  /**< The input type (left side). Stored as a view of the original string. */
    struct dicey_view output; /**< The output type (right side). Stored as a view of the original string. */
};

/**
 * @brief Represents a parsed and validated typedescr.
 * @note  The typedescr is stored as a view of the original string, which must be kept alive.
 */
struct dicey_typedescr {
    /**< The kind of item the typedescr specifies for. Used to specify which element of the union is active */
    enum dicey_typedescr_kind kind;

    union {
        const char *value;            /**< A plain value type, used by properties */
        struct dicey_typedescr_op op; /**< A functional type, used by operations */
    };
};

/**
 * @brief Checks if the given view starts with a valid typedescr.
 * @param view The view to inspect. The view must be valid and point to an array of characters. The view will be updated
 *             to point to the first character after the typedescr.
 * @return true if the view starts with a valid typedescr, false otherwise.
 */
DICEY_EXPORT bool dicey_typedescr_in_view(struct dicey_view *view);

/**
 * @brief Checks if the given typedescr is syntactically valid.
 * @note  This function is equivalent to `dicey_typedescr_parse(typedescr, &(struct dicey_typedescr) {0})`.
 * @param typedescr The typedescr to check.
 * @return true if the typedescr is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_typedescr_is_valid(const char *typedescr);

/**
 * @brief Parses a typedescr into a dicey_typedescr struct.
 * @param typedescr The typedescr to parse.
 * @param descr The struct to store the parsed typedescr.
 * @return true if the typedescr was successfully parsed, false otherwise.
 */
DICEY_EXPORT bool dicey_typedescr_parse(const char *typedescr, struct dicey_typedescr *descr);

#endif // WQWGWWXACS_TYPEDESCR_H
