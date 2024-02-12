// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(FJWTVTVLMM_BUILDERS_H)
#define FJWTVTVLMM_BUILDERS_H

#include <stdint.h>

#include "dicey_export.h"
#include "packet.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Structure representing a message builder.
 * @note Message builders are not thread-safe and should never be shared between threads.
 */
struct dicey_message_builder {
    // these fields are internal and should not be accessed directly

    int _state;

    enum dicey_op _type;
    uint32_t _seq;
    const char *_path;
    struct dicey_selector _selector;

    struct dicey_arg *_root; // Root argument of the message. Requires freeing by either discard or build

    // While constructing the value, _root is the root of the value, and _borrowed_to is the value builder that is
    // holding the lock over this message builder.
    const struct dicey_value_builder *_borrowed_to;
};

/**
 * @brief Initializes a message builder.
 * @note All message builders must be initialized before use using this function. Any current state is discarded without
 *       being freed, so make sure to discard or finalise a message builder before re-initializing it.
 *       Message builders are designed to be reused, so you should not init and destroy them for each message.
 * @param builder Message builder to initialize.
 * @return Error code. Possible errors are:
 *         - OK: (Never fails)
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_init(struct dicey_message_builder *builder);

/**
 * @brief Begins building a message.
 * @note This function must be called only on an initialised builder, and will fail if the builder is already in use.
 * @param builder Message builder (must be initialised and idle).
 * @param op The operation represented by the message under construction.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not idle
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_begin(struct dicey_message_builder *builder, enum dicey_op op);

/**
 * @brief Builds a message into a packet.
 * @note This function completely discards the builder contents, leaving it ready to be reused.
 * @param builder Message builder (must be initialised and fully constructed).
 * @param packet The packet to build the message into. Must point to valid memory, which will be overwritten.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EAGAIN: The builder is not fully constructed yet
 *         - EINVAL: The builder is not in the correct state or contains garbage
 *         - EPATH_TOO_LONG: The path exceeds the maximum length
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_build(
    struct dicey_message_builder *builder,
    struct dicey_packet *packet
);

/**
 * @brief Destroys a message builder, discarding any current state.
 * @param builder Message builder to destroy.
 * @return Error code. Possible errors are:
 *          - OK: (Never fails)
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_destroy(struct dicey_message_builder *builder);

/**
 * @brief Discards the current state of a message builder.
 * @note This function completely discards the builder contents, leaving it ready to be reused.
 * @param builder Message builder to wipe.
 */
DICEY_EXPORT void dicey_message_builder_discard(struct dicey_message_builder *builder);

/**
 * @brief Sets the path of a message builder.
 * @note This function does not copy the received string, so the string must remain valid until the message is built.
 * @param builder Message builder. Must be initialised and in the correct state.
 * @param path The path to set. Must be a valid null-terminated string, with a lifetime at least as long as the builder.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_set_path(struct dicey_message_builder *builder, const char *path);

/**
 * @brief Sets the selector of a message builder.
 * @note This function does not copy the received selector, so its strings must remain valid until the message is built.
 * @param builder Message builder. Must be initialised and in the correct state.
 * @param selector The selector to set. Must be a valid selector, with a lifetime at least as long as the builder.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_set_selector(
    struct dicey_message_builder *builder,
    struct dicey_selector selector
);

/**
 * @brief Sets the sequence number of a message builder.
 * @param builder Message builder. Must be initialised and in the correct state.
 * @param seq The sequence number to set.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *builder, uint32_t seq);

/**
 * @brief Structure representing an argument, passed to the builder to construct a value.
 *        `dicey_arg` is constructed as a tree, with the "base" types being leaves and the "complex" types being nodes
 *         with children. The tree can either be constructed by the builder or passed to it by the user.
 *         In the latter case, the builder deep copies the tree, so the user is not required to keep the tree alive.
 */
struct dicey_arg {
    enum dicey_type type; /**< Type of the argument. Must match the active member of the union below. */

