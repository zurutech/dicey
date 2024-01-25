#if !defined(PTDFNAAZWS_TYPE_H)
#define PTDFNAAZWS_TYPE_H

#include <stdint.h>

#include "errors.h"
#include "views.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef uint8_t dicey_bool;
typedef uint8_t dicey_byte;

typedef int16_t dicey_i16;
typedef int32_t dicey_i32;
typedef int64_t dicey_i64;
typedef uint16_t dicey_u16;
typedef uint32_t dicey_u32;
typedef uint64_t dicey_u64;

struct dicey_errmsg {
    uint16_t code;
    const char *message;
};

bool dicey_errmsg_is_valid(struct dicey_errmsg msg);

typedef double dicey_float;

struct dicey_selector {
    const char *trait;
    const char *elem;
};

bool dicey_selector_is_valid(struct dicey_selector selector);
ptrdiff_t dicey_selector_size(struct dicey_selector sel);
ptrdiff_t dicey_selector_write(struct dicey_selector sel, struct dicey_view_mut *dest);

enum dicey_type {
    DICEY_TYPE_INVALID = 0,

    DICEY_TYPE_UNIT  = '$',

    DICEY_TYPE_BOOL  = 'b',
    DICEY_TYPE_BYTE    = 'c',

    DICEY_TYPE_FLOAT = 'f',

    DICEY_TYPE_INT16   = 'n',    
    DICEY_TYPE_INT32   = 'i',
    DICEY_TYPE_INT64   = 'x',

    DICEY_TYPE_UINT16  = 'q',
    DICEY_TYPE_UINT32  = 'u',
    DICEY_TYPE_UINT64  = 't',

    DICEY_TYPE_ARRAY = '[', // an array of elements
    DICEY_TYPE_TUPLE = '(', // an array of variable elements
    DICEY_TYPE_PAIR  = '{', // specialised tuple of two elements
    DICEY_TYPE_BYTES = 'y', // optimized array of bytes
    DICEY_TYPE_STR   = 's', // alias for a null terminated byte array

    DICEY_TYPE_PATH     = '@', // alias for str
    DICEY_TYPE_SELECTOR = '%', // an optimized tuple of two strings

    DICEY_TYPE_ERROR = 'e',
};

bool dicey_type_is_container(enum dicey_type type);
bool dicey_type_is_valid(enum dicey_type type);
const char* dicey_type_name(enum dicey_type type);

#define DICEY_VARIANT_ID ((uint16_t) 'v')

#if defined(__cplusplus)
}
#endif

#endif // PTDFNAAZWS_TYPE_H
