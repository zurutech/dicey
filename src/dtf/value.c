#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "payload.h"
#include "to.h"
#include "writer.h"

#include "value.h"

enum item_write_policy {
    ITEM_WRITE_POLICY_EXACT,
    ITEM_WRITE_POLICY_VARIANT,
};

static ptrdiff_t item_write(struct dtf_bytes_writer dest, const struct dicey_arg *item, enum item_write_policy policy);
    
static ptrdiff_t items_write(
    const struct dtf_bytes_writer dest,
    const struct dicey_arg *const items,
    const size_t nitems,
    const enum item_write_policy policy
) {
    const struct dicey_arg *const end = items + nitems;

    for (const struct dicey_arg *item = items; item < end; ++item) {
        const ptrdiff_t write_res = item_write(dest, item, policy);

        if (write_res < 0) {
            return write_res;
        }
    }

    return DICEY_OK;
}

ptrdiff_t type_size(const enum dicey_type type) {
    switch (type) {
    default:
        assert(false);

    case DICEY_TYPE_INVALID:
        return DICEY_EINVAL;

    case DICEY_TYPE_UNIT:
        return 0;
        
    case DICEY_TYPE_BOOL:
        return sizeof(dtf_bool);

    case DICEY_TYPE_FLOAT:
        return sizeof(dtf_float);

    case DICEY_TYPE_INT:
        return sizeof(dtf_int);
    
    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_PAIR:
    case DICEY_TYPE_TUPLE:
    case DICEY_TYPE_BYTES:
    case DICEY_TYPE_STR:
    case DICEY_TYPE_PATH:
    case DICEY_TYPE_SELECTOR:
    case DICEY_TYPE_ERROR:
        return DTF_SIZE_DYNAMIC;
    }
}

static ptrdiff_t array_write(const struct dtf_bytes_writer dest, const struct dicey_array_arg array) {
    struct dicey_view header = {
        .data = &(struct dtf_array_header) { .type = array.type, .len = array.nitems },
        .len = sizeof(struct dtf_array_header),
    };

    ptrdiff_t write_res = dtf_bytes_writer_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    const ptrdiff_t size = type_size(array.type);
    assert(size >= 0);

    if (!size) {
        return DICEY_OK;
    }

    return items_write(dest, array.elems, array.nitems, ITEM_WRITE_POLICY_EXACT);
}

static ptrdiff_t bool_write(const struct dtf_bytes_writer dest, const dtf_bool value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dtf_bytes_writer_write(dest, data);
}

static ptrdiff_t bytes_write(const struct dtf_bytes_writer dest, const struct dicey_bytes_arg bytes) {
    struct dicey_view header = {
        .data = &(struct dtf_bytes_header) { .len = bytes.len },
        .len = sizeof(struct dtf_bytes_header),
    };

    ptrdiff_t write_res = dtf_bytes_writer_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    struct dicey_view data = {
        .data = bytes.data,
        .len = bytes.len,
    };

    return dtf_bytes_writer_write(dest, data);
}

static ptrdiff_t error_write(const struct dtf_bytes_writer dest, const struct dicey_error_arg error) {
    struct dicey_view header = {
        .data = &(struct dtf_error_header) { .code = error.code },
        .len = sizeof(struct dtf_error_header),
    };

    ptrdiff_t write_res = dtf_bytes_writer_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    return dtf_bytes_writer_write_zstring(dest, error.message);
}

static ptrdiff_t float_write(const struct dtf_bytes_writer dest, const dtf_float value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dtf_bytes_writer_write(dest, data);
}

static ptrdiff_t int_write(const struct dtf_bytes_writer dest, const dtf_int value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dtf_bytes_writer_write(dest, data);
}

static ptrdiff_t pair_write(const struct dtf_bytes_writer dest, const struct dicey_pair_arg pair) {
    const struct dicey_arg *const items[] = { pair.first, pair.second };
    const struct dicey_arg *const *const end = items + sizeof items / sizeof *items;

    for (const struct dicey_arg *const *item = items; item < end; ++item) {
        const ptrdiff_t write_res = item_write(dest, *item, ITEM_WRITE_POLICY_VARIANT);

        if (write_res < 0) {
            return write_res;
        }
    }

    return DICEY_OK;
}

