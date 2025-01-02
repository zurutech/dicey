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

#if !defined(VEGOWIWLXE_DATA_INFO_H)
#define VEGOWIWLXE_DATA_INFO_H

#include "type.h"
#include "views.h"

#if defined(__cplusplus)
extern "C" {
#endif

// union used internally by dicey_value to represent a parsed value. Not intended for external use.
union _dicey_data_info {
    dicey_bool boolean;
    dicey_byte byte;

    dicey_float floating;

    dicey_i16 i16;
    dicey_i32 i32;
    dicey_i64 i64;

    dicey_u16 u16;
    dicey_u32 u32;
    dicey_u64 u64;

    struct dtf_probed_list {
        uint16_t inner_type;
        uint16_t nitems;
        struct dicey_view data;
    } list; // for array, pair, tuple

    struct dtf_probed_bytes {
        uint32_t len;
        const uint8_t *data;
    } bytes;

    const char *str; // for str, path

    struct dicey_uuid uuid;

    struct dicey_selector selector;

    struct dicey_errmsg error;
};

#if defined(__cplusplus)
}
#endif

#endif // VEGOWIWLXE_DATA_INFO_H
