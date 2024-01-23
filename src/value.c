#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <dicey/errors.h>
#include <dicey/value.h>
#include <dicey/views.h>

#include "dtf/dtf.h"

#include "util.h"

static enum dicey_error value_get_full(
    const enum dicey_type type,
    const struct dicey_value value,
    union dtf_probed_data *const probed_data 
) {
    if (!dicey_value_is(value, type)) {
        return DICEY_EINVAL;
    }

    struct dicey_view view = value._data;
    const ptrdiff_t probe_val = dtf_value_probe_as(type, &view, probed_data);
    if (probe_val < 0) {
        return probe_val;
    }

    assert(dicey_view_is_empty(view));

    return DICEY_OK;
}

ptrdiff_t dicey_selector_size(const struct dicey_selector selector) {
    const ptrdiff_t trait_len = dutl_zstring_size(selector.trait); 
    if (trait_len < 0) {
        return trait_len;
    }

    const ptrdiff_t elem_len = dutl_zstring_size(selector.elem);
    if (elem_len < 0) {
        return elem_len;
    }

    ptrdiff_t result = 0;
    if (!dutl_checked_add(&result, trait_len, elem_len)) {
        return DICEY_EOVERFLOW;
    }

    return trait_len + elem_len;
}

ptrdiff_t dicey_selector_write(struct dicey_selector sel, struct dicey_view_mut *const dest) {
    if (!dest || !dest->data || !sel.trait || !sel.elem) {
        return DICEY_EINVAL;
    }

    const ptrdiff_t trait_len = dutl_zstring_size(sel.trait);
    if (trait_len < 0) {
        return trait_len;
    }

    const ptrdiff_t elem_len = dutl_zstring_size(sel.elem);
    if (elem_len < 0) {
        return elem_len;
    }

    struct dicey_view chunks[] = {
        (struct dicey_view) { .data = (void*) sel.trait, .len = (size_t) trait_len },
        (struct dicey_view) { .data = (void*) sel.elem, .len = (size_t) elem_len },
    };

    return dicey_view_mut_write_chunks(dest, chunks, sizeof chunks / sizeof *chunks);
}

bool dicey_type_is_valid(const enum dicey_type type) {
    switch (type) {
    case DICEY_TYPE_UNIT:
    
    case DICEY_TYPE_BOOL:
    case DICEY_TYPE_BYTE:

    case DICEY_TYPE_FLOAT:

    case DICEY_TYPE_INT16:
    case DICEY_TYPE_INT32:
    case DICEY_TYPE_INT64:

    case DICEY_TYPE_UINT16:
    case DICEY_TYPE_UINT32:
    case DICEY_TYPE_UINT64:

    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_TUPLE:
    case DICEY_TYPE_PAIR:
    case DICEY_TYPE_BYTES:
    case DICEY_TYPE_STR:

    case DICEY_TYPE_PATH:
    case DICEY_TYPE_SELECTOR:

    case DICEY_TYPE_ERROR:
        return true;

    default:
        return false;
    }
}

const char* dicey_type_name(const enum dicey_type type) {
    switch (type) {
    default:
        assert(false);
        return NULL;
    
    case DICEY_TYPE_INVALID:
        return "invalid";
    
    case DICEY_TYPE_UNIT:
        return "unit";
    
    case DICEY_TYPE_BOOL:
        return "bool";
    
    case DICEY_TYPE_FLOAT:
        return "float";

    case DICEY_TYPE_INT32:
        return "int";
    
    case DICEY_TYPE_ARRAY:
        return "array";

    case DICEY_TYPE_PAIR:
        return "pair";

    case DICEY_TYPE_TUPLE:
        return "tuple";

    case DICEY_TYPE_BYTES:
        return "bytes";

    case DICEY_TYPE_STR:
        return "str";

    case DICEY_TYPE_PATH:
        return "path";

    case DICEY_TYPE_SELECTOR:
        return "selector";

    case DICEY_TYPE_ERROR:
        return "error";
    }
}

enum dicey_type dicey_value_get_type(const struct dicey_value value) {
    return value._type;
}

#define DICEY_VALUE_GET_IMPL_TRIVIAL(NAME, TYPE, DICEY_TYPE, FIELD) \
    enum dicey_error dicey_value_get_##NAME(const struct dicey_value value, TYPE *const dest) { \
        assert(dicey_value_is_valid(value) && dest);                                            \
        union dtf_probed_data probed_data = {0};                                                \
        const enum dicey_error err = value_get_full(DICEY_TYPE_BOOL, value, &probed_data);      \
        if (err) {                                                                              \
            return err;                                                                         \
        }                                                                                       \
        *dest = probed_data.FIELD;                                                              \
        return DICEY_OK;                                                                        \
    }

DICEY_VALUE_GET_IMPL_TRIVIAL(bool, bool, DICEY_TYPE_BOOL, boolean)
DICEY_VALUE_GET_IMPL_TRIVIAL(byte, uint8_t, DICEY_TYPE_BYTE, byte)

DICEY_VALUE_GET_IMPL_TRIVIAL(float, double, DICEY_TYPE_FLOAT, floating)

DICEY_VALUE_GET_IMPL_TRIVIAL(i16, int16_t, DICEY_TYPE_INT16, i16)
DICEY_VALUE_GET_IMPL_TRIVIAL(i32, int32_t, DICEY_TYPE_INT32, i32)
DICEY_VALUE_GET_IMPL_TRIVIAL(i64, int64_t, DICEY_TYPE_INT64, i64)

DICEY_VALUE_GET_IMPL_TRIVIAL(u16, uint16_t, DICEY_TYPE_UINT16, u16)
DICEY_VALUE_GET_IMPL_TRIVIAL(u32, uint32_t, DICEY_TYPE_UINT32, u32)
DICEY_VALUE_GET_IMPL_TRIVIAL(u64, uint64_t, DICEY_TYPE_UINT64, u64)

enum dicey_error dicey_value_get_array(struct dicey_value value, struct dicey_array *dest);

enum dicey_error dicey_value_get_bytes(struct dicey_value value, const void **dest, size_t *nbytes) {
    assert(dest && nbytes);

    union dtf_probed_data probed_data = {0};
    const enum dicey_error err = value_get_full(DICEY_TYPE_BYTES, value, &probed_data);
    if (err) {
        return err;
    }

    *dest = probed_data.bytes.data;
    *nbytes = probed_data.bytes.len;

    return DICEY_OK;
}

DICEY_VALUE_GET_IMPL_TRIVIAL(error, struct dicey_errmsg, DICEY_TYPE_ERROR, error)

DICEY_VALUE_GET_IMPL_TRIVIAL(str, const char*, DICEY_TYPE_STR, str)
DICEY_VALUE_GET_IMPL_TRIVIAL(path, const char*, DICEY_TYPE_PATH, str)
DICEY_VALUE_GET_IMPL_TRIVIAL(selector, struct dicey_selector, DICEY_TYPE_SELECTOR, selector)
