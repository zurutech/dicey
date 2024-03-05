// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <string.h>

#include <dicey/ipc/address.h>

struct dicey_addr dicey_addr_from_str(const char *const str) {
    assert(str);

    return (struct dicey_addr) {
        .addr = str,
        .len = strlen(str),
    };
}
