#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/builders.h>
#include <dicey/errors.h>
#include <dicey/value.h>
#include <dicey/views.h>

#include "util.h"

#include "payload.h"
#include "to.h"
#include "writer.h"

#include "value.h"

enum item_policy {
    ITEM_POLICY_EXACT,
    ITEM_POLICY_VARIANT,
};

static ptrdiff_t item_write(struct dtf_bytes_writer *dest, const struct dicey_arg *item, enum item_policy policy);
    
static ptrdiff_t items_write(
    struct dtf_bytes_writer *const dest,
    const struct dicey_arg *const items,
    const size_t nitems,
    const enum item_policy policy
) {
    const struct dicey_arg *const end = items + nitems;

    ptrdiff_t written_bytes = 0;
    for (const struct dicey_arg *item = items; item < end; ++item) {
        const ptrdiff_t write_res = item_write(dest, item, policy);

        if (write_res < 0) {
            return write_res;
        }

        if (!dutl_checked_add(&written_bytes, written_bytes, write_res)) {
            return DICEY_EOVERFLOW;
        }
    }

    return written_bytes;
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

    case DICEY_TYPE_BYTE:
        return sizeof(dtf_byte);

    case DICEY_TYPE_FLOAT:
        return sizeof(dtf_float);

    case DICEY_TYPE_INT16:
        return sizeof(dtf_i16);

    case DICEY_TYPE_INT32:
        return sizeof(dtf_i32);

    case DICEY_TYPE_INT64:
        return sizeof(dtf_i64);

    case DICEY_TYPE_UINT16:
        return sizeof(dtf_u16);

    case DICEY_TYPE_UINT32:
        return sizeof(dtf_u32);

    case DICEY_TYPE_UINT64:
        return sizeof(dtf_u64);
    
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

static ptrdiff_t blob_write(struct dtf_bytes_writer *const dest, const void *const value, const size_t size) {
    struct dicey_view data = {
        .data = value,
        .len = size,
    };

    return dtf_bytes_writer_write(dest, data);
}

static ptrdiff_t len_write(struct dtf_bytes_writer *const dest, const ptrdiff_t slen) {
    if (slen < 0) {
        return DICEY_EINVAL;
    }

    if (slen > UINT32_MAX) {
        return DICEY_EOVERFLOW;
    }

    const uint32_t len = (uint32_t) slen;

    return blob_write(dest, &len, sizeof len);
}

static_assert(offsetof(struct dtf_array_header, nbytes) == 0, "nbytes must be the first field of dtf_array_header");
static_assert(offsetof(struct dtf_pair_header, nbytes) == 0, "nbytes must be the first field of dtf_pair_header");
static_assert(offsetof(struct dtf_tuple_header, nbytes) == 0, "nitems must be the first field of dtf_tuple_header");

static ptrdiff_t list_probe(struct dicey_view *const src, const uint32_t nbytes, struct dicey_view *const data) {
    assert(src && data);

    return dicey_view_take(src, nbytes, data);
}

static ptrdiff_t list_write(
    struct dtf_bytes_writer *const dest,
    const struct dicey_view header,
    const struct dicey_arg *elems,
    const uint16_t nitems,
    const enum item_policy policy
){
    // snapshot the writer. We will use the clone to write the byte size of the array. The byte size of the array
    // is guaranteed to be the first field of the array header, so we can write it from the clone safely.
    struct dtf_bytes_writer clone_at_nbytes = {0};
    const ptrdiff_t snapshot_res = dtf_bytes_writer_snapshot(dest, &clone_at_nbytes);
    if (snapshot_res < 0) {
        return snapshot_res;
    }

    const ptrdiff_t header_nbytes = dtf_bytes_writer_write(dest, header);
    if (header_nbytes < 0) {
        return header_nbytes;
    }

    const ptrdiff_t content_nbytes = items_write(dest, elems, nitems, policy);
    if (content_nbytes < 0) {
        return content_nbytes;
    }

    // set the nbytes field of the array header to the number of bytes written by the items_write call
    const ptrdiff_t nbytes_write_res = len_write(&clone_at_nbytes, content_nbytes);

    if (nbytes_write_res < 0) {
        return nbytes_write_res;
    }

    ptrdiff_t written_bytes = header_nbytes;
    if (!dutl_checked_add(&written_bytes, written_bytes, content_nbytes)) {
        return DICEY_EOVERFLOW;
    }

    return written_bytes;
}

static ptrdiff_t array_header_read(struct dicey_view *const src, struct dtf_array_header *const header) {
    assert(src && header);

    const ptrdiff_t read_bytes = dicey_view_read(
        src,
        (struct dicey_view_mut) { .data = header, .len = sizeof *header }
    );

    if (read_bytes >= 0 && !dicey_type_is_valid(header->type)) {
        return DICEY_EBADMSG;
    }

    return read_bytes;
}

static ptrdiff_t array_probe(struct dicey_view *const src, union dtf_probed_data *const data) {
    assert(src && data);

    struct dtf_array_header header = {0};

    const ptrdiff_t header_read_res = array_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = {0};
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return DICEY_EOVERFLOW;
    }

    *data = (union dtf_probed_data) {
        .list = {
            .nitems = header.nitems,
            .inner_type = header.type,
            .data = elems,
        },
    };

    return read_bytes;
}

