// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stddef.h>
#include <stdint.h>

#include <uv.h>

#include <dicey/dicey.h>

ptrdiff_t util_random_bytes(uint8_t *const bytes, const size_t len) {
    const ptrdiff_t res = uv_random(NULL, NULL, bytes, len, 0, NULL);

    return res ? res : (ptrdiff_t) len;
}
