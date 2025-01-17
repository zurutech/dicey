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

#include <assert.h>
#include <stddef.h>

#include <dicey/dicey.h>

#include <util/uuid.h>

#define HEX_DIGITS "0123456789abcdef"

static_assert(sizeof(struct dicey_uuid) * 2U + 4U + 1U == UTIL_UUID_STR_LEN, "UUID length mismatch");

enum dicey_error util_uuid_to_string(const struct dicey_uuid uuid, char *dest, size_t len) {
    const size_t *next_dash_at = (const size_t[]) { 4U, 6U, 8U, 10U };

    if (len < UTIL_UUID_STR_LEN) {
        return DICEY_EINVAL;
    }

    for (size_t i = 0U; i < sizeof uuid.bytes; ++i) {
        const size_t byte = uuid.bytes[i];

        *dest++ = HEX_DIGITS[byte >> 4U];
        *dest++ = HEX_DIGITS[byte & 0x0FU];

        if (i == *next_dash_at) {
            *dest++ = '-';
            ++next_dash_at;
        }
    }

    *dest = '\0';

    return DICEY_OK;
}