static ptrdiff_t array_write(struct dtf_bytes_writer *const dest, const struct dicey_array_arg array) {
    if (!dicey_type_is_valid(array.type)) {
        return DICEY_EINVAL;
    }

    const struct dtf_array_header header = { .nitems = array.nitems, .type = array.type };

    return list_write(
        dest,
        (struct dicey_view) { .data = &header, .len = sizeof header }, 
        array.elems,
        array.nitems,
        ITEM_POLICY_EXACT
    );
}

static ptrdiff_t bool_write(struct dtf_bytes_writer *const dest, const dtf_bool value) {
    return blob_write(dest, &value, sizeof value);
}

static ptrdiff_t byte_write(struct dtf_bytes_writer *const dest, const dtf_byte value) {
    return blob_write(dest, &value, sizeof value);
}

static ptrdiff_t bytes_header_read(struct dicey_view *const src, struct dtf_bytes_header *const header) {
    assert(src && header);

    return dicey_view_read(
        src,
        (struct dicey_view_mut) { .data = header, .len = sizeof *header }
    );
}

static ptrdiff_t bytes_probe(struct dicey_view *const src, struct dtf_probed_bytes *const dest) {
    assert(src && dest);

    struct dtf_bytes_header header = {0};

    const ptrdiff_t header_read_res = bytes_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    if (header.len > src->len) {
        return DICEY_EBADMSG;
    }

    *dest = (struct dtf_probed_bytes) {
        .len = header.len,
        .data = src->data,
    };

    const ptrdiff_t content_read_res = dicey_view_advance(src, header.len);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    dutl_checked_add(&read_bytes, header_read_res, content_read_res);

    return read_bytes;
}

static ptrdiff_t bytes_write(struct dtf_bytes_writer *const dest, const struct dicey_bytes_arg bytes) {
    const struct dtf_bytes_header header = { .len = bytes.len };

    const ptrdiff_t header_nbytes = blob_write(dest, &header, sizeof header);
    if (header_nbytes < 0) {
        return header_nbytes;
    }

    const ptrdiff_t content_nbytes = blob_write(dest, bytes.data, bytes.len);
    if (content_nbytes < 0) {
        return content_nbytes;
    }

    ptrdiff_t written_bytes = header_nbytes;
    if (!dutl_checked_add(&written_bytes, written_bytes, content_nbytes)) {
        return DICEY_EOVERFLOW;
    }

    return written_bytes;
}

