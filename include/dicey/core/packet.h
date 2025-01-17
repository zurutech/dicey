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

#if !defined(XYDQQUJZAI_PACKET_H)
#define XYDQQUJZAI_PACKET_H

#include <stddef.h>
#include <stdint.h>

#include "errors.h"
#include "version.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents all the disconnession reasons that can be sent in in a "bye" packet.
 */
enum dicey_bye_reason {
    DICEY_BYE_REASON_INVALID = 0, /**< Invalid or no reason */

    DICEY_BYE_REASON_SHUTDOWN = 1, /**< Client or server are shutting down cleanly */
    DICEY_BYE_REASON_ERROR = 2,    /**< A serious error has happened. The client must immediately disconnect */
    DICEY_BYE_REASON_KICKED = 3,   /**< The client has been kicked by the server */
};

/**
 * @brief Checks if the given bye reason is valid.
 * @param reason The bye reason to be validated.
 * @return true if the bye reason is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_bye_reason_is_valid(enum dicey_bye_reason reason);

/**
 * @brief Converts a bye reason to a fixed string representation.
 * @param reason A bye reason.
 * @return The string representation of the given bye reason.
 */
DICEY_EXPORT const char *dicey_bye_reason_to_string(enum dicey_bye_reason reason);

/**
 * @brief Enumeration of possible packet kinds.
 */
enum dicey_packet_kind {
    DICEY_PACKET_KIND_INVALID = 0, /**< Invalid packet (never sent on wire) */

    /**< HELLO: first packet, sent by client and then server. Used for version handshake */
    DICEY_PACKET_KIND_HELLO,

    /**< BYE: last packet, sent when disconnecting cleanly. Clients will be immediately disconnected after receiving it
     */
    DICEY_PACKET_KIND_BYE,

    /**< MESSAGE: variable payload which represent the entirety of all meaningful client-server interactions */
    DICEY_PACKET_KIND_MESSAGE
};

/**
 * @brief Checks if a given packet kind is valid.
 * @param kind The packet kind.
 * @return True if the packet kind is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_packet_kind_is_valid(enum dicey_packet_kind kind);

/**
 * @brief Converts a packet kind to a fixed string representation.
 * @param kind A packet kind.
 * @return The string representation of the given packet kind.
 */
DICEY_EXPORT const char *dicey_packet_kind_to_string(enum dicey_packet_kind kind);

/**
 * @brief Structure representing the reason in a "bye" packet.
 */
struct dicey_bye {
    enum dicey_bye_reason reason; /**< Bye reason */
};

/**
 * @brief Structure representing the version information in a "hello" packet.
 */
struct dicey_hello {
    struct dicey_version version; /**< Version information */
};

/**
 * @brief Structure representing a packet.
 */
struct dicey_packet {
    void *payload; /**< Raw payload, castable to uint8_t* and ready to be sent on the wire */
    size_t nbytes; /**< Number of bytes allocated in payload*/
};

/**
 * @brief Structure representing a message in a packet.
 */
struct dicey_message;

/**
 * @brief Quick macro to initialize an empty packet.
 */
#define DICEY_EMPTY_PACKET ((struct dicey_packet) { .payload = NULL, .nbytes = 0 })

/**
 * @brief Loads a packet from data pointer at `data`.
 * @note  This function will advance both `data` and `nbytes` by the number of bytes read.
 * @note  This function will allocate memory for the packet payload. The caller is responsible for calling
 *        `dicey_packet_deinit` to free the payload.
 * @note  This function also validates the correctness of the packet data.
 * @param packet The packet to load.
 * @param data The data to load the packet from. This pointer will be advanced by the number of bytes read.
 * @param nbytes The number of bytes in the data. This value will be decremented by the number of bytes read.
 * @return The error code indicating the success or failure of the operation.
 *         Possible errors are:
 *         - OK: the packet was successfully loaded
 *         - EAGAIN: the buffer does not contain enough data to load a packet
 *         - EBADMSG: the packet is not valid, malformed or contains invalid data
 *         - EINVAL: the arguments are not valid (i.e. something is NULL, ...)
 *         - ENOMEM: the packet could not be loaded because of insufficient memory
 *         - EOVERFLOW: the packet is too large to be loaded or some of its fields are too large
 */
DICEY_EXPORT enum dicey_error dicey_packet_load(struct dicey_packet *packet, const void **data, size_t *nbytes);

