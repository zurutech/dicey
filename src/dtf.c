#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "dtf.h"
#include "dtf-to.h"
#include "dtf-value.h"
#include "util.h"

static_assert(sizeof(uint32_t) <= sizeof(size_t), "uint32_t must fit in a size_t");
static_assert(sizeof(uint32_t) <= sizeof(ptrdiff_t), "uint32_t must fit in a ptrdiff_t");

static bool is_kind_invalid(const enum dtf_msgkind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_MSGKIND_INVALID:
        return true;

    case DTF_MSGKIND_HELLO:
    case DTF_MSGKIND_BYE:
    case DTF_MSGKIND_GET:
    case DTF_MSGKIND_SET:
    case DTF_MSGKIND_EXEC:
    case DTF_MSGKIND_EVENT:
    case DTF_MSGKIND_RESPONSE:
        return false;
    }
}

static bool is_message(const enum dtf_msgkind kind) {
    switch (kind) {
    default:
        return false;

    case DTF_MSGKIND_GET:
    case DTF_MSGKIND_SET:
    case DTF_MSGKIND_EXEC:
    case DTF_MSGKIND_EVENT:
    case DTF_MSGKIND_RESPONSE:
        return true;
    }
}

static uint32_t load_uint32(const char *const data) {
    // assume the same endianess. We are on the same machine
    uint32_t res = 0U;

    memcpy(&res, data, sizeof res);

    return res;
}

static size_t message_fixed_size(const enum dtf_msgkind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_MSGKIND_INVALID:
        return 0U;

    case DTF_MSGKIND_HELLO:
        return sizeof(struct dtf_hello);

    case DTF_MSGKIND_BYE:
        return sizeof(struct dtf_bye);
    
    case DTF_MSGKIND_GET:
    case DTF_MSGKIND_SET:
    case DTF_MSGKIND_EXEC:
    case DTF_MSGKIND_EVENT:
    case DTF_MSGKIND_RESPONSE:
        return sizeof(struct dtf_message_head);
    }
}

static ptrdiff_t message_write_header(
    struct dicey_view_mut *const dest,
    const enum dtf_msgkind kind,
    const uint32_t tid,
    const uint32_t trailer_size
) {
    const struct dicey_view header = {
        .data = &(struct dtf_message_head) {
            .kind = kind,
            .tid = tid,
            .data_len = trailer_size,
        },
        .len = sizeof(struct dtf_message_head),
    };

    return dicey_view_mut_write(dest, header);
}

static size_t get_trailer_size(const enum dtf_msgkind kind, const char *const payload) {
    switch (kind) {
    default:
        return 0U;
    
    case DTF_MSGKIND_GET:
    case DTF_MSGKIND_SET:
    case DTF_MSGKIND_EXEC:
    case DTF_MSGKIND_EVENT:
    case DTF_MSGKIND_RESPONSE:
        // get the length at its offset. It should fit in a size_t, hopefully
        return load_uint32(payload + offsetof(struct dtf_message_head, data_len));
    }
}

static void loadres_set(struct dtf_loadres *const res, const enum dtf_msgkind kind, void *const payload) {
    switch (kind) {
    case DTF_MSGKIND_HELLO:
        res->hello = payload;
        break;

    case DTF_MSGKIND_BYE:
        res->bye = payload;
        break;
    
    case DTF_MSGKIND_GET:
    case DTF_MSGKIND_SET:
    case DTF_MSGKIND_EXEC:
    case DTF_MSGKIND_EVENT:
    case DTF_MSGKIND_RESPONSE:
        res->msg = payload;
        break;

    default:
        free(payload);

        break;
    }
}

