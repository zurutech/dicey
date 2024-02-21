// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include "chunk.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <uv.h>

#define BUFFER_MINCAP 1024U // 1KB
#define READ_MINBUF 256U    // 256B

size_t dicey_chunk_avail(struct dicey_chunk *const cnk) {
    return cnk ? cnk->cap - cnk->len - sizeof *cnk : 0U;
}

struct dicey_chunk *dicey_chunk_grow(struct dicey_chunk *buf) {
    const bool zero = !buf;
    const size_t new_cap = buf && buf->cap ? buf->cap * 3 / 2 : BUFFER_MINCAP;

    buf = realloc(buf, new_cap);
    if (buf) {
        if (zero) {
            *buf = (struct dicey_chunk) { 0 };
        }

        buf->cap = new_cap;
    }

    return buf;
}

uv_buf_t dicey_chunk_get_buf(struct dicey_chunk **const buf, const size_t min) {
    assert(buf);

    size_t avail = 0U;
    for (;;) {
        avail = dicey_chunk_avail(*buf);

        if (avail >= min) {
            break;
        }

        *buf = dicey_chunk_grow(*buf);
    }

    struct dicey_chunk *const cnk = *buf;

    return uv_buf_init(cnk->bytes + cnk->len, avail > UINT_MAX ? UINT_MAX : (unsigned int) avail);
}

void dicey_chunk_clear(struct dicey_chunk *const buffer) {
    assert(buffer);

    buffer->len = 0;
}
