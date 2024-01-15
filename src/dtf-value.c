#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "dtf-to.h"
#include "dtf.h"
#include "util.h"

enum item_write_policy {
    ITEM_WRITE_POLICY_EXACT,
    ITEM_WRITE_POLICY_VARIANT,
};

static uint32_t header_sizeof(const enum dtf_type type) {
    switch (type) {
    default:
        return 0;

    case DTF_TYPE_ARRAY:
        return sizeof(struct dtf_array_header);

    case DTF_TYPE_TUPLE:
        return sizeof(struct dtf_tuple_header);

    case DTF_TYPE_BYTES:
    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
        return sizeof(struct dtf_bytes_header);
    }
}

static ptrdiff_t item_size(const struct dtf_item *item, enum item_write_policy policy);

static bool items_size(
    ptrdiff_t *const dest,
    const struct dtf_item *const items,
    const size_t nitems,
    const enum item_write_policy policy
) {
    const struct dtf_item *const end = items + nitems;

    for (const struct dtf_item *item = items; item < end; ++item) {
        const ptrdiff_t cur_size = item_size(item, policy);

        if (cur_size < 0 || !dutl_ssize_add(dest, *dest, cur_size)) {
            return false;
        }
    }

    return true;
}

static ptrdiff_t item_write(struct dicey_view_mut *dest, const struct dtf_item *item, enum item_write_policy policy);
    
static ptrdiff_t items_write(
    struct dicey_view_mut *const dest,
    const struct dtf_item *const items,
    const size_t nitems,
    const enum item_write_policy policy
) {
    const struct dtf_item *const end = items + nitems;

    for (const struct dtf_item *item = items; item < end; ++item) {
        const ptrdiff_t write_res = item_write(dest, item, policy);

        if (write_res < 0) {
            return write_res;
        }
    }

    return DICEY_OK;
}

static ptrdiff_t array_item_size(const struct dtf_array_item array) {
    ptrdiff_t size = header_sizeof(DTF_TYPE_ARRAY);

    if (!dtf_type_is_valid(array.type)) {
        return DICEY_EINVAL;
    }

    ptrdiff_t item_size = dtf_type_size(array.type);

    if (item_size < 0) {
        return item_size;
    }

    if (item_size == DTF_SIZE_DYNAMIC) {
        if (!items_size(&size, array.elems, array.len, ITEM_WRITE_POLICY_EXACT)) {
            return DICEY_EOVERFLOW;
        }
    } else {
        // My mystical divination skills lead me to assert that we'll never overflow a ptrdiff_t by adding
        // a uint16_t multiplied a trivial sizeof to it. If this ever happens, just know that I'm truly sorry.
        size += item_size * array.len;
    }

    return size;
}

static ptrdiff_t array_write(struct dicey_view_mut *const dest, const struct dtf_array_item array) {
    struct dicey_view header = {
        .data = &(struct dtf_array_header) { .type = array.type, .len = array.len },
        .len = sizeof(struct dtf_array_header),
    };

    ptrdiff_t write_res = dicey_view_mut_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    const ptrdiff_t size = dtf_type_size(array.type);
    assert(size >= 0);

    if (!size) {
        return DICEY_OK;
    }

    return items_write(dest, array.elems, array.len, ITEM_WRITE_POLICY_EXACT);
}

