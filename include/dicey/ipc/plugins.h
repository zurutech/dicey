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

#include "server.h"

/**
 * @brief The maximum length of a plugin name, in bytes
 */
#define DICEY_PLUGIN_MAX_NAME_LEN ((size_t) 64U)

/**
 * @brief Describes the version of a plugin.
 */
struct dicey_plugin_version {
    union {
        struct {
            uint32_t major; /**< Major version number. */
            uint32_t minor; /**< Minor version number. */
            uint32_t patch; /**< Patch version number. */
        };

        uint32_t rolling; /**< Rolling version number, if used */
    };

    bool is_rolling; /**< Whether the version is "rolling" (i.e. r123) */
};

struct dicey_plugin_info {
    const char name[DICEY_PLUGIN_MAX_NAME_LEN + 1U]; /**< The name of the plugin. Max DICEY_PLUGIN_MAX_NAME_LEN bytes */
    struct dicey_plugin_version version;             /**< The version of the plugin. */
    const char *description;                         /**< The description of the plugin. Assumed to be static */
};

/**
 * @brief Lists all the available plugins from the plugin directory.
 * @param server The server to list the plugins for. Must have a valid plugin path set.
 * @param buf    A pointer to a buffer that will be allocated to store the plugin information. if *buf is NULL, a new
 *               buffer will be allocated. If *buf is not NULL, the buffer will be used only if it is large enough.
 * @param count  A pointer to a size_t that will be set to the number of plugins found. If set, *count is assumed to be
 *               the size of the buffer pointed to by *buf. After the call, *count will be set to the number of plugins
 *               found and stored in *buf. If *count is too small, the function will return DICEY_EOVERFLOW and *count
 *               will be set to the number of plugins found.
 * @return       either
 */
enum dicey_error dicey_server_plugins_list(struct dicey_server *server, struct dicey_plugin_info **buf, size_t *count);

#endif // DICEY_HAS_PLUGINS

#endif // IJNDTHYBPN_PLUGINS_H
