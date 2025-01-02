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

#include "chunk.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <uv.h>

#define BUFFER_MINCAP 1024U // 1KB

size_t dicey_chunk_avail(const struct dicey_chunk *const cnk) {
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
    if (!cnk) {
        return uv_buf_init(NULL, 0U);
    }

    return uv_buf_init(cnk->bytes + cnk->len, avail > UINT_MAX ? UINT_MAX : (unsigned int) avail);
}

void dicey_chunk_clear(struct dicey_chunk *const buffer) {
    assert(buffer);

    buffer->len = 0;
}