static ptrdiff_t error_header_read(struct dicey_view *const src, struct dtf_error_header *const header) {
    assert(src && header);

    return dicey_view_read(
        src,
        (struct dicey_view_mut) { .data = header, .len = sizeof *header }
    );
}

static ptrdiff_t error_probe(struct dicey_view *const src, struct dicey_errmsg *const dest) {
    assert(src && dest);

    struct dtf_error_header header = {0};

    const ptrdiff_t header_read_res = error_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    const char *msg = NULL;
    const ptrdiff_t content_read_res = dicey_view_as_zstring(src, &msg);
    if (content_read_res < 0) {
        return content_read_res;
    }

    *dest = (struct dicey_errmsg) {
        .code = header.code,
        .message = msg,
    };

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return DICEY_EOVERFLOW;
    }

    return read_bytes;
}
    
static ptrdiff_t error_write(struct dtf_bytes_writer *const dest, const struct dicey_error_arg error) {
    const struct dtf_error_header header = { .code = error.code };

    const ptrdiff_t header_nbytes = blob_write(dest, &header, sizeof header);
    if (header_nbytes < 0) {
        return header_nbytes;
    }

    const ptrdiff_t content_nbytes = dtf_bytes_writer_write_zstring(dest, error.message);
    if (content_nbytes < 0) {
        return content_nbytes;
    }

    ptrdiff_t written_bytes = 0;
    if (!dutl_checked_add(&written_bytes, header_nbytes, content_nbytes)) {
        return DICEY_EOVERFLOW;
    }

    return written_bytes;
}

static ptrdiff_t float_write(struct dtf_bytes_writer *const dest, const dtf_float value) {
    return blob_write(dest, &value, sizeof value);
}

#define int_write(DEST, VALUE) blob_write(DEST, &(VALUE), sizeof (VALUE))

static ptrdiff_t pair_header_read(struct dicey_view *const src, struct dtf_pair_header *const header) {
    assert(src && header);

    return dicey_view_read(
        src,
        (struct dicey_view_mut) { .data = header, .len = sizeof *header }
    );
}

static ptrdiff_t pair_probe(struct dicey_view *const src, union dtf_probed_data *const data) {
    assert(src && data);

    struct dtf_pair_header header = {0};

    const ptrdiff_t header_read_res = pair_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = {0};
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return DICEY_EOVERFLOW;
    }

    *data = (union dtf_probed_data) {
        .list = {
            .nitems = 2,
            .inner_type = DICEY_VARIANT_ID,
            .data = elems,
        },
    };

    return read_bytes;
}

static ptrdiff_t pair_write(struct dtf_bytes_writer *const dest, const struct dicey_pair_arg pair) {
    const struct dicey_arg items[] = { *pair.first, *pair.second };

    return list_write(
        dest,
        (struct dicey_view) { .data = &(struct dtf_pair_header) { 0 }, .len = sizeof (struct dtf_pair_header) },
        items,
        2,
        ITEM_POLICY_VARIANT);
}

static ptrdiff_t tuple_header_read(struct dicey_view *const src, struct dtf_tuple_header *const header) {
    assert(src && header);

    return dicey_view_read(
        src,
        (struct dicey_view_mut) { .data = header, .len = sizeof *header }
    );
}

static ptrdiff_t tuple_probe(struct dicey_view *const src, union dtf_probed_data *const data) {
    assert(src && data);

    struct dtf_tuple_header header = {0};

    const ptrdiff_t header_read_res = tuple_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = {0};
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return DICEY_EOVERFLOW;
    }

    *data = (union dtf_probed_data) {
        .list = {
            .nitems = header.nitems,
            .inner_type = DICEY_VARIANT_ID,
            .data = elems,
        },
    };

    return read_bytes;
}

