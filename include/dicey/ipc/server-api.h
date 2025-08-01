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

#if !defined(JJYWCOYURK_SERVER_H)
#define JJYWCOYURK_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include "../core/builders.h"
#include "../core/errors.h"
#include "../core/message.h"
#include "../core/packet.h"

#include "address.h"
#include "registry.h"
#include "request.h"
#include "server.h"

#include "dicey_config.h"
#include "dicey_export.h"

#if DICEY_HAS_PLUGINS

#include "plugins.h"

#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Structure that describes the data entry for a client connected to a server.
 */
struct dicey_client_info {
    size_t id;       /**< The client's unique identifier. May be reused after the client disconnects. */
    void *user_data; /**< The user data associated with the client. Can be set by a on_connect callback. */
};

/**
 * @brief Callback type for when a client connects to the server.
 * @param server    The server that the client connected to.
 * @param id        The new client's unique identifier.
 * @param user_data A pointer that can be set by the callback to associate user data with the client.
 *                  This data can then be retrieved by other callbacks as the `user_data` field of `struct
 * dicey_client_info`.
 * @return          True if the client should be accepted, false otherwise. If false is returned, the client will be
 * kicked.
 */
typedef bool dicey_server_on_connect_fn(struct dicey_server *server, size_t id, void **user_data);

/**
 * @brief Callback type for when a client disconnects from the server.
 * @param server The server that the client disconnected from.
 * @param cln    The info of the client that disconnected. This is the last chance to properly clean up any resources
 *               associated with the client.
 */
typedef void dicey_server_on_disconnect_fn(struct dicey_server *server, const struct dicey_client_info *cln);

/**
 * @brief Callback type for when an error occurs on the server.
 * @param server The server instance where the error occurred on.
 * @param err    The error code.
 * @param cln    The info of the client that caused the error. This can be NULL if the error is not client-specific.
 * @param msg    The error message, which may be a format string.
 * @param ...    The arguments to the format string, if any, which the caller may want to format properly.
 */
typedef void dicey_server_on_error_fn(
    struct dicey_server *server,
    enum dicey_error err,
    const struct dicey_client_info *cln,
    const char *msg,
    ...
);

/**
 * @brief Callback type for when a request is received from a client.
 * @param server  The server instance that received the request.
 * @param cln     The info of the client that sent the request.
 * @param request The request that was received. User code will be able to reply and handle the request via this
 * structure. The request object is valid until the request is replied to or cancelled.
 */
typedef void dicey_server_on_request_fn(struct dicey_server *server, struct dicey_request *request);

/**
 * @brief Callback type that is called when the server either starts up successfully or fails to start up.
 * @note  This function is useful to probe the server's status after starting it up in another thread.
 & @param server The server instance that started up.
 */
typedef void dicey_server_on_startup(struct dicey_server *server, enum dicey_error error);

/**
 * @brief Describes the arguments that can be passed to a new Dicey server.
 */
struct dicey_server_args {
    dicey_server_on_connect_fn *on_connect;       /**< The callback to be called when a client connects. */
    dicey_server_on_disconnect_fn *on_disconnect; /**< The callback to be called when a client disconnects. */
    dicey_server_on_error_fn *on_error;           /**< The callback to be called when an error occurs. */

    dicey_server_on_startup *on_startup;    /**< The callback to be called when the server starts up. */
    dicey_server_on_request_fn *on_request; /**< The callback to be called when a request is received. */

#if DICEY_HAS_PLUGINS
    dicey_server_on_plugin_event_fn *on_plugin_event; /**< The callback to be called when a plugin event occurs. */

    uint64_t plugin_startup_timeout; /**< The timeout in ms for a plugin to start up. If not set, it's one second*/
#endif
};

/**
 * @brief Deletes a server instance, shutting it down first if necessary.
 */
DICEY_EXPORT void dicey_server_delete(struct dicey_server *state);

/**
 * @brief Creates a new Dicey server instance with the given arguments.
 * @param dest The destination pointer to store the pointer to the new server instance.
 * @param args The arguments to pass to the new server instance. If NULL, the server will ignore all events and
 * requests. The server will not hold a reference to this structure, so it can be safely stack allocated.
 * @return     Error code. The possible values are several and include:
 *             - OK: the server was successfully created
 *             - ENOMEM: memory allocation failed
 *             - ENOENT: the plugin path does not exist
 *             - ENOTDIR: the plugin path is not a directory
 */
DICEY_EXPORT enum dicey_error dicey_server_new(struct dicey_server **dest, const struct dicey_server_args *args);

