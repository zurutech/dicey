// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(KIAZVPTVYT_CHUNK_H)
#define KIAZVPTVYT_CHUNK_H

#include <stddef.h>

#include <uv.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

struct dicey_chunk {
    size_t len;
    size_t cap;
    char bytes[];
};

size_t dicey_chunk_avail(struct dicey_chunk *buf);
void dicey_chunk_clear(struct dicey_chunk *const buffer);
struct dicey_chunk *dicey_chunk_grow(struct dicey_chunk *buf);
uv_buf_t dicey_chunk_get_buf(struct dicey_chunk **buf, size_t min);

#endif // KIAZVPTVYT_CHUNK_H
