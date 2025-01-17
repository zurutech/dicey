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

#if !defined(SQKZIWSQVR_MESSAGE_H)
#define SQKZIWSQVR_MESSAGE_H

#include "packet.h"
#include "value.h"

#include "dicey_export.h"

/**
 * @brief All the possible operations that can be performed or have been performed on a given (path, selector).
 */
enum dicey_op {
    DICEY_OP_INVALID = 0, /**< Invalid operation. Always a fatal error */

    /**<
     * Get: instructs the server to generate a response containing the value of a property at a given (path, selector)
     */
    DICEY_OP_GET = '<',

    /**<
     * Set: instructs the server to set the value of a property at a given (path, selector) to a given value
     */
    DICEY_OP_SET = '>',

    /**<
     * Exec: instructs the server to execute an operation identified by the given (path, selector) with a given argument
     */
    DICEY_OP_EXEC = '?',

    /**<
     * Signal: raised when an event has happened on a given (path, selector) with a given value. Always server-initiated
     */
    DICEY_OP_SIGNAL = '!',

    /**<
     * Response: response to a previous {GET, SET, EXEC} operation. Always server-initiated
     */
    DICEY_OP_RESPONSE = ':',
};

/**
 * @brief Checks if a given value represents a valid operation code.
 * @param type The operation type.
 * @return True if the operation is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_op_is_valid(enum dicey_op type);

/**
 * @brief Checks if a given operation requires a payload.
 * @param kind The operation type.
 * @return True if the operation requires a payload, false otherwise.
 */
DICEY_EXPORT bool dicey_op_requires_payload(enum dicey_op kind);

/**
 * @brief Converts an operation to a fixed string representation.
 * @param type An operation type.
 * @return A static string representation of the operation.
 */
DICEY_EXPORT const char *dicey_op_to_string(enum dicey_op type);

/**
 * @brief Structure representing a message in a packet.
 */
struct dicey_message {
    enum dicey_op type;             /**< Operation type */
    const char *path;               /**< Path to operate on or that originated an event/response */
    struct dicey_selector selector; /**< Selector for the (trait:element) located at path target of this message */
    struct dicey_value value;       /**< Value either returned or to be submitted to the server*/
};

/**
 * @brief Changes the message header of a packet of type MESSAGE.
 * @note  This function is usually used to forward a packet as it is, keeping the value unchanged.
 * @param dest The destination packet.
 * @param old The packet to change. The old packet contents will not be freed.
 * @param seq The sequence number to set in the packet.
 * @param type The operation type to set in the packet.
 * @param path The path to set in the packet.
 * @param selector The selector to set in the packet.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the packet was successfully rewritten
 *         - EINVAL: the arguments are not valid (i.e. something is NULL, ...)
 *         - ENOMEM: the packet could not be rewritten because of insufficient memory
 *         - EBADMSG: the packet is not a message packet or is corrupted
 *         - EPATH_MALFORMED: the path is not a valid path
 *         - EPATH_TOO_LONG: the path is too long to be stored in the packet
 */
DICEY_EXPORT enum dicey_error dicey_packet_forward_message(
    struct dicey_packet *dest,
    struct dicey_packet old,
    uint32_t seq,
    enum dicey_op type,
    const char *path,
    struct dicey_selector selector
);

#endif // SQKZIWSQVR_MESSAGE_H
