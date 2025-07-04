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

#include <stdbool.h>
#include <string.h>

#include <dicey/core/message.h>

bool dicey_message_matches_element(
    const struct dicey_message *const msg,
    const char *const path,
    const char *const trait,
    const char *const elem
) {
    if (!msg || !path || !trait || !elem) {
        return false;
    }

    return !strcmp(msg->path, path) && !strcmp(msg->selector.trait, trait) && !strcmp(msg->selector.elem, elem);
}

bool dicey_message_matches_element_under_root(
    const struct dicey_message *const msg,
    const char *const root,
    const char *const trait,
    const char *const elem
) {
    return !strncmp(msg->path, root, strlen(root)) && !strcmp(msg->selector.trait, trait) &&
           !strcmp(msg->selector.elem, elem);
}
