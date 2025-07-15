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

#if !defined(RGSZHBDASZ_CLIENT_H)
#define RGSZHBDASZ_CLIENT_H

#include "dicey_export.h"

#include "../core/builders.h"
#include "../core/errors.h"
#include "../core/packet.h"

#include "address.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents all the possible client events
 */
enum dicey_client_event_type {
    DICEY_CLIENT_EVENT_CONNECT,         /**< Raised whenever `connect()` succeeds - i.e., if the server is up */
    DICEY_CLIENT_EVENT_ERROR,           /**< Raised whenever an error occurs, which always causes the client to drop */
    DICEY_CLIENT_EVENT_HANDSHAKE_START, /**< Raised when the client starts the handshake by sending the Hello packet */
    DICEY_CLIENT_EVENT_INIT,            /**< Raised when the client is initialized (before any connect takes place) */
    DICEY_CLIENT_EVENT_MESSAGE_RECEIVING, /**< Raised when the client is receiving a message. Can be used to intercept
                                             inbound packets */
    DICEY_CLIENT_EVENT_MESSAGE_SENDING,   /**< Raised when the client is sending a message. Can be used to intercept
                                             outbound packets */
    DICEY_CLIENT_EVENT_SERVER_BYE, /**< Raised when the client receives a Bye packet from the server, before quitting */
    DICEY_CLIENT_EVENT_QUITTING,   /**< Raised when the client is about to quit */
    DICEY_CLIENT_EVENT_QUIT,       /**< Raised when the client quits */
};

/**
 * @brief Represents a client event, as received by `dicey_client_inspect_fn` handlers
 */
struct dicey_client_event {
    enum dicey_client_event_type type; /**< The type of this event, as described by `dicey_client_event_type` */

    union {
        struct {
            enum dicey_error err;
            char *msg;
        } error;                      /**< The value of the error in case type is ERROR */
        struct dicey_packet packet;   /**< The packet in case type is MESSAGE_RECEIVING or MESSAGE_SENDING */
        struct dicey_version version; /**< The version in case type is HANDSHAKE_START */
    };
};

/**
 * @brief Represents an asynchronous IPC client that can connect to a server and send/receive packets and events.
 * @note  This is an opaque structure, and its internals are not meant to be accessed directly.
 */
struct dicey_client;

/**
 * @brief Represents the result of a subscription operation on a client.
 * @note  This structure must be initialised with `dicey_client_subscribe_result_deinit()` after use to free any
 * resources it may hold.
 */
struct dicey_client_subscribe_result {
    enum dicey_error
        err; /**< The error code of the subscription operation. If the operation was successful, this is `DICEY_OK`. */

    /**
     *  If the subscription targeted an alias, this will be the real path that the client will receive signals from.
     *  This string is heap allocated and must be freed if taken ownership of. Users can "steal" this string by
     *  setting `real_path` to NULL, which will prevent `dicey_client_subscribe_result_deinit()` from freeing it.
     */
    const char *real_path;
};

/**
 * @brief Deinitialises a `dicey_client_subscribe_result` structure.
 * @param result The result to deinitialise.
 */
DICEY_EXPORT void dicey_client_subscribe_result_deinit(struct dicey_client_subscribe_result *result);

/**
 * @brief Represents a callback function that is called when a client connects to a server.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_connect_async()`).
 * @param status The status of the connection - either `DICEY_OK` or an error describing why the connection failed.
 * @param msg    If error is not OK, an error message describing why the connection failed.
 */
typedef void dicey_client_on_connect_fn(
    struct dicey_client *client,
    void *ctx,
    enum dicey_error status,
    const char *msg
);

/**
 * @brief Represents a callback function that is called whenever a client disconnects from a server.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_disconnect_async()`).
 * @param status The status of the disconnection - either `DICEY_OK` or an error describing why the disconnection
 * failed.
 */
typedef void dicey_client_on_disconnect_fn(struct dicey_client *client, void *ctx, enum dicey_error status);

/**
 * @brief Represents a callback function that is called whenever a client checks if a path is an alias.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_is_alias_async()`).
 * @param status The status of the operation - either `DICEY_OK` or an error describing why the operation failed.
 * @param is_alias Whether the path is an alias or not.
 */