/**
 * @brief Adds an object to the server. The server will then be able to handle requests at the given path.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the trait is added to the server's registry immediately. If the server is running, the
 *       request is executed on the server thread: the function only submit the request to the server thread and
 *       return immediately.
 * @param server      The server to add the object to.
 * @param path        The path at which the object will be accessible.
 * @param trait_names The names of the traits that the object implements. The registry will take ownership of this set.
 * @return            Error code. The possible values are several and include:
 *                    - OK: the object was successfully added
 *                    - ENOMEM: memory allocation failed
 *                    - EPATH_MALFORMED: the path is malformed
 *                    - EEXIST: the object is already registered
 */
DICEY_EXPORT enum dicey_error dicey_server_add_object(
    struct dicey_server *server,
    const char *path,
    struct dicey_hashset *trait_names
);

/**
 * @brief Adds an object to the server. The server will then be able to handle requests at the given path.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the trait is added to the server's registry immediately (note: not thread safe).
 *       If the server is running, the request is executed on the server thread: the function only submit the request to
 *       the server thread and return immediately.
 * @param server      The server to add the object to.
 * @param path        The path at which the object will be accessible.
 * @param ...         The names of the traits that the object implements. The last argument must be NULL.
 */
DICEY_EXPORT enum dicey_error dicey_server_add_object_with(struct dicey_server *server, const char *path, ...);

/**
 * @brief Adds a trait to the server's registry. The server will then be able to handle requests for this trait.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the trait is added to the server's registry immediately (note: not thread safe).
 *       If the server is running, the request is executed on the server thread: the function only submit the request to
 *       the server thread and return immediately.
 * @param server The server to add the trait to.
 * @param trait  The trait to add to the server's registry. The registry will take ownership of this trait.
 * @return       Error code. The possible values are several and include:
 *               - OK: the trait was successfully added
 *               - ENOMEM: memory allocation failed
 *               - EEXIST: the trait is already registered
 */
DICEY_EXPORT enum dicey_error dicey_server_add_trait(struct dicey_server *server, struct dicey_trait *trait);

/**
 * @brief Adds an alias to an object. The object will then be accessible from the alias path.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the alias is added to the server's registry immediately (note: not thread safe).
 *       If the server is running, the request is executed on the server thread: the function only submit the request to
 *       the server thread and return immediately.
 * @note The alias must not be already registered, and the path must be a valid path. If the path is an alias, the real
 *       path of the object will be used to register the new alias.
 * @param server The server to add the alias to.
 * @param path   The path of the object to alias.
 * @param alias  The alias to add to the object.
 * @return       Error code. The possible values are several and include:
 *               - OK: the alias was successfully added
 *               - ENOMEM: memory allocation failed
 *               - EPATH_MALFORMED: the path or the alias is malformed
 *               - EPATH_NOT_FOUND: the object is not registered
 *               - EEXIST: the alias is already registered
 */
DICEY_EXPORT enum dicey_error dicey_server_add_object_alias(
    struct dicey_server *server,
    const char *path,
    const char *alias
);

/**
 * @brief Adds a set of aliases to an object. The object will then be accessible from the alias paths.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the aliases are added to the server's registry immediately (note: not thread safe).
 *       If the server is running, the request is executed on the server thread: the function only submit the request to
 *       the server thread and return immediately.
 * @note The aliases must not be already registered, and the path must be a valid path. If the path is an alias, the
 * real path of the object will be used to register the new aliases.
 * @param server  The server to add the aliases to.
 * @param path    The path of the object to alias.
 * @param aliases The set of aliases to add to the object. The server will take ownership of this set.
 * @return        Error code. The possible values are several and include:
 *                - OK: the aliases were successfully added
 *                - ENOMEM: memory allocation failed
 *                - EPATH_MALFORMED: the path or one of the aliases is malformed
 *                - EPATH_NOT_FOUND: the object is not registered
 *                - EEXIST: one of the aliases is already registered
 */
DICEY_EXPORT enum dicey_error dicey_server_add_object_aliases(
    struct dicey_server *server,
    const char *path,
    struct dicey_hashset *aliases
);

/**
 * @brief Deletes an alias from an object.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the alias is removed from the server's registry immediately (note: not thread safe).
 *       If the server is running, the request is executed on the server thread: the function only submit the request to
 *       the server thread and return immediately.
 * @param server The server to delete the alias from.
 * @param alias  The alias to remove.
 * @return       Error code. The possible values are several and include:
 *               - OK: the alias was successfully deleted
 *               - ENOMEM: memory allocation failed
 *               - EPATH_MALFORMED: the alias is malformed
 *               - EPATH_NOT_FOUND: the alias is not registered
 *               - EPATH_NOT_ALIAS: the path is not an alias
 */
