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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <dicey/dicey.h>

int main(const int argc, const char *const argv[]) {
    struct dicey_plugin *plugin = NULL;

    enum dicey_error err = dicey_plugin_init(argc, argv, &plugin, &(struct dicey_plugin_args) {
        .cargs = {
            .on_signal = NULL,
            .inspect_func = NULL,
        },

        .name = "dummy_plugin",
        .on_quit = NULL,
        .on_work_received = NULL,
    });

    if (err) {
        fprintf(stderr, "error: failed to initialise plugin: %s\n", dicey_error_msg(err));

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