typedef void dicey_client_on_is_alias_fn(
    struct dicey_client *client,
    void *ctx,
    enum dicey_error status,
    bool is_alias
);

/**
 * @brief Represents a callback function that is called whenever a client receives a reply to a previously sent request.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_request_async()`).
 * @param status The status of the requerst - either `DICEY_OK` or an error describing why the request failed.
 * @param packet The packet that was received as a reply to the request, or NULL if the request failed.
 *               A reply function is free to steal the packet it receives, taking ownership of it. This can be done by
 *               resetting the `dicey_packet` struct pointed to by `packet` to an empty state.
 */
typedef void dicey_client_on_reply_fn(
    struct dicey_client *client,
    void *ctx,
    enum dicey_error status,
    struct dicey_packet *packet
);

/**
 * @brief Represents a callback function that is called whenever a client finishes subscribing or unsubscribing to a
 *        signal.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_subscribe_to_async()` or
 *               `dicey_client_unsubscribe_from_async()`).
 * @param result The result of the subscription or unsubscription. If the operation was successful, `err` is `DICEY_OK`.
 *               If `real_path` is not NULL, it means the path that the client specified was an alias pointing to
 *               an object registered under `real_path`. All signals will be received on `real_path` instead of the
 *               original path.
 */
typedef void dicey_client_on_sub_done_fn(
    struct dicey_client *client,
    void *ctx,
    struct dicey_client_subscribe_result result
);

/**
 * @brief Represents a callback function that is called whenever a client finishes subscribing or unsubscribing to a
 *        signal.
 * @param client The client this callback is associated with.
 * @param ctx    The context associated to this function (usually passed through `dicey_client_subscribe_to_async()` or
 *               `dicey_client_unsubscribe_from_async()`).
 * @param status The status of the subscription/unsubscription - either `DICEY_OK` or an error describing why the
 *               operation failed.
 */
typedef void dicey_client_on_unsub_done_fn(struct dicey_client *client, void *ctx, enum dicey_error status);

/**
 * @brief Represents a callback function that is called whenever a client receives a Dicey Message containing a signal.
 * @param client  The client this callback is associated with.
 * @param ctx     The global context of `client`, as obtained via `dicey_client_get_context()`. This is provided for
 * convenience.
 * @param packet  The event packet that was received. This is guaranteed to be a valid message containing an event.
 *                The event handler can take ownership of the packet by zeroing it out. This will prevent the client
 *                from freeing it.
 */
typedef void dicey_client_signal_fn(struct dicey_client *client, void *ctx, struct dicey_packet *packet);

/**
 * @brief Represents a callback function that is called whenever anything happens in the client.
 *        Useful to inspect the client lifecycle and the packets it sends and receives.
 * @param client The client this callback is associated with.
 * @param ctx    The global context of `client`, as obtained via `dicey_client_get_context()`. This is provided for
 * convenience.
 * @param event  The event that was raised. This is guaranteed to be a valid event.
 */
typedef void dicey_client_inspect_fn(struct dicey_client *client, void *ctx, struct dicey_client_event event);

/**
 * @brief Represents the initialisation arguments that can be passed to `dicey_client_new()`.
 */
struct dicey_client_args {
    /** The function that will be called whenever an event happens in the client. */
    dicey_client_inspect_fn *inspect_func;

    /** The function that will be called whenever the client receives a signal. */
    dicey_client_signal_fn *on_signal;
};

/**
 * @brief Creates a new client using the provided arguments.
 * @param dest The destination pointer to the new client. Must be freed using `dicey_client_delete()`.
 * @param args The arguments to use for the client. Can be NULL - in that case, the client will ignore all events.
 * @return     Error code. Possible values are:
 *             - OK: the client was successfully created
 *             - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_new(struct dicey_client **dest, const struct dicey_client_args *args);

/**
 * @brief Deletes a client, releasing any resources it may own.
 * @note  The client must be disconnected before being deleted. Deleting a connected client will cause it to disconnect
 *        abruptly, which may result in data loss.
 * @param client The client to delete. May be NULL - in that case, this function does nothing.
 */
DICEY_EXPORT void dicey_client_delete(struct dicey_client *client);

