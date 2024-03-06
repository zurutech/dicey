// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _CRT_SECURE_NO_WARNINGS 1
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

static const char *connstr_fixup(const char *const connstr) {
    assert(connstr);

    char *const str_copy = strdup(connstr);
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

const char *dicey_addr_from_str(struct dicey_addr *const dest, const char *const str) {
    assert(dest && str);

    const size_t len = strlen(str);
    const char *const connstr = connstr_fixup(str);
    if (!connstr) {
        return NULL;
    }

    *dest = (struct dicey_addr) {
        .addr = connstr,
        .len = len,
    };

    return connstr;
}