static ptrdiff_t tuple_write(struct dtf_bytes_writer *const dest, const struct dicey_tuple_arg tuple) {
    const struct dicey_view header = {
        .data = &(struct dtf_tuple_header) { .nitems = tuple.nitems },
        .len = sizeof (struct dtf_tuple_header),
    };

    return list_write(dest, header, tuple.elems, tuple.nitems, ITEM_POLICY_EXACT);
}

static ptrdiff_t value_header_read(struct dicey_view *const src, struct dtf_value_header *const header) {
    assert(src && header);

    const ptrdiff_t result = dicey_view_read(src, (struct dicey_view_mut) { .data = header, .len = sizeof *header });
    if (result < 0) {
        return result;
    }

    if (!dicey_type_is_valid(header->type)) {
        return DICEY_EBADMSG;
    }

    return result;
}

static ptrdiff_t value_header_write(struct dtf_bytes_writer *const dest, const struct dicey_arg *const value) {
    struct dtf_value_header header = { .type = value->type };

    ptrdiff_t write_res = blob_write(dest, &header, sizeof header);
    if (write_res < 0) {
        return write_res;
    }

    return write_res;
}

static ptrdiff_t item_write(
    struct dtf_bytes_writer *const dest,
    const struct dicey_arg *const item,
    const enum item_policy policy
) {
    assert(dtf_bytes_writer_is_valid(dest) && item);
    
    if (!dicey_type_is_valid(item->type)) {
        return DICEY_EINVAL;
    }

    ptrdiff_t written_bytes = 0;

    if (policy == ITEM_POLICY_VARIANT) {
        const ptrdiff_t header_write_res = value_header_write(dest, item);

        if (header_write_res < 0) {
            return header_write_res;
        }

        written_bytes = header_write_res;
    }

    ptrdiff_t content_bytes = 0;
    switch (item->type) {
    default:
        assert(false);

    case DICEY_TYPE_UNIT:
        break;

    case DICEY_TYPE_BOOL:
        content_bytes = bool_write(dest, item->boolean);
        break;

    case DICEY_TYPE_BYTE:
        content_bytes = byte_write(dest, item->byte);
        break;

    case DICEY_TYPE_FLOAT:
        content_bytes = float_write(dest, item->floating);
        break;

    case DICEY_TYPE_INT16:
        content_bytes = int_write(dest, item->i16);
        break;

    case DICEY_TYPE_INT32:
        content_bytes = int_write(dest, item->i32);
        break;

    case DICEY_TYPE_INT64:
        content_bytes = int_write(dest, item->i64);
        break;

    case DICEY_TYPE_UINT16:
        content_bytes = int_write(dest, item->u16);
        break;

    case DICEY_TYPE_UINT32:
        content_bytes = int_write(dest, item->u32);
        break;

    case DICEY_TYPE_UINT64:
        content_bytes = int_write(dest, item->u64);
        break;

    case DICEY_TYPE_ARRAY:
        content_bytes = array_write(dest, item->array);
        break;

    case DICEY_TYPE_TUPLE:
        content_bytes = tuple_write(dest, item->tuple);
        break;

    case DICEY_TYPE_PAIR:
        content_bytes = pair_write(dest, item->pair);
        break;

    case DICEY_TYPE_BYTES:
        content_bytes = bytes_write(dest, item->bytes);
        break;

    case DICEY_TYPE_STR:
    case DICEY_TYPE_PATH:
        content_bytes = dtf_bytes_writer_write_zstring(dest, item->str);
        break;

    case DICEY_TYPE_SELECTOR:
        content_bytes = dtf_bytes_writer_write_selector(dest, item->selector);
        break;
    
    case DICEY_TYPE_ERROR:
        content_bytes = error_write(dest, item->error);
        break;
    }

    if (content_bytes < 0) {
        return content_bytes;
    }

    if (!dutl_checked_add(&written_bytes, written_bytes, content_bytes)) {
        return DICEY_EOVERFLOW;
    }

    return written_bytes;
}

