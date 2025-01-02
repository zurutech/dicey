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

#if !defined(KIAZVPTVYT_CHUNK_H)
#define KIAZVPTVYT_CHUNK_H

#include <stddef.h>

#include <uv.h>

#include "dicey_config.h"

#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4200)
#endif

struct dicey_chunk {
    size_t len;
    size_t cap;
    char bytes[];
};

size_t dicey_chunk_avail(const struct dicey_chunk *buf);
void dicey_chunk_clear(struct dicey_chunk *const buffer);
struct dicey_chunk *dicey_chunk_grow(struct dicey_chunk *buf);
uv_buf_t dicey_chunk_get_buf(struct dicey_chunk **buf, size_t min);

#endif // KIAZVPTVYT_CHUNK_H