    union {
        dicey_bool boolean;   /**< Boolean value. */
        dicey_byte byte;      /**< Byte value. */
        dicey_float floating; /**< Floating-point value. */
        dicey_i16 i16;        /**< 16-bit signed integer value. */
        dicey_i32 i32;        /**< 32-bit signed integer value. */
        dicey_i64 i64;        /**< 64-bit signed integer value. */
        dicey_u16 u16;        /**< 16-bit unsigned integer value. */
        dicey_u32 u32;        /**< 32-bit unsigned integer value. */
        dicey_u64 u64;        /**< 64-bit unsigned integer value. */

        struct dicey_array_arg {
            enum dicey_type type;          /**< Type of the array elements. */
            uint16_t nitems;               /**< Number of items in the array. */
            const struct dicey_arg *elems; /**< Children elements, which are required to be of type `type` */
        } array;                           /**< Array value. */

        struct dicey_tuple_arg {
            uint16_t nitems;               /**< Number of items in the tuple. */
            const struct dicey_arg *elems; /**< Children elements, which are represented as variants */
        } tuple;                           /**< Tuple value. */

        struct dicey_pair_arg {
            const struct dicey_arg *first;  /**< First element of the pair. */
            const struct dicey_arg *second; /**< Second element of the pair. */
        } pair;                             /**< Pair value. Both values are represented as variants. */

        struct dicey_bytes_arg {
            uint32_t len;        /**< Length of the byte array. */
            const uint8_t *data; /**< Byte array data. Note: must be alive for the entire lifetime of the argument */
        } bytes;                 /**< Byte array value. */

        /**< String value. Note: strings are never copied, and must be alive for the entire lifetime of the argument */
        const char *str;

        /**<
         * Selector value. Note: selectors are never copied, and must be alive for the entire lifetime of the argument
         */
        struct dicey_selector selector;

        struct dicey_error_arg {
            uint16_t code;       /**< Error code (any value is fine, this is not defined yet)*/
            const char *message; /**< Error message. Note: must be alive for the entire lifetime of the argument */
        } error;                 /**< Error value. */
    };
};

/**
 * @brief Sets the value of a message builder to a given argument.
 * @note This function deep copies the argument, so the user is not required to keep `value` alive.
 *       However, if the argument contains buffers, strings or selectors, the user is required to keep them alive for
 *       the entire lifetime of the message builder.
 *       This function is equivalent to calling `dicey_message_builder_value_start`, `dicey_value_builder_set` and
 *       `dicey_message_builder_value_end` in sequence.
 * @param builder Message builder. Must be initialised and in the correct state.
 * @param value The value to set. Must be a valid argument.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EBUILDER_TYPE_MISMATCH: when an argument does not match an array type
 *         - EINVAL: The builder is not in the correct state (or contains garbage) or the argument is invalid
 *         - ENOMEM: The builder is unable to allocate memory for the argument
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_set_value(
    struct dicey_message_builder *builder,
    struct dicey_arg value
);

/**
 * @brief Structure representing a value builder. Each value builder sets a single value, and may spawn other value
 *        builders to build its children.
 * @note Value builders are only valid while the message builder they belong to is being constructed, and can't be used
 *       standalone. Value builders are not thread-safe and should never be shared between threads.
 */
struct dicey_value_builder {
    // these fields are part of the internal interface of the value builder. Do not tamper with them

    int _state;

    // root of the built message, i.e. a leaf if the value is not a compound type, or a node otherwise.
    // this value is borrowed from the message builder, and must not be freed by the value builder
    struct dicey_arg *_root;

    // specialised builder structure for subvalues (used by arrays and tuples)
    struct _dicey_value_builder_list {
        enum dicey_type type; // type of the elements. Only valid if the value is an array

        // dynamic array of elements. Only valid if the value is an array or a tuple
        uint16_t nitems;
        size_t cap;
        struct dicey_arg *elems;
    } _list;
};

