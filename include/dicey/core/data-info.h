// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(VEGOWIWLXE_DATA_INFO_H)
#define VEGOWIWLXE_DATA_INFO_H

#include "type.h"
#include "views.h"

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
    struct dicey_selector selector;

    struct dicey_errmsg error;
};

#endif // VEGOWIWLXE_DATA_INFO_H
