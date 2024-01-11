#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "dtf.h"
#include "util.h"

static int size_add(size_t *const dest, const size_t a, const size_t b) {
    const size_t sum = a + b;

    if (sum < a) {
        return DICEY_EOVERFLOW;
    }

    *dest = sum;

    return 0;
}

static int sizes_add(size_t *const dest, const struct dicey_view *const chunks, const size_t nchunks) {
    const struct dicey_view *const end = chunks + nchunks;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        if (size_add(dest, *dest, chunk->len) < 0) {
            return DICEY_EOVERFLOW;
        }
    }

    return 0;
}

static void value_set_type(struct dtf_value *const value, const enum dtf_type type) {
    const uint8_t type_tag = type;

    memcpy(&value->type, &type_tag, sizeof type_tag);
}

static struct dtf_valueres value_new_many(
    const struct dicey_view_mut dest,
    const enum dtf_type kind,
    const struct dicey_view *const chunks,
    const size_t nchunks
) {
    size_t size = sizeof(struct dtf_value);

    if (sizes_add(&size, chunks, nchunks) < 0) {
        return (struct dtf_valueres) { .result = DICEY_EOVERFLOW };
    }

    struct dtf_value *dval = dest.data;

    if (dest.len < size) {
        if (dest.data) {
            return (struct dtf_valueres) { .result = DICEY_EAGAIN };
        } 

        dval = calloc(size, 1U);
        if (!dval) {
            return (struct dtf_valueres) { .result = DICEY_ENOMEM };
        }
    }

    value_set_type(dval, kind);

    dutl_write_chunks(&(void*) { dval->data }, chunks, nchunks);

    return (struct dtf_valueres) { .result = DICEY_OK, .size = size, .value = dval };
}

static struct dtf_valueres value_new(const struct dicey_view_mut dest, const enum dtf_type kind, void *const data, const size_t size) {
    struct dicey_view chunk = { .data = (void*) data, .len = size };

    return value_new_many(dest, kind, &chunk, 1U);
}

struct dtf_valueres dtf_value_new_unit(const struct dicey_view_mut dest) {
    return value_new(dest, DTF_TYPE_UNIT, NULL, 0U);
}

struct dtf_valueres dtf_value_new_bool(const struct dicey_view_mut dest, const dtf_bool value) {
    const struct dtf_valueres ret = dtf_value_new_byte(dest, value);

    if (ret.value) {
        value_set_type(ret.value, DTF_TYPE_BOOL);
    }

    return ret;
}

struct dtf_valueres dtf_value_new_byte(const struct dicey_view_mut dest, dtf_byte value) {
    return value_new(dest, DTF_TYPE_BYTE, &value, sizeof value);
}

struct dtf_valueres dtf_value_new_float(const struct dicey_view_mut dest, dtf_float value) {
    return value_new(dest, DTF_TYPE_FLOAT, &value, sizeof value);
}

struct dtf_valueres dtf_value_new_int(const struct dicey_view_mut dest, dtf_int value) {
    return value_new(dest, DTF_TYPE_INT, &value, sizeof value);
}

struct dtf_valueres dtf_value_new_bytes(struct dicey_view_mut dest, struct dicey_view src) {
    if (src.len > UINT32_MAX) {
        return (struct dtf_valueres) { .result = DICEY_EOVERFLOW };
    }

    uint32_t len = (uint32_t) src.len;

    struct dicey_view chunks[] = {
        (struct dicey_view) { .data = &len, .len = sizeof len },
        src,
    };

    const size_t nchunks = sizeof chunks / sizeof *chunks;

    return value_new_many(dest, DTF_TYPE_BYTES, chunks, nchunks);
}

struct dtf_valueres dtf_value_new_str(struct dicey_view_mut dest, const char *const src) {
    const ptrdiff_t size = dutl_zstring_sizeof(src);

    if (size < 0) {
        return (struct dtf_valueres) { .result = (int) size };
    }

    struct dtf_valueres ret = dtf_value_new_bytes(
        dest,
        (struct dicey_view) { 
            .data = (void*) src,
            .len = (uint32_t) size
        }
    );

    if (ret.value) {
        value_set_type(ret.value, DTF_TYPE_STR);
    }

    return ret;
}

struct dtf_valueres dtf_value_new_path(struct dicey_view_mut dest, const char *const src) {
    struct dtf_valueres ret = dtf_value_new_str(dest, src);

    if (ret.value) {
        value_set_type(ret.value, DTF_TYPE_PATH);
    }

    return ret;
}

struct dtf_valueres dtf_value_new_selector(struct dicey_view_mut dest, struct dicey_selector sel) {
    struct dicey_view chunks[] = {
        (struct dicey_view) { .data = (void*) sel.trait, .len = strlen(sel.trait) + 1U },
        (struct dicey_view) { .data = (void*) sel.elem, .len = strlen(sel.elem) + 1U },
    };

    return value_new_many(dest, DTF_TYPE_SELECTOR, chunks, 2U);
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
    
    case DTF_TYPE_BYTE:
        return "byte";
    
    case DTF_TYPE_FLOAT:
        return "float";
    
    case DTF_TYPE_INT:
        return "int";
    
    case DTF_TYPE_ARRAY:
        return "array";

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

    case DTF_TYPE_VARIANT:
        return "variant";
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

    case DTF_TYPE_BYTE:
        return sizeof(dtf_byte);

    case DTF_TYPE_FLOAT:
        return sizeof(dtf_float);

    case DTF_TYPE_INT:
        return sizeof(dtf_int);
    
    case DTF_TYPE_ARRAY:
    case DTF_TYPE_TUPLE:
    case DTF_TYPE_BYTES:
    case DTF_TYPE_STR:
    case DTF_TYPE_PATH:
    case DTF_TYPE_SELECTOR:
    case DTF_TYPE_VARIANT:
        return DTF_SIZE_DYNAMIC;
    }
}

ptrdiff_t dtf_value_size(const enum dtf_type type) {
    size_t size = sizeof(struct dtf_value);

    if (!size_add(&size, size, (size_t) dtf_type_size(type)) || size > PTRDIFF_MAX) {
        return DICEY_EOVERFLOW;
    }

    return size;
}