static ptrdiff_t tuple_write(const struct dtf_bytes_writer dest, const struct dicey_tuple_arg tuple) {
    struct dicey_view header = {
        .data = &(struct dtf_tuple_header) { .nitems = tuple.nitems },
        .len = sizeof(struct dtf_tuple_header),
    };

    ptrdiff_t write_res = dtf_bytes_writer_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    return items_write(dest, tuple.elems, tuple.nitems, ITEM_WRITE_POLICY_VARIANT);
}

static ptrdiff_t value_header_write(const struct dtf_bytes_writer dest, const struct dicey_arg *const value) {
    const struct dicey_view header = {
        .data = &(struct dtf_value_header) { .type = value->type },
        .len = sizeof(struct dtf_value_header),
    };

    ptrdiff_t write_res = dtf_bytes_writer_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    return write_res;
}

static ptrdiff_t item_write(
    const struct dtf_bytes_writer dest,
    const struct dicey_arg *const item,
    const enum item_write_policy policy
) {
    assert(dtf_bytes_writer_is_valid(dest) && item);
    
    if (!dicey_type_is_valid(item->type)) {
        return DICEY_EINVAL;
    }

    if (policy == ITEM_WRITE_POLICY_VARIANT) {
        const ptrdiff_t header_write_res = value_header_write(dest, item);

        if (header_write_res < 0) {
            return header_write_res;
        }
    }

    switch (item->type) {
    default:
        assert(false);

    case DICEY_TYPE_UNIT:
        return DICEY_OK;

    case DICEY_TYPE_BOOL:
        return bool_write(dest, item->boolean);

    case DICEY_TYPE_FLOAT:
        return float_write(dest, item->floating);

    case DICEY_TYPE_INT:
        return int_write(dest, item->integer);

    case DICEY_TYPE_ARRAY:
        return array_write(dest, item->array);

    case DICEY_TYPE_PAIR:
        return pair_write(dest, item->pair);

    case DICEY_TYPE_TUPLE:
        return tuple_write(dest, item->tuple);

    case DICEY_TYPE_BYTES:
        return bytes_write(dest, item->bytes);

    case DICEY_TYPE_STR:
    case DICEY_TYPE_PATH:
        return dtf_bytes_writer_write_zstring(dest, item->str);

    case DICEY_TYPE_SELECTOR:
        return dtf_bytes_writer_write_selector(dest, item->selector);
    
    case DICEY_TYPE_ERROR:
        return error_write(dest, item->error);
    }
}

ptrdiff_t dtf_value_estimate_size(const struct dicey_arg *const item) {
    ptrdiff_t size = 0;

    const struct dtf_bytes_writer sizer = dtf_bytes_writer_new_sizer(&size);

    const ptrdiff_t res = item_write(sizer, item, ITEM_WRITE_POLICY_VARIANT);

    return res < 0 ? res : size;
}

struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dicey_arg *const item) {
    ptrdiff_t size = dtf_value_estimate_size(item);

    if (size < 0) {
        return (struct dtf_valueres) { .result = size };
    }

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, (size_t) size);

    if (alloc_res < 0) {
        return (struct dtf_valueres) { .result = alloc_res, .size = (size_t) size };
    }

    struct dtf_value *const dval = dest.data;

    const struct dtf_bytes_writer writer = dtf_bytes_writer_new(&dest);

    const ptrdiff_t write_res = item_write(writer, item, ITEM_WRITE_POLICY_VARIANT);

    if (write_res < 0) {
        if (alloc_res != DICEY_OK) {
            free(dest.data);
        }

        return (struct dtf_valueres) { .result = write_res, .size = (size_t) size };
    }
    
    // return the size of the value into result, too. This will be 0 if no allocation was needed, or size otherwise
    return (struct dtf_valueres) { .result = alloc_res, .size = (size_t) size, .value = dval };
}

ptrdiff_t dtf_value_write_to(const struct dtf_bytes_writer writer, const struct dicey_arg *const item) {
    return item_write(writer, item, ITEM_WRITE_POLICY_VARIANT);
}
