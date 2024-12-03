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

#if !defined(IJNDTHYBPN_PLUGINS_H)
#define IJNDTHYBPN_PLUGINS_H

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../core/errors.h"

#include "server.h"

/**
 * @brief Represents the basic info about a plugin.
 */
struct dicey_plugin_info {
    const char *name; /**< The name of the plugin. May be NULL if the plugin hasn't yet registered itself */
    char *path;       /**< The path to the plugin. If the OS uses wide characters, the path is in whatever 8-bit
                           encoding the OS uses. Mutable due to libuv constraints, but shouldn't be mutated ever */
};

/**
 * @brief List of all event kinds that can occur with a plugin.
 */
enum dicey_plugin_event_kind {
    DICEY_PLUGIN_EVENT_SPAWNED,      /**< A plugin was spawned; it hasn't handshaked yet, and still is not registered */
    DICEY_PLUGIN_EVENT_READY,        /**< A plugin was loaded. It is now registered and ready to be used */
    DICEY_PLUGIN_EVENT_QUITTING,     /**< A plugin is quitting */
    DICEY_PLUGIN_EVENT_QUIT,         /**< A plugin quit cleanly */
    DICEY_PLUGIN_EVENT_FAILED,       /**< A plugin returned non-zero */
    DICEY_PLUGIN_EVENT_UNRESPONSIVE, /**< A plugin was killed because it failed to handshake in time. Expect a FAILED
                                        event */
};

/**
 * @brief A struct that represents an event that occurred with a plugin.
 */
struct dicey_plugin_event {
    enum dicey_plugin_event_kind kind; /**< The kind of event. */
    struct dicey_plugin_info info;     /**< The info of the affected plugin */
};

/**
 * @brief Lists all the plugins currently running.
 * @param server The server to list the plugins for.
 * @param buf    A pointer to a buffer that will be allocated to store the plugin information. if *buf is NULL, a new
 *               buffer will be allocated. If *buf is not NULL, the buffer will be used only if it is large enough.
 *               If buf is null, the function will count the number of plugins set *count to the number of plugins found
 *               and return DICEY_OK.
 * @param count  A pointer to a size_t that will be set to the number of plugins found. If set, *count is assumed to be
 *               the size of the buffer pointed to by *buf. After the call, *count will be set to the number of plugins
 *               found and stored in *buf. If *count is too small, the function will return DICEY_EOVERFLOW and *count
 *               will be set to the number of plugins found.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the plugins were successfully listed
 *               - EINVAL: the server is in the wrong state (i.e. not running)
 *               - ENOMEM: memory allocation failed (out of memory)
 *               - EOVERFLOW: the buffer is too small
 */
enum dicey_error dicey_server_list_plugins(
    struct dicey_server *server,
    struct dicey_plugin_info **buf,
    uint16_t *count
);

/**
 * @brief Spawns the plugin at the given path. The binary is expected to be an executable file or a file the OS can
 * execute directly, like a script with a shebang, binfmt, PATHEXT, ...
 * @note  This function is asynchronous and will return immediately. The caller should listen for the plugin events on
 *        the `dicey_server_on_plugin_event_fn` callback.
 * @param server The server to spawn the plugin for.
 * @param path   The path to the plugin binary.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the plugin was successfully spawned
 */
enum dicey_error dicey_server_spawn_plugin(struct dicey_server *server, const char *path);

/**
 * @brief Spawns the plugin at the given path. The binary is expected to be an executable file or a file the OS can
 * execute directly, like a script with a shebang, binfmt, PATHEXT, ...
 * @note  This function is synchronous and will block until the plugin has been spawned correctly.
 * @param server The server to spawn the plugin for.
 * @param path   The path to the plugin binary.
 * @return       Error code. A (non-exhaustive) list of possible values are:
 *               - OK: the plugin was successfully spawned
 */
enum dicey_error dicey_server_spawn_plugin_and_wait(struct dicey_server *server, const char *path);

/**
 * @brief Callback type for when a plugin event occurs.
 * @param server The server instance where the event occurred.
 * @param event  The event that occurred.
 */
typedef void dicey_server_on_plugin_event_fn(struct dicey_server *server, const struct dicey_plugin_event event);

#endif // DICEY_HAS_PLUGINS

#endif // IJNDTHYBPN_PLUGINS_H
