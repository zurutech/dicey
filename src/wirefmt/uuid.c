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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>
#include <dicey/core/type.h>
#include <dicey/core/views.h>

#include "sup/trace.h"

static bool hextobyte(const char str[2], uint8_t *const dest) {
    char *end = NULL;

    const unsigned long res = strtoul(str, &end, 16);

    if (res > UINT8_MAX || (!res && *end)) {
        return false;
    }

    *dest = (uint8_t) res;

    return true;
}

static bool parse_uuid(const char *uuid, uint8_t bytes[DICEY_UUID_SIZE]) {
    const size_t uuid_len = strlen(uuid);

    switch (uuid_len) {
    case 32U:
    case 36U:
        break;

    default:
        return false;
    }

    const uint8_t *end = bytes + DICEY_UUID_SIZE;

    for (uint8_t *it = bytes; it < end; ++it) {
        const char cur_byte[3] = { [0] = uuid[0], [1] = uuid[1], [2] = '\0' };

        uuid += 2U;

        if (!hextobyte(cur_byte, it)) {
            return false;
        }

        if (*uuid == '-') {
            ++uuid;
        }
    }

    return !*uuid; // the string must be fully consumed
}

enum dicey_error dicey_uuid_from_bytes(struct dicey_uuid *const uuid, const uint8_t *const bytes, const size_t len) {
    assert(uuid && bytes);

    if (len != DICEY_UUID_SIZE) {
        return TRACE(DICEY_EUUID_NOT_VALID);
    }

    memcpy(uuid->bytes, bytes, DICEY_UUID_SIZE);

    return DICEY_OK;
}

enum dicey_error dicey_uuid_from_string(struct dicey_uuid *const uuid, const char *const str) {
    assert(uuid && str);

    if (!parse_uuid(str, uuid->bytes)) {
        return TRACE(DICEY_EUUID_NOT_VALID);
    }

    return DICEY_OK;
}
