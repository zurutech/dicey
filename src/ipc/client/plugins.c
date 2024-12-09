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
#include <stddef.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/client.h>
#include <dicey/ipc/plugins.h>

#include "sup/util.h"

#include "ipc/plugin-macros.h"

#include "client-internal.h"

enum dicey_error dicey_plugin_init(
    const int argc,
    const char *const argv[],
    struct dicey_client **const dest,
    const struct dicey_client_args *const args
) {
    // future proof: we don't do anything with argc and argv, but we might in the future
    // and it's better not having to change the signature of this function
    DICEY_UNUSED(argc);
    DICEY_UNUSED(argv);

    assert(argc && argv && dest && args);

    struct dicey_client *client = NULL;

    enum dicey_error err = dicey_client_new(&client, args);
    if (err) {
        return err;
    }

    assert(client);

    err = dicey_client_open_fd(client, DICEY_PLUGIN_FD);
    if (err) {
        dicey_client_delete(client);
    }

    *dest = client;

    return err;
}
