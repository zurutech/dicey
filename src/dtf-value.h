#if !defined(CNHZVJKDMF_DTF_VALUE_H)
#define CNHZVJKDMF_DTF_VALUE_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

enum dtf_type {
    DTF_TYPE_INVALID = 0,
    DTF_TYPE_UNIT,
    DTF_TYPE_BOOL,
    DTF_TYPE_BYTE,
    DTF_TYPE_FLOAT,
    DTF_TYPE_INT,

    DTF_TYPE_ARRAY = 0x10,
    DTF_TYPE_TUPLE,     
    DTF_TYPE_BYTES,     // optimized array of bytes
    DTF_TYPE_STR,       // alias for a null terminated byte array
    DTF_TYPE_PATH,      // alias for str
    DTF_TYPE_SELECTOR,  // an optimized tuple of two strings
    DTF_TYPE_VARIANT,
};

typedef uint8_t dtf_bool;
typedef uint8_t dtf_byte;
typedef int64_t dtf_int;
typedef double dtf_float;

struct dtf_array_info {
    uint16_t type;
    uint16_t len;
};

struct dtf_array {
    struct dtf_array_info info;

    uint8_t data[];
};

struct dtf_tuple_info {
    uint8_t len;
    uint8_t types[];
};

struct dtf_tuple {
    uint8_t len;

    // assume this represents the type list + the data
    uint8_t data[];
};

struct dtf_bytes {
    uint32_t len;
    uint8_t data[];
};

struct dtf_value {
    uint8_t type;
    uint8_t data[];
};

#define DTF_SIZE_DYNAMIC PTRDIFF_MAX

struct dtf_array_elem {
    union {
        dtf_bool boolean;
        dtf_byte byte;
        dtf_float floating;
        dtf_int integer;
        struct dicey_view bytes;
        struct dicey_selector selector;
        struct {
            uint16_t type;
            uint16_t size;
            void *elem;
        } dynamic;
    };
};

struct dtf_valueres {
    int result;
    size_t size;
    struct dtf_value* value;
};

struct dtf_valueres dtf_value_new_unit(struct dicey_view_mut dest);
struct dtf_valueres dtf_value_new_bool(struct dicey_view_mut dest, dtf_bool value);
struct dtf_valueres dtf_value_new_byte(struct dicey_view_mut dest, dtf_byte value);
struct dtf_valueres dtf_value_new_float(struct dicey_view_mut dest, dtf_float value);
struct dtf_valueres dtf_value_new_int(struct dicey_view_mut dest, dtf_int value);

struct dtf_valueres dtf_value_new_bytes(struct dicey_view_mut dest, struct dicey_view src);
struct dtf_valueres dtf_value_new_str(struct dicey_view_mut dest, const char *src);
struct dtf_valueres dtf_value_new_path(struct dicey_view_mut dest, const char *src);
struct dtf_valueres dtf_value_new_selector(struct dicey_view_mut dest, struct dicey_selector src);
struct dtf_valueres dtf_value_new_tuple(struct dicey_view_mut dest, uint8_t len, const uint8_t *types, const struct dtf_array_elem *elems);

struct dtf_valueres dtf_value_array_begin(struct dicey_view_mut dest, enum dtf_type type);
struct dtf_valueres dtf_value_array_elem(struct dicey_view_mut dest, struct dtf_value* base, struct dtf_array_elem elem);
struct dtf_valueres dtf_value_array_end(struct dicey_view_mut dest, struct dtf_value* base);

struct dtf_valueres dtf_value_new_array(struct dicey_view_mut dest, struct dtf_array_info info, struct dtf_array_elem *data);

ptrdiff_t dtf_value_size(enum dtf_type type);

static inline bool dft_type_is_trivial(enum dtf_type type) {
    return type <= DTF_TYPE_BYTE;
}

const char* dtf_type_name(enum dtf_type type);
ptrdiff_t dtf_type_size(enum dtf_type type);

#endif // CNHZVJKDMF_DTF_VALUE_H