static ptrdiff_t bool_write(struct dicey_view_mut *const dest, const dtf_bool value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t bytes_item_size(const struct dtf_bytes_item bytes) {
    ptrdiff_t size = header_sizeof(DTF_TYPE_BYTES);

    if (!dutl_ssize_add(&size, size, bytes.len)) {
        return DICEY_EOVERFLOW;
    }

    return size;
}

static ptrdiff_t bytes_write(struct dicey_view_mut *const dest, const struct dtf_bytes_item bytes) {
    struct dicey_view header = {
        .data = &(struct dtf_bytes_header) { .len = bytes.len },
        .len = sizeof(struct dtf_bytes_header),
    };

    ptrdiff_t write_res = dicey_view_mut_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    struct dicey_view data = {
        .data = bytes.data,
        .len = bytes.len,
    };

    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t float_write(struct dicey_view_mut *const dest, const dtf_float value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t int_write(struct dicey_view_mut *const dest, const dtf_int value) {
    struct dicey_view data = {
        .data = &value,
        .len = sizeof value,
    };

    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t pair_item_size(const struct dtf_pair_item pair) {
    ptrdiff_t size = 0;

    const struct dtf_item *const items[] = { pair.first, pair.second };
    const struct dtf_item *const *const end = items + sizeof items / sizeof *items;

    for (const struct dtf_item *const *item = items; item < end; ++item) {
        const ptrdiff_t it_size = item_size(*item, ITEM_WRITE_POLICY_EXACT);

        if (it_size < 0) {
            return it_size;
        }

        if (!dutl_ssize_add(&size, size, it_size)) {
            return DICEY_EOVERFLOW;
        }
    }

    return size;
}

static ptrdiff_t pair_write(struct dicey_view_mut *const dest, const struct dtf_pair_item pair) {
    const struct dtf_item *const items[] = { pair.first, pair.second };
    const struct dtf_item *const *const end = items + sizeof items / sizeof *items;

    for (const struct dtf_item *const *item = items; item < end; ++item) {
        const ptrdiff_t write_res = item_write(dest, *item, ITEM_WRITE_POLICY_VARIANT);

        if (write_res < 0) {
            return write_res;
        }
    }

    return DICEY_OK;
}

static ptrdiff_t tuple_item_size(const struct dtf_tuple_item tuple) {
    ptrdiff_t size = header_sizeof(DTF_TYPE_TUPLE);

    if (!items_size(&size, tuple.elems, tuple.nitems, ITEM_WRITE_POLICY_VARIANT)) {
        return DICEY_EOVERFLOW;
    }

    return size;
}

static ptrdiff_t tuple_write(struct dicey_view_mut *const dest, const struct dtf_tuple_item tuple) {
    struct dicey_view header = {
        .data = &(struct dtf_tuple_header) { .nitems = tuple.nitems },
        .len = sizeof(struct dtf_tuple_header),
    };

    ptrdiff_t write_res = dicey_view_mut_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    return items_write(dest, tuple.elems, tuple.nitems, ITEM_WRITE_POLICY_VARIANT);
}

static ptrdiff_t value_header_write(struct dicey_view_mut *const dest, const struct dtf_item *const value) {
    const struct dicey_view header = {
        .data = &(struct dtf_value_header) { .type = value->type },
        .len = sizeof(struct dtf_value_header),
    };

    ptrdiff_t write_res = dicey_view_mut_write(dest, header);
    if (write_res < 0) {
        return write_res;
    }

    return write_res;
}

static ptrdiff_t item_size(const struct dtf_item *const item, const enum item_write_policy policy) {
    ptrdiff_t fixed_size = dtf_type_size(item->type);
    
    if (fixed_size != DTF_SIZE_DYNAMIC) {
        return fixed_size;
    }

    // start from zero and compute the dynamic size of the item
    fixed_size = 0;

    if (policy == ITEM_WRITE_POLICY_VARIANT) {
        if (!dutl_ssize_add(&fixed_size, fixed_size, (ptrdiff_t) sizeof(struct dtf_value_header))) {
            return DICEY_EOVERFLOW;
        }
    }

    ptrdiff_t item_size = 0;

    switch (item->type) {
    default:
        assert(false);
    
    case DTF_TYPE_INVALID:
        return DICEY_EINVAL;

    case DTF_TYPE_ARRAY: 
        item_size = array_item_size(item->array);
        break;

    case DTF_TYPE_PAIR:
        item_size = pair_item_size(item->pair);
        break;

    case DTF_TYPE_TUPLE:
        item_size = tuple_item_size(item->tuple);
        break;

    case DTF_TYPE_BYTES:
        item_size = bytes_item_size(item->bytes);
        break;

    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
        item_size = dutl_zstring_size(item->str);
        break;

    case DTF_TYPE_SELECTOR:
        item_size = dicey_selector_size(item->selector);
        break;
    }

    if (item_size < 0) {
        return item_size;
    }

    if (!dutl_ssize_add(&fixed_size, fixed_size, item_size)) {
        return DICEY_EOVERFLOW;
    }

    return fixed_size;
}

static ptrdiff_t item_write(
    struct dicey_view_mut *const dest,
    const struct dtf_item *const item,
    const enum item_write_policy policy
) {
    if (!dtf_type_is_valid(item->type)) {
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

    case DTF_TYPE_UNIT:
        return DICEY_OK;

    case DTF_TYPE_BOOL:
        return bool_write(dest, item->boolean);

    case DTF_TYPE_FLOAT:
        return float_write(dest, item->floating);

    case DTF_TYPE_INT:
        return int_write(dest, item->integer);

    case DTF_TYPE_ARRAY:
        return array_write(dest, item->array);

    case DTF_TYPE_PAIR:
        return pair_write(dest, item->pair);

    case DTF_TYPE_TUPLE:
        return tuple_write(dest, item->tuple);

    case DTF_TYPE_BYTES:
        return bytes_write(dest, item->bytes);

    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
        return dicey_view_mut_write_zstring(dest, item->str);

    case DTF_TYPE_SELECTOR:
        return dicey_view_mut_write_selector(dest, item->selector);
    }
}

bool dtf_type_is_valid(const enum dtf_type type) {
    switch (type) {
    default:
        return false;

    case DTF_TYPE_UNIT:
    case DTF_TYPE_BOOL:
    case DTF_TYPE_FLOAT:
    case DTF_TYPE_INT:
    case DTF_TYPE_ARRAY:
    case DTF_TYPE_PAIR:
    case DTF_TYPE_TUPLE:
    case DTF_TYPE_BYTES:
    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
    case DTF_TYPE_SELECTOR:
        return true;
    }
}

const char* dtf_type_name(const enum dtf_type type) {
    switch (type) {
    default:
        assert(false);
        return NULL;
    
    case DTF_TYPE_INVALID:
        return "invalid";
    
    case DTF_TYPE_UNIT:
        return "unit";
    
    case DTF_TYPE_BOOL:
        return "bool";
    
    case DTF_TYPE_FLOAT:
        return "float";

    case DTF_TYPE_INT:
        return "int";
    
    case DTF_TYPE_ARRAY:
        return "array";

    case DTF_TYPE_PAIR:
        return "pair";

    case DTF_TYPE_TUPLE:
        return "tuple";

    case DTF_TYPE_BYTES:
        return "bytes";

    case DTF_TYPE_STR:
        return "str";

    case DTF_TYPE_PATH:
        return "path";

    case DTF_TYPE_SELECTOR:
        return "selector";
    }
}

ptrdiff_t dtf_type_size(const enum dtf_type type) {
    switch (type) {
    default:
        assert(false);

    case DTF_TYPE_INVALID:
        return DICEY_EINVAL;

    case DTF_TYPE_UNIT:
        return 0;
        
    case DTF_TYPE_BOOL:
        return sizeof(dtf_bool);

    case DTF_TYPE_FLOAT:
        return sizeof(dtf_float);

    case DTF_TYPE_INT:
        return sizeof(dtf_int);
    
    case DTF_TYPE_ARRAY:
    case DTF_TYPE_PAIR:
    case DTF_TYPE_TUPLE:
    case DTF_TYPE_BYTES:
    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
    case DTF_TYPE_SELECTOR:
        return DTF_SIZE_DYNAMIC;
    }
}

ptrdiff_t dtf_value_estimate_size(const struct dtf_item *const item) {
    return item_size(item, ITEM_WRITE_POLICY_VARIANT);
}

struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dtf_item *const item) {
    ptrdiff_t size = dtf_value_estimate_size(item);

    if (size < 0) {
        return (struct dtf_valueres) { .result = size };
    }

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, (size_t) size);

    if (alloc_res < 0) {
        return (struct dtf_valueres) { .result = alloc_res, .size = (size_t) size };
    }

    struct dtf_value *const dval = dest.data;

    const ptrdiff_t write_res = item_write(&dest, item, ITEM_WRITE_POLICY_VARIANT);

    if (write_res < 0) {
        if (alloc_res != DICEY_OK) {
            free(dest.data);
        }

        return (struct dtf_valueres) { .result = write_res, .size = (size_t) size };
    }
    
    // return the size of the value into result, too. This will be 0 if no allocation was needed, or size otherwise
    return (struct dtf_valueres) { .result = alloc_res, .size = (size_t) size, .value = dval };
}
