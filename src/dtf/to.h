#if !defined(OQJPHQJJLF_DTF_TO_H)
#define OQJPHQJJLF_DTF_TO_H

#include <stdint.h>

#if defined(_MSC_VER)
#pragma warning(disable: 4200)
#endif

typedef uint8_t dtf_bool;
typedef int64_t dtf_int;
typedef double dtf_float;

// this should be pointless, in theory: all structs fields below are already packed.
// This is here if some smarty pants compiler decides to add padding anyway before the flexible array members
#pragma pack(push, 1)

struct dtf_array_header {
    uint16_t type;
    uint16_t len;
};

struct dtf_array {
    struct dtf_array_header info;

    uint8_t data[];
};

struct dtf_tuple_header {
    uint8_t nitems;
};

struct dtf_tuple {
    uint8_t nitems;

    // assume this is a list of dtf_value 
    uint8_t data[];
};

struct dtf_bytes_header {
    uint32_t len;
};

struct dtf_bytes {
    struct dtf_bytes_header header;
    uint8_t data[];
};

struct dtf_value_header {
    uint8_t type;
};

struct dtf_value {
    struct dtf_value_header header;
    uint8_t data[];
};

#define DTF_PAYLOAD_HEAD \
    uint32_t kind; \
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
    uint32_t id;
};

struct dtf_bye {
    DTF_PAYLOAD_HEAD
};

#pragma pack(pop)

#endif // OQJPHQJJLF_DTF_TO_H