struct dtf_msgres dtf_message_write(
    struct dicey_view_mut dest,
    const enum dtf_msgkind kind,
    const uint32_t tid,
    const char *path,
    const struct dicey_selector selector,
    const struct dtf_item *const value
) {
    if (dutl_zstring_size(path) == DICEY_EOVERFLOW) {
        return (struct dtf_msgres) { .result = DICEY_EPATH_TOO_LONG };
    }

    const ptrdiff_t needed_len = dtf_message_estimate_size(kind, path, selector, value);

    if (needed_len < 0) {
        return (struct dtf_msgres) { .result = (enum dicey_error) needed_len };
    }

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, (size_t) needed_len);

    if (alloc_res < 0) {
        return (struct dtf_msgres) { .result = alloc_res, .size = (size_t) needed_len };
    }

    const uint32_t trailer_size = (uint32_t) needed_len - (uint32_t) sizeof(struct dtf_message_head);

    ptrdiff_t result = message_write_header(&dest, kind, tid, trailer_size);
    if (result < 0) {
        goto fail;
    }    

    result = dicey_view_mut_write_zstring(&dest, path);
    if (result < 0) {
        goto fail;
    }

    result = dicey_view_mut_write_selector(&dest, selector);
    if (result < 0) {
        goto fail;
    }

    const struct dtf_valueres value_res = dtf_value_write(dest, value);
    if (value_res.result < 0) {
        result = value_res.result;

        goto fail;
    }

    assert(value_res.result == DICEY_OK); // no allocation should happen
    assert((char*) value_res.value == (char*) dest.data + offsetof(struct dtf_message, data));
    assert(value_res.size == trailer_size);

    // success: return the payload. Return the size as well, in case the caller wants to know how much was written
    // result will either be DICEY_OK or, if positive, the number of bytes that were allocated (aka size)
    // This allows the caller to free the payload if needed and/or detect allocations
    return (struct dtf_msgres) { .result = alloc_res, .msg = dest.data, .size = (size_t) needed_len };

fail:
    if (alloc_res > 0) {
        free(dest.data);
    }

    return (struct dtf_msgres) { .result = result, .size = (size_t) needed_len };
}

ptrdiff_t dtf_message_estimate_size(
    const enum dtf_msgkind kind,
    const char *const path,
    const struct dicey_selector selector,
    const struct dtf_item *const value
) {
    if(!is_message(kind) || !path || !selector.trait || !selector.elem) {
        return DICEY_EINVAL;
    }

    // the value should always be present, except for GET messages
    if (!((kind == DTF_MSGKIND_GET) ^ (bool) { value })) {
        return DICEY_EINVAL;
    }

    uint32_t total_size = (uint32_t) message_fixed_size(kind);

    const ptrdiff_t sizes[] = {
        dutl_zstring_size(path),
        dicey_selector_size(selector),
        dtf_value_estimate_size(value)
    };

    const ptrdiff_t *end = sizes + sizeof sizes / sizeof *sizes;

    for (const ptrdiff_t *size = sizes; size != end; ++size) {
        if (*size < 0 || !dutl_u32_add(&total_size, total_size, (uint32_t) *size)) {
            return DICEY_EOVERFLOW;
        }
    }

    return total_size;
}

struct dtf_loadres dtf_payload_load(const char *const data, const size_t len) {
    struct dtf_loadres res = { .result = DICEY_OK, .remainder = data };

    // ensure we have at least the message kind
    if (!data || len < sizeof(uint32_t)) {
        res.result = DICEY_EAGAIN;

        return res;
    }

    // peek the msgkind value (common to all payloads) without advancing the pointer
    const enum dtf_msgkind kind = load_uint32(data);

    // check if the message kind is valid
    if (is_kind_invalid(kind)) {
        res.result = DICEY_EBADMSG;

        return res;
    }

    // get the base size of the message (fixed part)
    size_t needed_len = message_fixed_size(kind);
    assert(needed_len > 0U);

    if (needed_len > len) {
        res.result = DICEY_EAGAIN;
    
        return res;
    }

    // get the trailer, if any. Given that the trailer size is part of the fixed part, we know already that it's
    // available for the given message kind (or it's 0)
    const size_t trailer_size = get_trailer_size(kind, data);

    // detect overflows
    if (trailer_size > SIZE_MAX - needed_len) {
        res.result = DICEY_EOVERFLOW;

        return res;
    }

    needed_len += trailer_size;

    if (needed_len > len) {
        res.result = DICEY_EAGAIN;

        return res;
    }

    // allocate the payload and then load it
    void *const payload = malloc(needed_len);
    if (!payload) {
        res.result = DICEY_ENOMEM;

        return res;
    }

    memcpy(payload, data, needed_len);

    // success: return the payload and advance the pointer
    res.remainder += needed_len;

    loadres_set(&res, kind, payload);

    return res;
}
