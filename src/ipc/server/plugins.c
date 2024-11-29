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

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _XOPEN_SOURCE 700

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <assert.h>
#include <stddef.h>

#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "server.h"

enum dicey_error dicey_server_plugins_list(
    struct dicey_server *const server,
    struct dicey_plugin_info **const buf,
    size_t *const count
) {
    assert(server && buf && count);

    return DICEY_OK;
}

#endif // DICEY_HAS_PLUGINS
