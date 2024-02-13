// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stdint.h>
#include <string.h>

#include <dicey/internal/views.h>

#include "unsafe.h"

void dunsafe_read_bytes(const struct dicey_view_mut dest, const void **const src) {
    memcpy(dest.data, *src, dest.len);

    *src = (const uint8_t *) *src + dest.len;
}

void dunsafe_write_bytes(void **const dest, const struct dicey_view view) {
    memcpy(*dest, view.data, view.len);

    *dest = (uint8_t *) *dest + view.len;
}
