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
#include <stdint.h>
#include <string.h>

#include <dicey/core/data-info.h>
#include <dicey/core/errors.h>
#include <dicey/core/message.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>

#include "dtf/dtf.h"

#include "sup/trace.h"
#include "sup/util.h"

bool dicey_iterator_has_next(const struct dicey_iterator iter) {
    return iter._data.len > 0;
}

enum dicey_error dicey_iterator_next(struct dicey_iterator *const iter, struct dicey_value *const dest) {
    assert(iter && dest);

    if (!dicey_iterator_has_next(*iter)) {
        return TRACE(DICEY_ENODATA);
    }

    struct dicey_view view = iter->_data;
    struct dtf_probed_value probed_value = { .type = iter->_type };

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

const struct dicey_value *dicey_owning_value_borrow(const struct dicey_owning_value *value) {
    return dicey_owning_value_is_valid(value) ? &value->_value : NULL;
}

void dicey_owning_value_deinit(struct dicey_owning_value *const value) {
    if (dicey_owning_value_is_valid(value)) {
        dicey_packet_deinit(&value->_owner);

        *value = (struct dicey_owning_value) { 0 };
    }
}

void dicey_owning_value_from_parts(
    struct dicey_owning_value *dest,
    struct dicey_packet owner,
    struct dicey_value *wanted_bit
) {
    assert(dest && wanted_bit && dicey_packet_is_valid(owner));

    *dest = (struct dicey_owning_value) {
        ._owner = owner,
        ._value = *wanted_bit,
    };
}

bool dicey_owning_value_is_valid(const struct dicey_owning_value *const value) {
    return value && dicey_packet_is_valid(value->_owner);
}

enum dicey_error dicey_packet_into_value(struct dicey_packet *const packet, struct dicey_owning_value *const dest) {
    assert(packet && dest);

    if (!dicey_packet_is_valid(*packet)) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_message msg = { 0 };
    enum dicey_error err = dicey_packet_as_message(*packet, &msg);
    if (err) {
        return err;
    }

    dicey_owning_value_from_parts(dest, *packet, &msg.value);

    return DICEY_OK;
}

int dicey_selector_cmp(const struct dicey_selector a, const struct dicey_selector b) {
    const int res = strcmp(a.trait, b.trait);

    return res ? res : strcmp(a.elem, b.elem);
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
        return TRACE(DICEY_EOVERFLOW);
    }

    return trait_len + elem_len;
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

    case DICEY_TYPE_UUID:

    case DICEY_TYPE_PATH:
    case DICEY_TYPE_SELECTOR:

    case DICEY_TYPE_ERROR:
        return true;

    default:
        return false;
    }
}

const char *dicey_type_name(const enum dicey_type type) {
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

    case DICEY_TYPE_INT16:
        return "i16";

    case DICEY_TYPE_INT32:
        return "i32";

    case DICEY_TYPE_INT64:
        return "i64";

    case DICEY_TYPE_UINT16:
        return "u16";

    case DICEY_TYPE_UINT32:
        return "u32";

    case DICEY_TYPE_UINT64:
        return "u64";

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

    case DICEY_TYPE_UUID:
        return "uuid";

    case DICEY_TYPE_PATH:
        return "path";

    case DICEY_TYPE_SELECTOR:
        return "selector";

    case DICEY_TYPE_ERROR:
        return "error";
    }
}

enum dicey_type dicey_value_get_type(const struct dicey_value *const value) {
    return value ? value->_type : DICEY_TYPE_INVALID;
}

#define DICEY_VALUE_GET_IMPL_TRIVIAL(NAME, TYPE, DICEY_TYPE, FIELD)                                                    \
    enum dicey_error dicey_value_get_##NAME(const struct dicey_value *const value, TYPE *const dest) {                 \
        assert(value &&dest);                                                                                          \
        if (dicey_value_get_type(value) != DICEY_TYPE) {                                                               \
            return TRACE(DICEY_EVALUE_TYPE_MISMATCH);                                                                  \
        }                                                                                                              \
        *dest = value->_data.FIELD;                                                                                    \
        return DICEY_OK;                                                                                               \
    }

enum dicey_error dicey_value_get_array(const struct dicey_value *const value, struct dicey_list *const dest) {
    if (dicey_value_get_type(value) != DICEY_TYPE_ARRAY) {
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
    }

    return dicey_value_get_list(value, dest);
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
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
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

DICEY_VALUE_GET_IMPL_TRIVIAL(path, const char *, DICEY_TYPE_PATH, str)

enum dicey_error dicey_value_get_list(const struct dicey_value *const value, struct dicey_list *const dest) {
    assert(value && dest);

    const enum dicey_type type = dicey_value_get_type(value);
    if (type != DICEY_TYPE_ARRAY && type != DICEY_TYPE_TUPLE) {
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
    }

    assert(value && dest);

    *dest = (struct dicey_list) {
        ._type = value->_data.list.inner_type,
        ._data = value->_data.list.data,
    };

    return DICEY_OK;
}

enum dicey_error dicey_value_get_pair(const struct dicey_value *const value, struct dicey_pair *const dest) {
    assert(value && dest);

    if (dicey_value_get_type(value) != DICEY_TYPE_PAIR) {
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
    }

    // hack: craft a tuple and use it to get the pair, given that in memory is identical to (vv) after the header
    struct dicey_list tuple = {
        ._type = DICEY_VARIANT_ID,
        ._data = value->_data.list.data,
    };

    struct dicey_iterator iter = dicey_list_iter(&tuple);

    struct dicey_value *const items[] = { &dest->first, &dest->second, NULL };

    for (struct dicey_value *const *item = items; *item; ++item) {
        const enum dicey_error err = dicey_iterator_next(&iter, *item);
        if (err) {
            // this is not acceptable, the tuple should have exactly 2 items
            return TRACE(DICEY_EBADMSG);
        }

        DICEY_UNUSED(err); // silence unused warning, damn you MSVC
    }

    return DICEY_OK;
}

DICEY_VALUE_GET_IMPL_TRIVIAL(selector, struct dicey_selector, DICEY_TYPE_SELECTOR, selector)
DICEY_VALUE_GET_IMPL_TRIVIAL(str, const char *, DICEY_TYPE_STR, str)

enum dicey_error dicey_value_get_tuple(const struct dicey_value *const value, struct dicey_list *const dest) {
    assert(value && dest);

    if (dicey_value_get_type(value) != DICEY_TYPE_TUPLE) {
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
    }

    return dicey_value_get_list(value, dest);
}

DICEY_VALUE_GET_IMPL_TRIVIAL(uuid, struct dicey_uuid, DICEY_TYPE_UUID, uuid)
DICEY_VALUE_GET_IMPL_TRIVIAL(u16, uint16_t, DICEY_TYPE_UINT16, u16)
DICEY_VALUE_GET_IMPL_TRIVIAL(u32, uint32_t, DICEY_TYPE_UINT32, u32)
DICEY_VALUE_GET_IMPL_TRIVIAL(u64, uint64_t, DICEY_TYPE_UINT64, u64)

bool dicey_value_is(const struct dicey_value *const value, const enum dicey_type type) {
    return value->_type == type;
}

bool dicey_value_is_unit(const struct dicey_value *const value) {
    return dicey_value_is(value, DICEY_TYPE_UNIT);
}

bool dicey_value_is_valid(const struct dicey_value *const value) {
    return dicey_type_is_valid(value->_type);
}
