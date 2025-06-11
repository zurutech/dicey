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

#if !defined(DCMRMJXVLH_REQUEST_H)
#define DCMRMJXVLH_REQUEST_H

#include <stdint.h>

#include "../core/builders.h"
#include "../core/errors.h"
#include "../core/message.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents a request from a client to a server. Servers receive requests in their request handlers,
 *        and have to handle them by replying with a response. Failing to handle a request will result in the
 *        request remaining pending until the server is stopped or the client disconnects.
 */
struct dicey_request;

/**
 * @brief Fails a request with an error code and message.
 * @param req The request to fail.
 * @param code The error code.
 * @param msg The error message.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_fail(struct dicey_request *req, uint16_t code, const char *msg);

/**
 * @brief Fails a request with an error code and message and waits for the response to be sent.
 * @param req The request to fail.
 * @param code The error code.
 * @param msg The error message.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_fail_and_wait(struct dicey_request *req, uint16_t code, const char *msg);

/**
 * @brief Gets the client information associated with a request.
 * @param req The request.
 * @return A pointer to the client information, with the same lifetime as the request.
 */
DICEY_EXPORT const struct dicey_client_info *dicey_request_get_client_info(const struct dicey_request *req);

/**
 * @brief Gets the message associated with a request.
 * @param req The request.
 * @return A pointer to the message, with the same lifetime as the request.
 */
DICEY_EXPORT const struct dicey_message *dicey_request_get_message(const struct dicey_request *req);

/**
 * @brief Gets the sequence number of a request.
 * @param req The request.
 * @return The sequence number of the request.
 */
DICEY_EXPORT uint32_t dicey_request_get_seq(const struct dicey_request *req);

/**
 * @brief Replies to a request with a value assembled from the given argument.
 * @param req The request to reply to.
 * @param arg The value to reply with.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_reply(struct dicey_request *req, const struct dicey_arg arg);

/**
 * @brief Replies to a request with a value assembled from the given argument and waits for the response to be sent.
 * @param req The request to reply to.
 * @param arg The value to reply with.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_reply_and_wait(struct dicey_request *req, const struct dicey_arg arg);

/**
 * @brief Replies to a request with a coming from another request.
 * @param req The request to reply to.
 * @param value The value to reply with, which must come from a valid packet.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_reply_with_existing(
    struct dicey_request *req,
    const struct dicey_value *value
);

/**
 * @brief Replies to a request with an existing value and waits for the response to be sent.
 * @param req The request to reply to.
 * @param value The value to reply with.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_reply_with_existing_and_wait(
    struct dicey_request *req,
    const struct dicey_value *value
);

/**
 * @brief Resets the response builder for a request.
 * @param req The request.
 * @param builder The response builder to reset.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_response_reset(
    struct dicey_request *req,
    struct dicey_value_builder *builder
);

/**
 * @brief Sends the response for a request.
 * @param req The request.
 * @param builder The response builder.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_response_send(
    struct dicey_request *req,
    struct dicey_value_builder *builder
);

/**
 * @brief Sends the response for a request and waits for it to be sent.
 * @param req The request.
 * @param builder The response builder.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_response_send_and_wait(
    struct dicey_request *req,
    struct dicey_value_builder *builder
);

/**
 * @brief Starts building the response for a request.
 * @param req The request.
 * @param builder The response builder.
 * @return An error code indicating if the operation was successful.
 */
DICEY_EXPORT enum dicey_error dicey_request_response_start(
    struct dicey_request *req,
    struct dicey_value_builder *builder
);

#if defined(__cplusplus)
}
#endif

#endif // DCMRMJXVLH_REQUEST_H
