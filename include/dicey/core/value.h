// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(TOJAFCVDUG_VALUE_H)
#define TOJAFCVDUG_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dicey_export.h"
#include "errors.h"
#include "type.h"

#include "data-info.h"
#include "views.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief A variant value, representing a value extracted by a dicey message.
 */
struct dicey_value {
    // internal data - do not touch
    enum dicey_type _type;
    union _dicey_data_info _data;
};

/**
 * @brief The dicey_iterator struct represents an iterator over an array, or tuple, of Dicey values.
 */
struct dicey_iterator {
    uint16_t _type;
    struct dicey_view _data; // Borrowed view over a sequence of dicey_value
};

/**
 * @brief Checks if the iterator has more elements to iterate over.
 * @param iter The iterator to check.
 * @return true if the iterator has more elements, false otherwise.
 */
DICEY_EXPORT bool dicey_iterator_has_next(struct dicey_iterator iter);

/**
 * @brief Advances the iterator to the next element and stores the value in the destination.
 * @note  The value returned is borrowed and tied to the lifetime of the list that created the iterator.
 * @param iter The iterator to advance. Must be valid and have more elements to iterate over.
 * @param dest The destination to store the next value.
 * @return The error code indicating the success or failure of the operation.
 *         Possible errors include:
 *         - OK: The operation was successful.
 *         - ENODATA: The iterator has no more elements to iterate over.
 */
DICEY_EXPORT enum dicey_error dicey_iterator_next(struct dicey_iterator *iter, struct dicey_value *dest);

/**
 * @brief A view over a list of dicey values. May be backed by a tuple or an array.
 */
struct dicey_list {
    // internal data
    uint16_t _type;          // The array type or VARIANT_ID (for a tuple)
    uint16_t _nitems;        // The number of items in the list.
    struct dicey_view _data; // Borrowed data associated with this list
};

/**
 * @brief Creates an iterator for iterating over the given list.
 * @param list The list to create an iterator for. Must be valid.
 * @return The iterator for the given list.
 */
DICEY_EXPORT struct dicey_iterator dicey_list_iter(const struct dicey_list *list);

/**
 * @brief Gets the type of the list.
 * @param list The list to get the type of. Must be valid.
 * @return Either:
 *        - A valid dicey_type if the list is backed by an array
 *        - DICEY_VARIANT_ID if the list is backed by a tuple
 */
DICEY_EXPORT int dicey_list_type(const struct dicey_list *list);

/**
 * @brief A pair of Dicey values.
 */
struct dicey_pair {
    struct dicey_value first;
    struct dicey_value second;
};

/**
 * @brief Verifies if the given value can be returned by an operation or property having the given signature.
 * @param value The value to check.
 * @param sigstr The signature string of the operation or property whose return type is to be checked against the given
 * value.
 * @return true if the operation described by the signature can return the given value, false otherwise.
 */
DICEY_EXPORT bool dicey_value_can_be_returned_from(const struct dicey_value *value, const char *sigstr);

/**
 * @brief Gets the Dicey type of the given value.
 * @param value The value to get the type of.
 * @return The type of the value (INVALID if the value is NULL).
 */
DICEY_EXPORT enum dicey_type dicey_value_get_type(const struct dicey_value *value);

