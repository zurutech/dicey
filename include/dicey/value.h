#include <stddef.h>
#if !defined(TOJAFCVDUG_VALUE_H)
#define TOJAFCVDUG_VALUE_H

#include <stdbool.h>
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

typedef double dicey_float;

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

bool dicey_type_is_valid(enum dicey_type type);
const char* dicey_type_name(enum dicey_type type);

struct dicey_array {
    enum dicey_type _type;

    struct dicey_view _data;
};

struct dicey_errmsg {
    uint16_t code;
    const char *message;
};

struct dicey_iterator {
    int8_t _type;

    struct dicey_view _data;
};

struct dicey_pair {
    enum dicey_type _type;

    struct dicey_view _data;
};

struct dicey_selector {
    const char *trait;
    const char *elem;
};

enum dicey_error dicey_selector_from(struct dicey_selector *sel, struct dicey_view *src);
ptrdiff_t dicey_selector_size(struct dicey_selector sel);
ptrdiff_t dicey_selector_write(struct dicey_selector sel, struct dicey_view_mut *dest);

struct dicey_tuple {
    enum dicey_type _type;

    struct dicey_view _data;
};

struct dicey_value {
    enum dicey_type _type;

    struct dicey_view _data;
};

enum dicey_type dicey_value_get_type(struct dicey_value value);

enum dicey_error dicey_value_get_bool(struct dicey_value value, bool *dest);
enum dicey_error dicey_value_get_byte(struct dicey_value value, uint8_t *dest);

enum dicey_error dicey_value_get_float(struct dicey_value value, double *dest);

enum dicey_error dicey_value_get_i16(struct dicey_value value, int16_t *dest);
enum dicey_error dicey_value_get_i32(struct dicey_value value, int32_t *dest);
enum dicey_error dicey_value_get_i64(struct dicey_value value, int64_t *dest);

enum dicey_error dicey_value_get_u16(struct dicey_value value, uint16_t *dest);
enum dicey_error dicey_value_get_u32(struct dicey_value value, uint32_t *dest);
enum dicey_error dicey_value_get_u64(struct dicey_value value, uint64_t *dest);

enum dicey_error dicey_value_get_array(struct dicey_value value, struct dicey_array *dest);
enum dicey_error dicey_value_get_bytes(struct dicey_value value, const void **dest, size_t *nbytes);
enum dicey_error dicey_value_get_error(struct dicey_value value, struct dicey_errmsg *dest);
enum dicey_error dicey_value_get_str(struct dicey_value value, const char **dest);
enum dicey_error dicey_value_get_path(struct dicey_value value, const char **dest);
enum dicey_error dicey_value_get_selector(struct dicey_value value, struct dicey_selector *dest);
enum dicey_error dicey_value_get_tuple(struct dicey_value value, struct dicey_tuple *dest);

enum dicey_error dicey_value_get_error(struct dicey_value value, struct dicey_errmsg *dest);

static inline bool dicey_value_is(const struct dicey_value value, const enum dicey_type type) {
    return value._type == type;
}

static inline bool dicey_value_is_valid(const struct dicey_value value) {
    return dicey_type_is_valid(value._type) && dicey_view_is_valid(value._data);
}

#if defined(__cplusplus)
}
#endif


#endif // TOJAFCVDUG_VALUE_H
