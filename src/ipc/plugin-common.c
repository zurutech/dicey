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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <string.h>

#include <dicey/ipc/builtins/plugins.h>

#include "server/plugins-internal.h"

#define METAPLUGIN_PREFIX DICEY_SERVER_PLUGINS_PATH "/"

const char *dicey_plugin_name_from_path(const char *const path) {
    assert(path);

    if (strncmp(path, METAPLUGIN_PREFIX, sizeof METAPLUGIN_PREFIX - 1)) {
        return NULL; // has to start with the prefix
    }

    const char *const name = path + sizeof METAPLUGIN_PREFIX - 1;

    return dicey_string_is_valid_plugin_name(name) ? name : NULL;
}