/**
 * @brief Gets the array value from the given value as a list.
 * @param value The value to get the array from.
 * @param dest The list which will point to the internal array store.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not an array.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_array(const struct dicey_value *value, struct dicey_list *dest);

/**
 * @brief Gets the boolean value from the given value.
 * @param value The value to get the boolean from.
 * @param dest The destination to store the boolean value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a boolean.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_bool(const struct dicey_value *value, bool *dest);

/**
 * @brief Gets the byte value from the given value.
 * @param value The value to get the byte from.
 * @param dest The destination to store the byte value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a byte.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_byte(const struct dicey_value *value, uint8_t *dest);

/**
 * @brief Gets the bytes value from the given value.
 * @param value The value to get the bytes from.
 * @param dest The destination to store the bytes pointer. Note that this is a borrowed pointer and should not be freed
 *             by the caller
 * @param nbytes The number of bytes in the bytes value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a bytes value.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_bytes(
    const struct dicey_value *value,
    const uint8_t **dest,
    size_t *nbytes
);

/**
 * @brief Gets the error value from the given value.
 * @param value The value to get the error from.
 * @param dest The destination to store the error value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not an error.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_error(const struct dicey_value *value, struct dicey_errmsg *dest);

/**
 * @brief Gets the float value from the given value.
 * @param value The value to get the float from.
 * @param dest The destination to store the float value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a float.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_float(const struct dicey_value *value, double *dest);

/**
 * @brief Gets the int16 value from the given value.
 * @param value The value to get the int16 from.
 * @param dest The destination to store the int16 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not an int16.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_i16(const struct dicey_value *value, int16_t *dest);

/**
 * @brief Gets the int32 value from the given value.
 * @param value The value to get the int32 from.
 * @param dest The destination to store the int32 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not an int32.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_i32(const struct dicey_value *value, int32_t *dest);

/**
 * @brief Gets the int64 value from the given value.
 * @param value The value to get the int64 from.
 * @param dest The destination to store the int64 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not an int64.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_i64(const struct dicey_value *value, int64_t *dest);

/**
 * @brief Gets the pair value from the given value.
 * @param value The value to get the pair from.
 * @param dest The destination to store the pair value.. Note that this is a borrowed pointer and should
 *             not be freed by the caller.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a pair.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_pair(const struct dicey_value *value, struct dicey_pair *dest);

/**
 * @brief Gets the path value from the given value.
 * @param value The value to get the path from.
 * @param dest The destination to store the path value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a path.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_path(const struct dicey_value *value, const char **dest);

/**
 * @brief Gets the selector value from the given value.
 * @param value The value to get the selector from.
 * @param dest The destination to store the selector value. Note that this is comprised of borrowed pointers which
 *             should not be freed by the caller.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a selector.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_selector(const struct dicey_value *value, struct dicey_selector *dest);

/**
 * @brief Gets the string value from the given value.
 * @param value The value to get the string from.
 * @param dest The destination to store a pointer to the string value. Note that this is a borrowed pointer and should
 *             not be freed by the caller.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a string.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_str(const struct dicey_value *value, const char **dest);

/**
 * @brief Gets the tuple value from the given value.
 * @param value The value to get the tuple from.
 * @param dest The destination to store the tuple value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a tuple.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_tuple(const struct dicey_value *value, struct dicey_list *dest);

/**
 * @brief Gets the uint16 value from the given value.
 * @param value The value to get the uint16 from.
 * @param dest The destination to store the uint16 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a uint16.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_u16(const struct dicey_value *value, uint16_t *dest);

/**
 * @brief Gets the uint32 value from the given value.
 * @param value The value to get the uint32 from.
 * @param dest The destination to store the uint32 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a uint32.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_u32(const struct dicey_value *value, uint32_t *dest);

/**
 * @brief Gets the uint64 value from the given value.
 * @param value The value to get the uint64 from.
 * @param dest The destination to store the uint64 value.
 * @return The error code indicating the success or failure of the operation.
 *        Possible errors include:
 *        - OK: The operation was successful.
 *        - EVALUE_TYPE_MISMATCH: The value is not a uint64.
 */
DICEY_EXPORT enum dicey_error dicey_value_get_u64(const struct dicey_value *value, uint64_t *dest);

/**
 * @brief Checks if the value is of the specified type.
 * @param value The value to check.
 * @param type The type to check against.
 * @return true if the value is of the specified type, false otherwise.
 */
DICEY_EXPORT bool dicey_value_is(const struct dicey_value *value, enum dicey_type type);

/**
 * @brief Verifies if the given value is compatible with the given signature.
 * @note  If the signature represents an operation, this function verifies that the given value can be used as an
 *        argument to an operation with the given signature.
 * @param value The value to check.
 * @param sigstr The signature string to check the value against
 * @return true if the value is compatible with the given signature, false otherwise.
 */
DICEY_EXPORT bool dicey_value_is_compatible_with(const struct dicey_value *value, const char *sigstr);

/**
 * @brief Returns true if the value is unit. Equivalent to dicey_value_is(value, DICEY_UNIT).
 * @param value The value to check.
 * @return true if the value is of the specified type, false otherwise.
 */
DICEY_EXPORT bool dicey_value_is_unit(const struct dicey_value *value);

/**
 * @brief Checks if the value is valid. Currently, this means that the value is not NULL and has a valid type.
 * @param value The value to check.
 * @return true if the value is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_value_is_valid(const struct dicey_value *value);

#ifdef __cplusplus
}
#endif

#endif // TOJAFCVDUG_VALUE_H