/**
 * @brief Starts building a value. This locks the builder, which enters in a "borrowed" state until value_end is called.
 * @note  Any previously set value will be freed.
 * @param builder Message builder to target. Must be initialised and in the correct state.
 * @param value The value builder, whose contents will be overwritten. Can be in any state.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 *         - ENOMEM: The builder is unable to allocate memory for the value
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_value_start(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

/**
 * @brief Ends building a value.
 * @note  This unlocks the message builder, which returns to an "idle" state. The value builder must be the same as the
 *        one passed to value_start, and may be in any state (including garbage).
 *        Calling this function on a value builder that is not "closed" (i.e. set) is UB and may lead to invalid values.
 * @param builder Message builder. Must be initialised and in a locked state.
 * @param value The value builder. Must be the same may be zeroed)
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder or the value builder are not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_message_builder_value_end(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

/**
 * @brief Starts building an array of the given type.
 * @note  This function locks the value builder, which enters in an "array" state until array_end is called.
 * @param builder The value builder. Must be ready for writing and empty.
 * @param type The type all array elements will have.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_array_start(
    struct dicey_value_builder *builder,
    enum dicey_type type
);

/**
 * @brief Ends building an array value.
 * @note  This function "completes" the value builder. The value builder can then be reused to build another value, but
 *        the array cannot be modified anymore. If the constructed node is the root of a value, this function does not
 *        unlock the message builder. Call value_end to unlock it.
 * @param builder The value builder. Must be in an "array" state.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the array state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *builder);

/**
 * @brief Moves to the next element in a value builder locked on a "list" state (i.e. array or tuple).
 * @param list The value builder of the list. Must be in a "list" state.
 * @param elem An empty value builder for the next element. If the list is an array, it will be pre-filled with the
 *             array type.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: when `list` is not in a "list" state
 *         - EOVERFLOW: when trying to add a third element to a pair
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_next(
    struct dicey_value_builder *list,
    struct dicey_value_builder *elem
);

/**
 * @brief Starts building a pair value.
 * @note This function locks the value builder, which enters in a "pair" state until pair_end is called.
 * @param builder The value builder. Must be ready for writing and empty.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_pair_start(struct dicey_value_builder *builder);

/**
 * @brief Ends building a pair value.
 * @note This function "completes" the value builder. The value builder can then be reused to build another value, but
 *       the pair cannot be modified anymore. If the constructed node is the root of a value, this function does not
 *       unlock the message builder. Call value_end to unlock it.
 * @param builder The value builder. Must be in a "pair" state.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EAGAIN: The pair is not fully constructed yet
 *         - EINVAL: The builder is not in the pair state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_pair_end(struct dicey_value_builder *builder);

/**
 * @brief Sets the value of a value builder.
 * @note This function recursively copies the argument, so the user is not required to keep `value` alive. Any
 *       previously set values will be discarded. Buffers, strings and selectors must be kept alive for the entire
 *       lifetime of the value builder.
 * @param builder The value builder. Must be ready for writing and empty.
 * @param value The value to set. Must be a valid argument. Any previously set value will be discarded.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EBUILDER_TYPE_MISMATCH: when an argument does not match the pre-set type of this builder (if any)
 *         - EINVAL: The builder is not in the correct state
 *         - ENOMEM: The builder is unable to allocate memory for the argument
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_set(struct dicey_value_builder *builder, struct dicey_arg value);

/**
 * @brief Starts building a tuple value.
 * @note This function locks the value builder, which enters in a "tuple" state until tuple_end is called.
 * @param builder The value builder. Must be ready for writing and empty.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the correct state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *builder);

/**
 * @brief Ends building a tuple value.
 * @note This function "completes" the value builder. The value builder can then be reused to build another value, but
 *       the tuple cannot be modified anymore. If the constructed node is the root of a value, this function does not
 *       unlock the message builder. Call value_end to unlock it.
 * @param builder The value builder. Must be in a "tuple" state.
 * @return Error code. Possible errors are:
 *         - OK: The operation was successful
 *         - EINVAL: The builder is not in the tuple state
 */
DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *builder);

#if defined(__cplusplus)
}
#endif

#endif // FJWTVTVLMM_BUILDERS_H