DICEY_EXPORT enum dicey_error dicey_server_delete_object_alias(struct dicey_server *server, const char *alias);

/**
 * @brief Deletes an object from the server. The server will stop handling requests at the given path.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the trait is removed from the server's registry immediately. If the server is running, the
 *       request is executed on the server thread: the function only submit the request to the server thread and
 *       return immediately.
 * @param server The server to delete the object from.
 * @param path   The path at which the object is accessible. Must be a valid path.
 * @return       Error code. The possible values are several and include:
 *               - OK: the object was successfully deleted
 *               - ENOMEM: memory allocation failed
 *               - EPATH_MALFORMED: the path is malformed
 *               - EPATH_NOT_FOUND: the object is not registered
 */
DICEY_EXPORT enum dicey_error dicey_server_delete_object(struct dicey_server *server, const char *path);

/**
 * @brief Deletes all aliases of an object from the server.
 * @note This function has a different behaviour depending on whether the server is running or not. If the server is
 *       in a stopped state, the aliases are removed from the server's registry immediately. If the server is running,
 * the request is executed on the server thread: the function only submit the request to the server thread and return
 * immediately.
 * @param server The server to delete the aliases from.
 * @param path   The path at which the object is accessible. Must be the object's main path, not an alias.
 * @return       Error code. The possible values are several and include:
 *               - OK: the aliases were successfully deleted
 *               - EINVAL: the path is not the main path of the object (i.e. it is an alias)
 *               - EPATH_MALFORMED: the path is malformed
 *               - EPATH_NOT_FOUND: the object is not registered
 */
DICEY_EXPORT enum dicey_error dicey_server_drop_all_aliases_of_object(struct dicey_server *server, const char *path);

/**
 * @brief Gets the context associated with the server, as set by `dicey_server_set_context`.
 * @param server The server to get the context from.
 * @return       The context pointer associated with the server (or NULL if none was set).
 */
DICEY_EXPORT void *dicey_server_get_context(struct dicey_server *server);

/**
 * @brief Gets the registry associated with the server.
 * @note  This function can't be called after the server has been started. The registry is owned by the server and will
 *        be deallocated when the server is deleted. Do not use the registry after the server has been started.
 * @param server The server to get the registry from. Must be in a stopped state.
 * @return       The registry associated with the server, or NULL if the server is running.
 */
DICEY_EXPORT struct dicey_registry *dicey_server_get_registry(struct dicey_server *server);

/**
 * @brief Kicks a client from the server. The request is asynchronous.
 * @param server The server to kick the client from.
 * @param id     The unique identifier of the client to kick.
 * @return       Error code. The possible values are several and include:
 *               - OK: the kick request was successfully initiated
 *               - ENOMEM: memory allocation failed
 */
DICEY_EXPORT enum dicey_error dicey_server_kick(struct dicey_server *server, size_t id);

/**
 * @brief Raises a signal, notifying all clients subscribed to it. This function is asynchronous and won't wait for the
 *        signal to actually be sent.
 * @param server The server to raise the signal from.
 * @param packet The signal to raise. The ownership of the packet is transferred to the server, which will free it when
 *               done. This packet must be a signal packet.
 * @return       Error code. The possible values are several and include:
 *               - OK: the signal was successfully raised
 *               - ENOMEM: memory allocation failed
 *               - EINVAL: the packet is invalid (e.g. it is not a signal)
 *               - EELEMENT_NOT_FOUND: the signal's element is not found
 */
DICEY_EXPORT enum dicey_error dicey_server_raise(struct dicey_server *server, struct dicey_packet packet);

/*
 * @brief Raises a signal, notifying all clients subscribed to it. This function is synchronous and will block until the
 *        signal is actually sent.
 * @note  Even if this function returns, there is no guarantee that the clients actually received anything. This
 * function only guarantees that the `write()` syscall is actually performed and that it succeeded.
 * @param server The server to raise the signal from.
 * @param packet The signal to raise. The ownership of the packet is transferred to the server, which will free it when
 *               done. This packet must be a signal packet.
 * @return       Error code. The possible values are several and include:
 *               - OK: the signal was successfully raised
 *               - ENOMEM: memory allocation failed
 *               - EINVAL: the packet is invalid (e.g. it is not a signal)
 *               - EELEMENT_NOT_FOUND: the signal's element is not found
 */
