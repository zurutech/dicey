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

#include <stdint.h>
#include <string.h>

#include <dicey/core/views.h>

#include "unsafe.h"

void dunsafe_read_bytes(const struct dicey_view_mut dest, const void **const src) {
    memcpy(dest.data, *src, dest.len);

    *src = (const uint8_t *) *src + dest.len;
}

void dunsafe_write_bytes(void **const dest, const struct dicey_view view) {
    memcpy(*dest, view.data, view.len);

    *dest = (uint8_t *) *dest + view.len;
}
