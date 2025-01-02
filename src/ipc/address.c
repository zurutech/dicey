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
#include <stdlib.h>
#include <string.h>

#include <dicey/ipc/address.h>

#if defined(__linux__)
#define HAS_ABSTRACT_SOCKETS 1
#else
#define HAS_ABSTRACT_SOCKETS 0
#endif

static char *cnkdup(const char *const str, const size_t len) {
    assert(str);

    char *const copy = malloc(len);
    if (!copy) {
        return NULL;
    }

    return memcpy(copy, str, len);
}

static const char *connstr_fixup(const char *const connstr, const size_t len) {
    assert(connstr);

    char *const str_copy = cnkdup(connstr, len + 1);
    if (!str_copy) {
        return NULL;
    }

#if HAS_ABSTRACT_SOCKETS
    if (*str_copy == '@') {
        *str_copy = '\0';
    }
#endif

    return str_copy;
}

void dicey_addr_deinit(struct dicey_addr *const addr) {
    if (addr) {
        free((void *) addr->addr); // cast away const, this originated from strdup

        *addr = (struct dicey_addr) { 0 };
    }
}

DICEY_EXPORT enum dicey_error dicey_addr_dup(struct dicey_addr *const dest, const struct dicey_addr src) {
    assert(dest);

    if (!src.addr || !src.len) {
        *dest = (struct dicey_addr) { 0 };

        return DICEY_OK;
    }

    char *const addr_copy = cnkdup(src.addr, src.len + 1);
    if (!addr_copy) {
        return DICEY_ENOMEM;
    }

    *dest = (struct dicey_addr) {
        .addr = addr_copy,
        .len = src.len,
    };

    return DICEY_OK;
}

const char *dicey_addr_from_str(struct dicey_addr *const dest, const char *const str) {
    assert(dest && str);

    const size_t len = strlen(str);
    const char *const connstr = connstr_fixup(str, len);
    if (!connstr) {
        return NULL;
    }

    *dest = (struct dicey_addr) {
        .addr = connstr,
        .len = len,
    };

    return connstr;
}
