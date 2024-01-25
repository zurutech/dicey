#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <dicey/errors.h>
#include <dicey/value.h>
#include <dicey/views.h>

#include <dicey/internal/data-info.h>

#include "dicey/type.h"
#include "dtf/dtf.h"

#include "util.h"

static enum dicey_error value_get_list(const struct dicey_value *const value, struct dicey_list *const dest) {
    assert(value && dest);

    *dest = (struct dicey_list) {
        ._type = value->_data.list.inner_type,
        ._data = value->_data.list.data,
    };

    return DICEY_OK;
}

bool dicey_iterator_has_next(const struct dicey_iterator iter) {
    return iter._data.len > 0;
}

enum dicey_error dicey_iterator_next(struct dicey_iterator *const iter, struct dicey_value *const dest) {
    assert(iter && dest);

    if (!dicey_iterator_has_next(*iter)) {
        return DICEY_ENODATA;
    }

    struct dicey_view view = iter->_data;
    struct dtf_probed_value probed_value = {.type = iter->_type};
    
    const ptrdiff_t read_bytes = iter->_type == DICEY_VARIANT_ID 
        ? dtf_value_probe(&view, &probed_value) 
        : dtf_value_probe_as(iter->_type, &view, &probed_value.data);

    if (read_bytes < 0) {
        return read_bytes;
    }

    assert(iter->_type == DICEY_VARIANT_ID || iter->_type == probed_value.type);

    *dest = (struct dicey_value) {
        ._type = probed_value.type,
        ._data = probed_value.data,
    };

    iter->_data = view;

    return DICEY_OK;    
}

struct dicey_iterator dicey_list_iter(const struct dicey_list *const list) {
    assert(list);

    return (struct dicey_iterator) {
        ._type = list->_type,
        ._data = list->_data,
    };
}

int dicey_list_type(const struct dicey_list *const list) {
    assert(list);

    return list->_type;
}

bool dicey_errmsg_is_valid(const struct dicey_errmsg msg) {
    return msg.message;
}

bool dicey_selector_is_valid(const struct dicey_selector selector) {
    return selector.trait && selector.elem;
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

bool dicey_type_is_container(const enum dicey_type type) {
    switch (type) {
    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_PAIR:
    case DICEY_TYPE_TUPLE:
        return true;

    default:
        return false;
    }
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
    
    case DICEY_TYPE_BYTE:
        return "byte";

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

enum dicey_type dicey_value_get_type(const struct dicey_value *const value) {
    return value->_type;
}

#define DICEY_VALUE_GET_IMPL_TRIVIAL(NAME, TYPE, DICEY_TYPE, FIELD) \
    enum dicey_error dicey_value_get_##NAME(const struct dicey_value *const value, TYPE *const dest) { \
        assert(value && dest);                                                                         \
        if (dicey_value_get_type(value) != DICEY_TYPE) {                                               \
            return DICEY_EVALUE_TYPE_MISMATCH;                                                         \
        }                                                                                              \
        *dest = value->_data.FIELD;                                                                     \
        return DICEY_OK;                                                                               \
    }

enum dicey_error dicey_value_get_array(const struct dicey_value *const value, struct dicey_list *const dest) {
    return value_get_list(value, dest);
}

DICEY_VALUE_GET_IMPL_TRIVIAL(bool, bool, DICEY_TYPE_BOOL, boolean)
DICEY_VALUE_GET_IMPL_TRIVIAL(byte, uint8_t, DICEY_TYPE_BYTE, byte)

enum dicey_error dicey_value_get_bytes(
    const struct dicey_value *const value,
    const uint8_t **const dest,
    size_t *const nbytes
) {
    assert(value && dest && nbytes);

    if (dicey_value_get_type(value) != DICEY_TYPE_BYTES) {
        return DICEY_EVALUE_TYPE_MISMATCH;
    }

    *dest = value->_data.bytes.data;
    *nbytes = value->_data.bytes.len;

    return DICEY_OK;
}

DICEY_VALUE_GET_IMPL_TRIVIAL(error, struct dicey_errmsg, DICEY_TYPE_ERROR, error)
DICEY_VALUE_GET_IMPL_TRIVIAL(float, double, DICEY_TYPE_FLOAT, floating)

DICEY_VALUE_GET_IMPL_TRIVIAL(i16, int16_t, DICEY_TYPE_INT16, i16)
DICEY_VALUE_GET_IMPL_TRIVIAL(i32, int32_t, DICEY_TYPE_INT32, i32)
DICEY_VALUE_GET_IMPL_TRIVIAL(i64, int64_t, DICEY_TYPE_INT64, i64)

DICEY_VALUE_GET_IMPL_TRIVIAL(path, const char*, DICEY_TYPE_PATH, str)

enum dicey_error dicey_value_get_pair(const struct dicey_value *const value, struct dicey_pair *const dest) {
    assert(value && dest);

    if (dicey_value_get_type(value) != DICEY_TYPE_PAIR) {
        return DICEY_EVALUE_TYPE_MISMATCH;
    }

    // hack: craft a tuple and use it to get the pair, given that in memory is identical to (vv) after the header
    struct dicey_list tuple = {
        ._type = DICEY_VARIANT_ID,
        ._nitems = 2U,
        ._data = value->_data.list.data,
    };

    struct dicey_iterator iter = dicey_list_iter(&tuple);

    struct dicey_value *const items[] = { &dest->first, &dest->second, NULL };

    for (struct dicey_value *const *item = items; *item; ++item) {
        const enum dicey_error err = dicey_iterator_next(&iter, *item);
        assert(!err);

        DICEY_UNUSED(err); // silence unused warning, damn you MSVC
    }

    return DICEY_OK;
}

DICEY_VALUE_GET_IMPL_TRIVIAL(selector, struct dicey_selector, DICEY_TYPE_SELECTOR, selector)
DICEY_VALUE_GET_IMPL_TRIVIAL(str, const char*, DICEY_TYPE_STR, str)

enum dicey_error dicey_value_get_tuple(const struct dicey_value *const value, struct dicey_list *const dest) {
    assert(value && dest);

    if (dicey_value_get_type(value) != DICEY_TYPE_TUPLE) {
        return DICEY_EVALUE_TYPE_MISMATCH;
    }

    return value_get_list(value, dest);
}

DICEY_VALUE_GET_IMPL_TRIVIAL(u16, uint16_t, DICEY_TYPE_UINT16, u16)
DICEY_VALUE_GET_IMPL_TRIVIAL(u32, uint32_t, DICEY_TYPE_UINT32, u32)
DICEY_VALUE_GET_IMPL_TRIVIAL(u64, uint64_t, DICEY_TYPE_UINT64, u64)