/**
 * @brief Connects a client to a server, blocking until the connection is established or an error occurs.
 * @param client The client to connect.
 * @param addr   The address (UDS or NT named pipe) of the server to connect to.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the client was successfully connected
 *               - EINVAL: the client is in the wrong state (i.e. already connected, dead, ...)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 */
DICEY_EXPORT enum dicey_error dicey_client_connect(struct dicey_client *client, struct dicey_addr addr);

/**
 * @brief Connects a client to a server, returning immediately and calling the provided callback when the connection is
 *        established or an error occurs.
 * @param client The client to connect.
 * @param addr   The address (UDS or NT named pipe) of the server to connect to.
 * @param cb     The callback to call when the connection is established or an error occurs.
 * @param data   The context to pass to the callback.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the client was successfully connected
 *               - EINVAL: the client is in the wrong state (i.e. already connected, dead, ...)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 */
DICEY_EXPORT enum dicey_error dicey_client_connect_async(
    struct dicey_client *client,
    struct dicey_addr addr,
    dicey_client_on_connect_fn *cb,
    void *data
);

/**
 * @brief Disconnects a client from a server, blocking until the disconnection is complete or an error occurs.
 * @param client The client to disconnect.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the client was successfully disconnected
 *               - EINVAL: the client is in the wrong state (i.e. not connected or connecting)
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_disconnect(struct dicey_client *client);

/**
 * @brief Disconnects a client from a server, returning immediately and calling the provided callback when the
 * disconnection sequence is complete or an error occurs.
 * @param client The client to disconnect.
 * @param cb     The callback to call when the disconnection is complete or an error occurs.
 * @param data   The context to pass to the callback.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the client was successfully disconnected
 *               - EINVAL: the client is in the wrong state (i.e. not connected or connecting)
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_disconnect_async(
    struct dicey_client *client,
    dicey_client_on_disconnect_fn *cb,
    void *data
);

/**
 * @brief Sends an EXEC request to the server, blocking until a response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request()`, and is equivalent to calling
 *        `dicey_client_request()` with a custom EXEC packet.
 * @param client   The client to send the request with.
 * @param path     The object path to send the request to.
 * @param sel      The selector pointing to the operation to execute.
 * @param payload  The payload to send with the request.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 * done.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - ESIGNATURE_MISMATCH: failed to match the signature of the operation
 */