/**
 * @brief Attempts extracting a "bye" packet from a packet.
 * @param packet The packet to convert.
 * @param bye The extracted "bye" packet, if any
 * @return The error code indicating the success or failure of the operation.
 *         Possible errors are:
 *         - OK: the packet was successfully converted to a "bye" packet
 *         - EINVAL: the packet is not a "bye" packet
 */
DICEY_EXPORT enum dicey_error dicey_packet_as_bye(struct dicey_packet packet, struct dicey_bye *bye);

/**
 * @brief Converts a packet to a "hello" packet.
 * @param packet The packet to convert.
 * @param hello The extracted "hello" packet, if any
 * @return The error code indicating the success or failure of the operation.
 *         Possible errors are:
 *         - OK: the packet was successfully converted to a "hello" packet
 *         - EINVAL: the packet is not a "hello" packet
 */
DICEY_EXPORT enum dicey_error dicey_packet_as_hello(struct dicey_packet packet, struct dicey_hello *hello);

/**
 * @brief Converts a packet to a message packet. The packet is assumed as correctly validated, and the data is lazily
 *        extracted from the binary payload.
 * @param packet The packet to convert.
 * @param message The converted message packet, if any
 * @return The error code indicating the success or failure of the operation.
 *         Possible errors are:
 *         - OK: the packet was successfully converted to a message packet
 *         - EINVAL: the packet is not a message packet
 */
DICEY_EXPORT enum dicey_error dicey_packet_as_message(struct dicey_packet packet, struct dicey_message *message);

/**
 * @brief Deinitializes a packet, freeing its contents.
 * @param packet The packet to deinitialize.
 */
DICEY_EXPORT void dicey_packet_deinit(struct dicey_packet *packet);

/**
 * @brief Dumps the contents of a packet into the given data buffer.
 * @param packet The packet to dump.
 * @param data The buffer to dump the packet into. This pointer will be reinterpreted as `uint8_t*` and then advanced on
 *             success by the number of bytes written.
 * @param nbytes The number of bytes in the buffer. This value will be decremented by the number of bytes written.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the packet was successfully written to the buffer
 *         - EINVAL: the packet or the buffer are invalid
 *         - EOVERFLOW: the buffer does not contain enough space to write the packet
 */
DICEY_EXPORT enum dicey_error dicey_packet_dump(struct dicey_packet packet, void **data, size_t *nbytes);

/**
 * @brief Gets the kind of a packet.
 * @param packet The packet.
 * @return The packet kind.
 */
DICEY_EXPORT enum dicey_packet_kind dicey_packet_get_kind(struct dicey_packet packet);

/**
 * @brief Gets the sequence number of a packet.
 * @param packet The packet.
 * @param seq The sequence number.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the sequence number was successfully retrieved
 *         - EINVAL: the packet is invalid
 */
DICEY_EXPORT enum dicey_error dicey_packet_get_seq(struct dicey_packet packet, uint32_t *seq);

/**
 * @brief Sets the sequence number of a packet. This is rarely useful, as the sequence number is usually set by the
 *        server or client before sending a packet.
 * @param packet The packet.
 * @param seq The sequence number.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the sequence number was successfully set
 *         - EINVAL: the packet is invalid
 */
DICEY_EXPORT enum dicey_error dicey_packet_set_seq(struct dicey_packet packet, uint32_t seq);

/**
 * @brief Checks if a packet is valid.
 * @param packet The packet.
 * @return True if the packet is valid, false otherwise.
 */
DICEY_EXPORT bool dicey_packet_is_valid(struct dicey_packet packet);

/**
 * @brief Creates a "bye" packet.
 * @param dest The destination packet.
 * @param seq The sequence number.
 * @param reason The bye reason.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the packet was successfully created
 *         - EINVAL: the packet or the reason are invalid
 *         - ENOMEM: the packet could not be created because of insufficient memory
 */
DICEY_EXPORT enum dicey_error dicey_packet_bye(struct dicey_packet *dest, uint32_t seq, enum dicey_bye_reason reason);

/**
 * @brief Creates a "hello" packet.
 * @param dest The destination packet.
 * @param seq The sequence number.
 * @param version The embedded version number.
 * @return The error code indicating the success or failure of the operation. Possible errors are:
 *         - OK: the packet was successfully created
 *         - EINVAL: the packet or the version are invalid
 *         - ENOMEM: the packet could not be created because of insufficient memory
 */
DICEY_EXPORT enum dicey_error dicey_packet_hello(struct dicey_packet *dest, uint32_t seq, struct dicey_version version);

#ifdef __cplusplus
}
#endif

#endif // XYDQQUJZAI_PACKET_H
