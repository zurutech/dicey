#if !defined(CNHZVJKDMF_DTF_VALUE_H)
#define CNHZVJKDMF_DTF_VALUE_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#include "to.h"

enum dtf_type {
    DTF_TYPE_INVALID = 0,

    DTF_TYPE_UNIT,
    DTF_TYPE_BOOL,
    DTF_TYPE_FLOAT,
    DTF_TYPE_INT,

    DTF_TYPE_ARRAY = 0x10, // an array of idential elements
    DTF_TYPE_TUPLE,     // an array of variable elements
    DTF_TYPE_PAIR,      // specialised tuple of two elements
    DTF_TYPE_BYTES,     // optimized array of bytes
    DTF_TYPE_STR,       // alias for a null terminated byte array
    DTF_TYPE_PATH,      // alias for str
    DTF_TYPE_SELECTOR,  // an optimized tuple of two strings
};

#define DTF_SIZE_DYNAMIC PTRDIFF_MAX

struct dtf_item {
    enum dtf_type type;

    union {
        dtf_bool boolean;
        dtf_float floating;
        dtf_int integer;
        struct dtf_array_item {
            enum dtf_type type;
            uint16_t len;
            const struct dtf_item *elems;
        } array;
        struct dtf_tuple_item {
            uint8_t nitems;
            const struct dtf_item *elems;
        } tuple;
        struct dtf_pair_item {
            const struct dtf_item *first;
            const struct dtf_item *second;
        } pair;
        struct dtf_bytes_item {
            uint32_t len;
            const uint8_t *data;
        } bytes;
        const char *str;// for str, path
        struct dicey_selector selector;
    };
};

bool dtf_type_is_valid(enum dtf_type type);
const char* dtf_type_name(enum dtf_type type);
ptrdiff_t dtf_type_size(enum dtf_type type);

struct dtf_valueres {
    ptrdiff_t result;
    size_t size;
    struct dtf_value* value;
};

ptrdiff_t dtf_value_estimate_size(const struct dtf_item *item);
struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dtf_item *item);

#endif // CNHZVJKDMF_DTF_VALUE_H