DICEY_EXPORT enum dicey_error dicey_client_exec(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    struct dicey_arg payload,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Sends an EXEC request to the server, returning immediately and calling the provided callback when either a
 * response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request_async()`, and is equivalent to
 *        calling `dicey_client_request_async()` with a custom EXEC packet.
 * @param client The client to send the request with.
 * @param path   The object path to send the request to.
 * @param sel    The selector pointing to the operation to execute.
 * @param payload  The payload to send with the request.
 * @param cb     The callback to call when a response is received or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - ESIGNATURE_MISMATCH: failed to match the signature of the operation
 */
DICEY_EXPORT enum dicey_error dicey_client_exec_async(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    struct dicey_arg payload,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Sends a GET request to the server, blocking until a response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request()`, and is equivalent to calling
 *        `dicey_client_request()` with a custom GET packet.
 * @param client   The client to send the request with.
 * @param path     The object path to send the request to.
 * @param sel      The selector pointing to the property to get.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 * done.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - ESIGNATURE_MISMATCH: failed to match the signature of the property
 */
DICEY_EXPORT enum dicey_error dicey_client_get(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Sends a GET request to the server, returning immediately and calling the provided callback when either a
 * response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request_async()`, and is equivalent to
 *        calling `dicey_client_request_async()` with a custom GET packet.
 * @param client The client to send the request with.
 * @param path     The object path to send the request to.
 * @param sel      The selector pointing to the property to get.
 * @param cb     The callback to call when a response is received or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - ESIGNATURE_MISMATCH: failed to match the signature of the property
 */
DICEY_EXPORT enum dicey_error dicey_client_get_async(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Gets the context associated with a client. This is either the `ctx` parameter set via
 * `dicey_client_set_context()` or NULL if no context has been set.
 * @param client The client to get the context from.
 * @return       The context associated with the client, or NULL if no context has been set
 */
DICEY_EXPORT void *dicey_client_get_context(const struct dicey_client *client);

/**
 * @brief Asks the server the real path of a given path, blocking until a response is received or an error occurs.
 *        This function is useful to resolve aliases and get the actual path of an object.
 * @param client The client to send the request with.
 * @param path   The object path to inspect.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The response packet is expected to contain the real path as a path.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully sent and a response was received (`response` is valid)
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ETIMEDOUT: the request timed out
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPATH_NOT_FOUND: the path was not found
 * @param
 */
DICEY_EXPORT enum dicey_error dicey_client_get_real_path(
    struct dicey_client *client,
    const char *path,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Asks the server the real path of a given path, blocking until a response is received or an error occurs.
 *        This function is useful to resolve aliases and get the actual path of an object.
 */
DICEY_EXPORT enum dicey_error dicey_client_get_real_path_async(
    struct dicey_client *client,
    const char *path,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Inspects the object at a given path.
 * @param client   The client to send the request with.
 * @param path     The object path to inspect.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The response packet is expected to have a signature as specified by the object's introspection
 *                 trait.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - EPATH_NOT_FOUND: the path was not found
 */
DICEY_EXPORT enum dicey_error dicey_client_inspect_path(
    struct dicey_client *client,
    const char *path,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Inspects the object at a given path, returning immediately and calling the provided callback when a response
 *        is received or an error occurs.
 * @param client The client to send the request with.
 * @param path   The object path to inspect.
 * @param cb     The callback to call when a response is received or an error occurs. The packet is expected to have a
 *               signature as specified by the object's introspection trait.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_inspect_path_async(
    struct dicey_client *client,
    const char *path,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Inspects the object at a given path as XML.
 * @param client   The client to send the request with.
 * @param path     The object path to inspect.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The response packet is expected to contain an XML file as a string.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - EPATH_NOT_FOUND: the path was not found
 */
DICEY_EXPORT enum dicey_error dicey_client_inspect_path_as_xml(
    struct dicey_client *client,
    const char *path,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Inspects the object at a given path as XML, returning immediately and calling the provided callback when a
 *        response is received or an error occurs.
 * @param client The client to send the request with.
 * @param path   The object path to inspect.
 * @param cb     The callback to call when a response is received or an error occurs. The packet is expected to contain
 *               an XML file as a string.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_inspect_path_as_xml_async(
    struct dicey_client *client,
    const char *path,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Checks if a path is an alias for another object. Synchronous.
 * @param client The client to send the request with.
 * @param path   The path to check.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the path is an alias for another object
 *               - EPATH_NOT_ALIAS: the path is not an alias for another object
 *               - EPATH_NOT_FOUND: the path does not exist
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ETIMEDOUT: the request timed out
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_is_path_alias(
    struct dicey_client *client,
    const char *path,
    uint32_t timeout
);

/**
 * @brief Checks if a path is an alias for another object. Asynchronous.
 * @param client The client to send the request with.
 * @param path   The path to check.
 * @param cb     The callback to call when a response is received or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the path is an alias for another object
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ETIMEDOUT: the request timed out
 *               - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_is_path_alias_async(
    struct dicey_client *client,
    const char *path,
    dicey_client_on_is_alias_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Returns true if the client is connected to a server, false otherwise.
 * @param client The client to check.
 * @return       True if the client is connected to a server, false otherwise.
 */
DICEY_EXPORT bool dicey_client_is_running(const struct dicey_client *client);

/**
 * @brief Lists all objects on the server, blocking until a response is received or an error occurs.
 * @note  The list of objects returned will not contain aliases. Use `dicey_client_list_paths()` to get a list of all
 *        paths, including aliases.
 * @param client   The client to send the request with.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The packet is expected to have a signature as specified by `DICEY_REGISTRY_OBJECTS_PROP_SIG`.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out'
 *                 - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_objects(
    struct dicey_client *client,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Lists all objects on the server, returning immediately and calling the provided callback when a response is
 *        received or an error occurs.
 * @note  The list of objects returned will not contain aliases. Use `dicey_client_list_paths_async()` to get a list of
 *        all paths, including aliases.
 * @param client  The client to send the request with.
 * @param cb      The callback to call when a response is received or an error occurs. The packet is expected to have a
 *                signature as specified by `DICEY_REGISTRY_OBJECTS_PROP_SIG`.
 * @param data    The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return        Error code. A (non-exhaustive) list of possible values are:
 *                - OK: the request was successfully submitted for sending
 *                - EINVAL: the client is in the wrong state (i.e. not connected)
 *                - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_objects_async(
    struct dicey_client *client,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Lists all paths on the server, blocking until a response is received or an error occurs.
 * @note  The list of paths returned will also contain aliases. Use `dicey_client_list_objects()` to get a list of just
 *        objects.
 * @param client   The client to send the request with.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The packet is expected to have a signature as specified by `DICEY_REGISTRY_PATHS_PROP_SIG`.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out'
 *                 - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_paths(
    struct dicey_client *client,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Lists all paths on the server, returning immediately and calling the provided callback when a response is
 *        received or an error occurs.
 * @note  The list of paths returned will also contain aliases. Use `dicey_client_list_objects_async()` to get a list of
 *        just objects.
 * @param client  The client to send the request with.
 * @param cb      The callback to call when a response is received or an error occurs. The packet is expected to have a
 *                signature as specified by `DICEY_REGISTRY_PATHS_PROP_SIG`.
 * @param data    The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return        Error code. A (non-exhaustive) list of possible values are:
 *                - OK: the request was successfully submitted for sending
 *                - EINVAL: the client is in the wrong state (i.e. not connected)
 *                - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_paths_async(
    struct dicey_client *client,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Lists all traits on the server, blocking until a response is received or an error occurs.
 * @param client   The client to send the request with.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 *                 done. The packet is expected to have a signature as specified by `DICEY_REGISTRY_TRAITS_PROP_SIG`.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out'
 *                 - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_traits(
    struct dicey_client *client,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Lists all traits on the server, returning immediately and calling the provided callback when a response is
 *        received or an error occurs.
 * @param client  The client to send the request with.
 * @param cb      The callback to call when a response is received or an error occurs. The packet is expected to have a
 *                signature as specified by `DICEY_REGISTRY_TRAITS_PROP_SIG`.
 * @param data    The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return        Error code. A (non-exhaustive) list of possible values are:
 *                - OK: the request was successfully submitted for sending
 *                - EINVAL: the client is in the wrong state (i.e. not connected)
 *                - ENOMEM: memory allocation failed (out of memory)
 */
DICEY_EXPORT enum dicey_error dicey_client_list_traits_async(
    struct dicey_client *client,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Sends a request to the server, blocking until a response is received or an error occurs.
 * @param client   The client to send the request with.
 * @param packet   The packet to send.
 * @param response The response packet, if the request was successful. Must be freed using `dicey_packet_deinit()` when
 * done.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - ESIGNATURE_MISMATCH: failed to match the signature of the property or operation
 */
DICEY_EXPORT enum dicey_error dicey_client_request(
    struct dicey_client *client,
    struct dicey_packet packet,
    struct dicey_packet *response,
    uint32_t timeout
);

/**
 * @brief Sends a request to the server, returning immediately and calling the provided callback when either a response
 *        is received or an error occurs.
 * @param client The client to send the request with.
 * @param packet The packet to send.
 * @param cb     The callback to call when a response is received or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - ESIGNATURE_MISMATCH: failed to match the signature of the property or operation
 */
DICEY_EXPORT enum dicey_error dicey_client_request_async(
    struct dicey_client *client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Sends a SET request to the server, blocking until a response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request()`, and is equivalent to calling
 *        `dicey_client_request()` with a custom SET packet.
 * @param client   The client to send the request with.
 * @param path     The object path to send the request to.
 * @param sel      The selector pointing to the property to set.
 * @param payload  The payload to set the property to.
 * @param timeout  The maximum time to wait for a response, in milliseconds.
 * @return         Error code. A (non-exhaustive) list of possible values are:
 *                 - OK: the request was successfully sent and a response was received (`response` is valid)
 *                 - EINVAL: the client is in the wrong state (i.e. not connected)
 *                 - ETIMEDOUT: the request timed out
 *                 - ENOMEM: memory allocation failed (out of memory)
 *                 - ESIGNATURE_MISMATCH: failed to match the signature of the property
 */
DICEY_EXPORT enum dicey_error dicey_client_set(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    struct dicey_arg payload,
    uint32_t timeout
);

/**
 * @brief Sends a SET request to the server, returning immediately and calling the provided callback when either a
 * response is received or an error occurs.
 * @note  This function is meant as a convenience wrapper around `dicey_client_request_async()`, and is equivalent to
 *        calling `dicey_client_request_async()` with a custom SET packet.
 * @param client The client to send the request with.
 * @param path   The object path to send the request to.
 * @param sel    The selector pointing to the property to set.
 * @param payload  The payload to set the property to.
 * @param cb     The callback to call when a response is received or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - ESIGNATURE_MISMATCH: failed to match the signature of the property
 */
DICEY_EXPORT enum dicey_error dicey_client_set_async(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    struct dicey_arg payload,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Sets the context associated with a client. This context can then be retrieved using
 * `dicey_client_get_context()`, or passwd automatically to global callbacks.
 * @note  The client does not take ownership of the context, and it is the responsibility of the caller to manage its
 * memory.
 * @param client The client to set the context for.
 * @param data   The context to set. Can be anything, as long as it is a pointer whose lifetime exceeds that of the
 * client.
 * @return       The previous context associated with the client, or NULL if no context was set.
 */
DICEY_EXPORT void *dicey_client_set_context(struct dicey_client *client, void *data);

/**
 * @brief Subscribes to a signal identified by a given path and selector. Blocks until the subscription is complete or
 * an error occurs.
 * @note  If the path is an alias, the client will subscribe to the signal on the object that the alias points to.
 *        Any signal  will be sent from the path that the alias points to, not the alias itself; use the `aliases`
 *        parameter of the signal handler to determine whether the signal was sent from an alias or not.
 * @param client The client that will subscribe to the signal.
 * @param path   The path of the object hosting the signal.
 * @param sel    The selector pointing to the signal element.
 * @param timeout The maximum time to wait for the subscription to complete, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the subscription was successful
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 *               - ETIMEDOUT: the request timed out
 */
DICEY_EXPORT struct dicey_client_subscribe_result dicey_client_subscribe_to(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    uint32_t timeout
);

/**
 * @brief Subscribes to a signal identified by a given path and selector. Returns immediately and calls the provided
 *        callback when the subscription is complete or an error occurs.
 * @param client The client that will subscribe to the signal.
 * @param path   The path of the object hosting the signal.
 * @param sel    The selector pointing to the signal element.
 * @param cb     The callback to call when the subscription is complete or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for the subscription to complete, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the subscription request was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 *               - ETIMEDOUT: the request timed out
 */
DICEY_EXPORT enum dicey_error dicey_client_subscribe_to_async(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    dicey_client_on_sub_done_fn *cb,
    void *data,
    uint32_t timeout
);

/**
 * @brief Unubscribes from a signal identified by a given path and selector. Blocks until the unsubscription is complete
 *        or an error occurs.
 * @param client The client that will unsubscribe from the signal.
 * @param path   The path of the object hosting the signal.
 * @param sel    The selector pointing to the signal element.
 * @param timeout The maximum time to wait for the unsubscription to complete, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the unsubscription was successful
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 *               - ETIMEDOUT: the request timed out
 */
DICEY_EXPORT enum dicey_error dicey_client_unsubscribe_from(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    uint32_t timeout
);

/**
 * @brief Unubscribes from a signal identified by a given path and selector.
 * @param client The client that will unsubscribe from the signal.
 * @param path   The path of the object hosting the signal.
 * @param sel    The selector pointing to the signal element.
 * @param cb     The callback to call when the unsubscription is complete or an error occurs.
 * @param data   The context to pass to the callback.
 * @param timeout The maximum time to wait for the unsubscription to complete, in milliseconds.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the unsubscription was successfully submitted for sending
 *               - EINVAL: the client is in the wrong state (i.e. not connected)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EPEER_NOT_FOUND: the server is not up
 *               - ETIMEDOUT: the request timed out
 */
DICEY_EXPORT enum dicey_error dicey_client_unsubscribe_from_async(
    struct dicey_client *client,
    const char *path,
    struct dicey_selector sel,
    dicey_client_on_unsub_done_fn *cb,
    void *data,
    uint32_t timeout
);

#if defined(__cplusplus)
}
#endif

#endif // RGSZHBDASZ_CLIENT_H