static ptrdiff_t value_probe_container(
    const enum dicey_type type,
    struct dicey_view *const src,
    union dtf_probed_data *const data
) {
    switch (type) {
    default:
        assert(false);
        return DICEY_EINVAL;

    case DICEY_TYPE_ARRAY:
        return array_probe(src, data);

    case DICEY_TYPE_TUPLE:
        return tuple_probe(src, data);

    case DICEY_TYPE_PAIR:
        return pair_probe(src, data);
    }
}

static ptrdiff_t value_probe_dynamic(
    const enum dicey_type type,
    struct dicey_view *const src,
    union dtf_probed_data *const data
) {
    switch (type) {
    default:
        assert(false);
        return DICEY_EINVAL;

    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_TUPLE:
    case DICEY_TYPE_PAIR:
        return value_probe_container(type, src, data);

    case DICEY_TYPE_BYTES:
        return bytes_probe(src, &data->bytes);

    case DICEY_TYPE_STR:
    case DICEY_TYPE_PATH:
        return dicey_view_as_zstring(src, &data->str);

    case DICEY_TYPE_SELECTOR:
        return dtf_selector_from(&data->selector, src);

    case DICEY_TYPE_ERROR:
        return error_probe(src, &data->error);
    }
}

ptrdiff_t dtf_selector_from(struct dicey_selector *const sel, struct dicey_view *const src) {
    assert(sel && src && src->data);

    const ptrdiff_t trait_len = dicey_view_as_zstring(src, &sel->trait);
    if (trait_len < 0) {
        return trait_len;
    }

    const ptrdiff_t elem_len = dicey_view_as_zstring(src, &sel->elem);
    if (elem_len < 0) {
        return elem_len;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, trait_len, elem_len)) {
        return DICEY_EOVERFLOW;
    }

    return read_bytes;
}

ptrdiff_t dtf_value_estimate_size(const struct dicey_arg *const item) {
    struct dtf_bytes_writer sizer = dtf_bytes_writer_new_sizer();

    const ptrdiff_t res = item_write(&sizer, item, ITEM_POLICY_VARIANT);

    return res < 0 ? res : dtf_bytes_writer_get_state(&sizer).size;
}

ptrdiff_t dtf_value_probe(struct dicey_view *const src, struct dtf_probed_value *const info) {
    struct dtf_value_header header = {0};

    const ptrdiff_t header_read_res = value_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    union dtf_probed_data data = {0};
    const ptrdiff_t content_read_res = dtf_value_probe_as(header.type, src, &data);

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return DICEY_EOVERFLOW;
    }

    *info = (struct dtf_probed_value) {
        .type = header.type,
        .data = data,
    };

    return read_bytes;
}

ptrdiff_t dtf_value_probe_as(
    const enum dicey_type type,
    struct dicey_view *const src,
    union dtf_probed_data *const info
) {
    assert(src && info);

    if (!dicey_type_is_valid(type)) {
        return DICEY_EINVAL;
    }

    const ptrdiff_t size = type_size(type);
    assert(size >= 0);

    return size == DTF_SIZE_DYNAMIC
        ? value_probe_dynamic(type, src, info)
        : dicey_view_read(src, (struct dicey_view_mut) { .data = info, .len = size });
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

    struct dtf_bytes_writer writer = dtf_bytes_writer_new(dest);

    const ptrdiff_t write_res = dtf_value_write_to(&writer, item);
    if (write_res < 0) {
        if (alloc_res != DICEY_OK) {
            free(dest.data);
        }

        return (struct dtf_valueres) { .result = write_res, .size = (size_t) size };
    }
    
    // return the size of the value into result, too. This will be 0 if no allocation was needed, or size otherwise
    return (struct dtf_valueres) { .result = alloc_res, .size = (size_t) size, .value = dval };
}

ptrdiff_t dtf_value_write_to(struct dtf_bytes_writer *const writer, const struct dicey_arg *const item) {
    return item_write(writer, item, ITEM_POLICY_VARIANT);
}
