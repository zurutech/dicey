/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <dicey/core/errors.h>
#include <dicey/core/views.h>

#include <dicey/core/data-info.h>

#include "sup/trace.h"
#include "sup/util.h"
#include "sup/view-ops.h"

#include "value.h"

static_assert(offsetof(struct dtf_array_header, nbytes) == 0, "nbytes must be the first field of dtf_array_header");
static_assert(offsetof(struct dtf_pair_header, nbytes) == 0, "nbytes must be the first field of dtf_pair_header");
static_assert(offsetof(struct dtf_tuple_header, nbytes) == 0, "nitems must be the first field of dtf_tuple_header");

static ptrdiff_t list_probe(struct dicey_view *const src, const uint32_t nbytes, struct dicey_view *const data) {
    assert(src && data);

    return dicey_view_take(src, nbytes, data);
}

static ptrdiff_t array_header_read(struct dicey_view *const src, struct dtf_array_header *const header) {
    assert(src && header);

    const ptrdiff_t read_bytes = dicey_view_read_ptr(src, header, sizeof *header);

    if (read_bytes >= 0 && !dicey_type_is_valid(header->type)) {
        return TRACE(DICEY_EBADMSG);
    }

    return read_bytes;
}

static ptrdiff_t array_probe(struct dicey_view *const src, union _dicey_data_info *const data) {
    assert(src && data);

    struct dtf_array_header header = { 0 };

    const ptrdiff_t header_read_res = array_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = { 0 };
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *data = (union _dicey_data_info) {
        .list = {
            .nitems = header.nitems,
            .inner_type = header.type,
            .data = elems,
        },
    };

    return read_bytes;
}

static ptrdiff_t bytes_header_read(struct dicey_view *const src, struct dtf_bytes_header *const header) {
    assert(src && header);

    return dicey_view_read_ptr(src, header, sizeof *header);
}

static ptrdiff_t bytes_probe(struct dicey_view *const src, struct dtf_probed_bytes *const dest) {
    assert(src && dest);

    struct dtf_bytes_header header = { 0 };

    const ptrdiff_t header_read_res = bytes_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    if (header.len > src->len) {
        return TRACE(DICEY_EBADMSG);
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
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    return read_bytes;
}

static ptrdiff_t error_header_read(struct dicey_view *const src, struct dtf_error_header *const header) {
    assert(src && header);

    return dicey_view_read_ptr(src, header, sizeof *header);
}

static ptrdiff_t error_probe(struct dicey_view *const src, struct dicey_errmsg *const dest) {
    assert(src && dest);

    struct dtf_error_header header = { 0 };

    const ptrdiff_t header_read_res = error_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    const char *msg = NULL;
    const ptrdiff_t content_read_res = dicey_view_as_zstring(src, &msg);
    if (content_read_res < 0) {
        return content_read_res;
    }

    assert(content_read_res); // even the empty string should be terminated by a null byte

    *dest = (struct dicey_errmsg) {
        .code = header.code,
        .message = content_read_res > 1 ? msg : NULL,
    };

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    return read_bytes;
}

static ptrdiff_t pair_header_read(struct dicey_view *const src, struct dtf_pair_header *const header) {
    assert(src && header);

    return dicey_view_read_ptr(src, header, sizeof *header);
}

static ptrdiff_t pair_probe(struct dicey_view *const src, union _dicey_data_info *const data) {
    assert(src && data);

    struct dtf_pair_header header = { 0 };

    const ptrdiff_t header_read_res = pair_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = { 0 };
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *data = (union _dicey_data_info) {
        .list = {
            .nitems = 2,
            .inner_type = DICEY_VARIANT_ID,
            .data = elems,
        },
    };

    return read_bytes;
}

static ptrdiff_t tuple_header_read(struct dicey_view *const src, struct dtf_tuple_header *const header) {
    assert(src && header);

    return dicey_view_read_ptr(src, header, sizeof *header);
}

static ptrdiff_t tuple_probe(struct dicey_view *const src, union _dicey_data_info *const data) {
    assert(src && data);

    struct dtf_tuple_header header = { 0 };

    const ptrdiff_t header_read_res = tuple_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    struct dicey_view elems = { 0 };
    const ptrdiff_t content_read_res = list_probe(src, header.nbytes, &elems);
    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *data = (union _dicey_data_info) {
        .list = {
            .nitems = header.nitems,
            .inner_type = DICEY_VARIANT_ID,
            .data = elems,
        },
    };

    return read_bytes;
}

ptrdiff_t type_size(const enum dicey_type type) {
    switch (type) {
    default:
        assert(false);

    case DICEY_TYPE_INVALID:
        return TRACE(DICEY_EINVAL);

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

    case DICEY_TYPE_UUID:
        return sizeof(struct dicey_uuid);

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

static ptrdiff_t value_header_read(struct dicey_view *const src, struct dtf_value_header *const header) {
    assert(src && header);

    const ptrdiff_t result = dicey_view_read_ptr(src, header, sizeof *header);
    if (result < 0) {
        return result;
    }

    if (!dicey_type_is_valid(header->type)) {
        return TRACE(DICEY_EBADMSG);
    }

    return result;
}

static ptrdiff_t value_probe_container(
    const enum dicey_type type,
    struct dicey_view *const src,
    union _dicey_data_info *const data
) {
    switch (type) {
    default:
        assert(false);
        return TRACE(DICEY_EINVAL);

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
    union _dicey_data_info *const data
) {
    switch (type) {
    default:
        assert(false);
        return TRACE(DICEY_EINVAL);

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

ptrdiff_t dtf_value_probe(struct dicey_view *const src, struct dtf_probed_value *const info) {
    struct dtf_value_header header = { 0 };

    const ptrdiff_t header_read_res = value_header_read(src, &header);
    if (header_read_res < 0) {
        return header_read_res;
    }

    union _dicey_data_info data = { 0 };
    const ptrdiff_t content_read_res = dtf_value_probe_as(header.type, src, &data);

    if (content_read_res < 0) {
        return content_read_res;
    }

    ptrdiff_t read_bytes = 0;
    if (!dutl_checked_add(&read_bytes, header_read_res, content_read_res)) {
        return TRACE(DICEY_EOVERFLOW);
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
    union _dicey_data_info *const info
) {
    assert(src && info);

    if (!dicey_type_is_valid(type)) {
        return TRACE(DICEY_EINVAL);
    }

    const ptrdiff_t size = type_size(type);
    assert(size >= 0);

    return size == DTF_SIZE_DYNAMIC ? value_probe_dynamic(type, src, info)
                                    // this is probably slightly UB, but we are sure that we are memcpying valid data,
                                    // so we can ignore the technicalities. If this weren't actually possible to do
                                    // then no OS ever would ever work. Given that we only support a limited set of
                                    // compilers, we can safely assume that this is fine.
                                    : dicey_view_read_ptr(src, info, size);
}
