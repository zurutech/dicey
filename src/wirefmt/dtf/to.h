// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(OQJPHQJJLF_DTF_TO_H)
#define OQJPHQJJLF_DTF_TO_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/value.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

typedef dicey_bool dtf_bool;
typedef dicey_byte dtf_byte;

typedef dicey_float dtf_float;

typedef dicey_i16 dtf_i16;
typedef dicey_i32 dtf_i32;
typedef dicey_i64 dtf_i64;

typedef dicey_u16 dtf_u16;
typedef dicey_u32 dtf_u32;
typedef dicey_u64 dtf_u64;

typedef uint8_t  dtf_typeid;
typedef uint16_t dtf_nmemb;
typedef uint32_t dtf_size;

static_assert(sizeof(dtf_size) <= sizeof(ptrdiff_t), "dtf_size must never be larger than ptrdiff_t");

#define DTF_TYPEID_MAX UINT8_MAX
#define DTF_NMEMB_MAX UINT16_MAX
#define DTF_SIZE_MAX UINT32_MAX

// this should be pointless, in theory: all structs fields below are already packed except for the flexible array
// members This is here if some smarty pants compiler decides to add padding anyway
#pragma pack(push, 1)

#define DTF_LIST_HEAD                                                                                                  \
    dtf_size  nbytes;                                                                                                  \
    dtf_nmemb nitems;

struct dtf_array_header {
    DTF_LIST_HEAD

    dtf_typeid type;
};

struct dtf_array {
    struct dtf_array_header info;

    uint8_t data[];
};

struct dtf_tuple_header {
    DTF_LIST_HEAD
};

struct dtf_tuple {
    struct dtf_tuple_header header;

    // assume this is a list of dtf_value
    uint8_t data[];
};

struct dtf_pair_header {
    dtf_size nbytes;
};

struct dtf_pair {
    struct dtf_pair_header header;

    uint8_t data[];
};

struct dtf_bytes_header {
    dtf_size len;
};

struct dtf_bytes {
    struct dtf_bytes_header header;
    uint8_t                 data[];
};

struct dtf_error_header {
    uint16_t code;
};

struct dtf_error {
    struct dtf_error_header header;
    uint8_t                 msg[];
};

struct dtf_value_header {
    dtf_typeid type;
};

struct dtf_value {
    struct dtf_value_header header;
    uint8_t                 data[];
};

#define DTF_PAYLOAD_HEAD                                                                                               \
    uint32_t kind;                                                                                                     \
    uint32_t seq;

struct dtf_payload_head {
    DTF_PAYLOAD_HEAD
};

struct dtf_message_head {
    DTF_PAYLOAD_HEAD

    uint32_t data_len;
};

struct dtf_message {
    struct dtf_message_head head;

    uint8_t data[];
};

struct dtf_hello {
    DTF_PAYLOAD_HEAD

    uint32_t version;
};

struct dtf_bye {
    DTF_PAYLOAD_HEAD

    uint32_t reason;
};

#pragma pack(pop)

#if defined(_MSC_VER)
#pragma warning(default : 4200)
#endif

#endif // OQJPHQJJLF_DTF_TO_H