DICEY_EXPORT enum dicey_error dicey_server_raise_and_wait(struct dicey_server *server, struct dicey_packet packet);

/**
 * @brief Replies to a client. This functions is asynchronous and won't wait for the packet to actually be sent.
 * @param server The server to send the packet from.
 * @param id     The unique identifier of the client to send the packet to.
 * @param packet The packet to send. The ownership of the packet is transferred to the server, which will free it when
 * done. This packet must be response matching the sequence number, path and selector of a previously received request.
 * @return       Error code. The possible values are several and include:
 *               - OK: the packet was successfully sent
 *               - ENOMEM: memory allocation failed
 *               - EINVAL: the packet is invalid (e.g. it is not a response or the sequence number does not match any
 *                         request)
 */
DICEY_EXPORT enum dicey_error dicey_server_send_response(
    struct dicey_server *server,
    size_t id,
    struct dicey_packet packet
);

/**
 * @brief Replies to a client and waits for a response. This function is synchronous and will block until the message is
 *        sent.
 * @note  Even if this function returns, there is no guarantee that the client actually received anything. This function
 *        only guarantees that the `write()` syscall is actually performed and that it succeeded.
 * @param server The server to send the packet from.
 * @param id     The unique identifier of the client to send the packet to.
 * @param packet The packet to send. The ownership of the packet is transferred to the server, which will free it when
 * done. This packet must be response matching the sequence number, path and selector of a previously received request.
 * @return       Error code. The possible values are several and include:
 *               - OK: the packet was successfully sent
 *               - ENOMEM: memory allocation failed
 *               - EINVAL: the packet is invalid (e.g. it is not a response or the sequence number does not match any
 *                         request)
 */
DICEY_EXPORT enum dicey_error dicey_server_send_response_and_wait(
    struct dicey_server *server,
    size_t id,
    struct dicey_packet packet
);

/**
 * @brief Sets the context associated with the server. This context can be retrieved later with
 * `dicey_server_get_context`.
 * @note  The context is not owned by the server and will not be deallocated when the server is deleted. The caller is
 *        responsible for managing the memory associated with the context.
 * @param server       The server to set the context for.
 * @param new_context  The new context pointer to associate with the server.
 * @return             The old context pointer associated with the server (or NULL if none was set).
 */
DICEY_EXPORT void *dicey_server_set_context(struct dicey_server *server, void *new_context);

/**
 * @brief Starts the server, making it accept new connections and requests.
 * @note  This function actually executes a loop that runs until one of the `dicey_server_stop` family of functions is
 *        called. The thread calling this function thus becomes the server thread and will be blocked indefinitely or
 *        until the server is stopped.
 * @param server The server to start.
 * @param addr   The address (UDS or NT Named Pipe) to bind the server to. If an UDS address is used, the caller must
 *               ensure that the path does not point to an already existing file or socket, and that the server has the
 *               necessary permissions to listen() on the path.
 * @return       Error code. The possible values are several and include:
 *               - OK: the server start operation was successfully initiated
 *               - ENOMEM: memory allocation failed
 *               TODO: incomplete, document later
 */
DICEY_EXPORT enum dicey_error dicey_server_start(struct dicey_server *server, struct dicey_addr addr);

/**
 * @brief Closes the server, stopping it from accepting new connections and requests. This function immediately returns,
 *        and the server will be stopped asynchronously.
 * @note  This functions kicks all clients still connected. The thread blocked on `dicey_server_start` will be unblocked
 *        upon successful completion of this operation.
 * @param server The server to stop.
 * @return       Error code. The possible values are several and include:
 *               - OK: the server stop operation was successfully initiated
 *               - ENOMEM: memory allocation failed
 */
DICEY_EXPORT enum dicey_error dicey_server_stop(struct dicey_server *server);

/**
 * @brief Closes the server, stopping it from accepting new connections and requests. This function blocks until the
 *        server is actually stopped.
 * @note  This functions kicks all clients still connected. The thread blocked on `dicey_server_start` will be unblocked
 *        upon successful completion of this operation.
 * @param server The server to stop.
 * @return       Error code. The possible values are several and include:
 *               - OK: the server stop operation was successfully initiated
 *               - ENOMEM: memory allocation failed
 */
DICEY_EXPORT enum dicey_error dicey_server_stop_and_wait(struct dicey_server *server);

#if defined(__cplusplus)
}
#endif

#endif // JJYWCOYURK_SERVER_H
