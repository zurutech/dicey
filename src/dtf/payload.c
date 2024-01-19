#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/builders.h>
#include <dicey/errors.h>
#include <dicey/types.h>

#include "util.h"

#include "to.h"
#include "value.h"

#include "payload.h"

static_assert(sizeof(uint32_t) <= sizeof(size_t), "uint32_t must fit in a size_t");
static_assert(sizeof(uint32_t) <= sizeof(ptrdiff_t), "uint32_t must fit in a ptrdiff_t");

static bool is_kind_invalid(const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
        return true;

    case DTF_PAYLOAD_HELLO:
    case DTF_PAYLOAD_BYE:
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return false;
    }
}

static bool is_message(const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        return false;

    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return true;
    }
}

static size_t message_fixed_size(const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
        return 0U;

    case DTF_PAYLOAD_HELLO:
        return sizeof(struct dtf_hello);

    case DTF_PAYLOAD_BYE:
        return sizeof(struct dtf_bye);
    
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return sizeof(struct dtf_message_head);
    }
}

static ptrdiff_t message_header_read(struct dtf_message_head *const head, struct dicey_view *const src) {
    if (!head) {
        return DICEY_EINVAL;
    }

    return dicey_view_read(src, (struct dicey_view_mut) { .data = head, .len = sizeof *head });
}

static ptrdiff_t message_header_write(
    struct dicey_view_mut *const dest,
    const enum dtf_payload_kind kind,
    const uint32_t seq,
    const uint32_t trailer_size
) {
    const struct dicey_view header = {
        .data = &(struct dtf_message_head) {
            .kind = kind,
            .seq = seq,
            .data_len = trailer_size,
        },
        .len = sizeof(struct dtf_message_head),
    };

    return dicey_view_mut_write(dest, header);
}

static ptrdiff_t payload_header_read(struct dtf_payload_head *const head, struct dicey_view *const src) {
    if (!head) {
        return DICEY_EINVAL;
    }

    return dicey_view_read(src, (struct dicey_view_mut) { .data = head, .len = sizeof *head });
}

static ptrdiff_t trailer_read_size(struct dicey_view src, const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        return 0U;
    
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE: {
        struct dtf_message_head head = { 0 };

        // get the length at its offset. It should fit in a size_t, hopefully
        const ptrdiff_t read_res = message_header_read(&head, &src);

        if (read_res < 0) {
            return read_res;
        }

        return head.data_len;
    }
    }
}

static ptrdiff_t payload_from(union dtf_payload *const payload, const enum dtf_payload_kind kind, void *const val) {
    switch (kind) {
    case DTF_PAYLOAD_HELLO:
        payload->hello = val;
        return DICEY_OK;

    case DTF_PAYLOAD_BYE:
        payload->bye = val;
        return DICEY_OK;
    
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        payload->msg = val;
        return DICEY_OK;

    default:
        free(payload);

        return DICEY_EINVAL;
    }
}

struct dtf_writeres dtf_bye_write(struct dicey_view_mut dest, const uint32_t seq, const uint32_t reason) {
    const size_t needed_len = sizeof(struct dtf_bye);

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, needed_len);

    if (alloc_res < 0) {
        return (struct dtf_writeres) { .result = alloc_res, .size = needed_len };
    }

    struct dtf_bye bye = (struct dtf_bye) {
        .kind = DTF_PAYLOAD_BYE,
        .seq = seq,
        .reason = reason,
    };
    
    const ptrdiff_t write_res = dicey_view_mut_write(&dest, (struct dicey_view) { .data = &bye, .len = sizeof bye });
    assert(write_res >= 0);
    (void) write_res; // suppress unused warning

    return (struct dtf_writeres) { .result = DICEY_OK, .start = dest.data, .size = needed_len };
}

struct dtf_writeres dtf_hello_write(struct dicey_view_mut dest, const uint32_t seq, const uint32_t version) {
    const size_t needed_len = sizeof(struct dtf_hello);

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, needed_len);

    if (alloc_res < 0) {
        return (struct dtf_writeres) { .result = alloc_res, .size = needed_len };
    }

    struct dtf_hello hello = (struct dtf_hello) {
        .kind = DTF_PAYLOAD_HELLO,
        .seq = seq,
        .version = version,
    };

    const ptrdiff_t write_res = dicey_view_mut_write(&dest, (struct dicey_view) { .data = &hello, .len = sizeof hello });
    assert(write_res >= 0); 
    (void) write_res; // suppress unused warning

    return (struct dtf_writeres) { .result = DICEY_OK, .start = dest.data, .size = needed_len };
}

ptrdiff_t dtf_message_get_content(
    const struct dtf_message *msg,
    const size_t alloc_len,
    struct dtf_message_content *dest
) {
    if (!msg || !dest) {
        return DICEY_EINVAL;
    }

    const ptrdiff_t trailer_size = dtf_message_get_trailer_size(msg);
    if (trailer_size < 0) {
        return trailer_size;
    }

    const ptrdiff_t path_len = dtf_message_get_path(msg, alloc_len, &dest->path);
    if (path_len < 0) {
        return path_len;
    }

    assert(path_len <= trailer_size);

    struct dicey_view cursor = {
        .data = dest->path + path_len,
        .len = (size_t) (trailer_size - path_len),
    };

    const ptrdiff_t selector_len = dicey_selector_from(&dest->selector, &cursor);
    if (selector_len < 0) {
        return selector_len;
    }

    assert(selector_len + path_len + cursor.len == (size_t) trailer_size);

    *dest = (struct dtf_message_content) {
        .path = dest->path,
        .selector = dest->selector,
        .value = cursor.data,
        .value_len = cursor.len,
    };

    return DICEY_OK;
}

ptrdiff_t dtf_message_get_path(const struct dtf_message *const msg, const size_t alloc_size, const char **const dest) {
    if (!msg || !dest) {
        return DICEY_EINVAL;
    }

    if (alloc_size <= offsetof(struct dtf_message, data)) {
        return DICEY_EOVERFLOW;
    }

    const ptrdiff_t trailer_size = dtf_message_get_trailer_size(msg);

    if (trailer_size < 0) {
        return trailer_size;
    }

    if (alloc_size < (size_t) trailer_size) {
        return DICEY_EOVERFLOW;
    }

    return dicey_view_as_zstring(&(struct dicey_view) { .data = msg->data, .len = trailer_size }, dest);
}

ptrdiff_t dtf_message_get_selector(
    const struct dtf_message *const msg,
    const size_t alloc_len,
    struct dicey_selector *const dest
) {
    struct dtf_message_content content = { 0 };

    const ptrdiff_t content_res = dtf_message_get_content(msg, alloc_len, &content);
    if (content_res < 0) {
        return content_res;
    }

    *dest = content.selector;

    return dicey_selector_size(*dest);
}

ptrdiff_t dtf_message_get_size(const struct dtf_message *const msg) {
    ptrdiff_t size = dtf_message_get_trailer_size(msg);
    if (size < 0) {
        return size;
    }

    if (!dutl_ssize_add(&size, size, (ptrdiff_t) sizeof msg->head)) {
        return DICEY_EOVERFLOW;
    }

    return size;
}

ptrdiff_t dtf_message_get_trailer_size(const struct dtf_message *const msg) {
    if (!msg) {
        return DICEY_EINVAL;
    }

    return (ptrdiff_t) msg->head.data_len;
}

struct dtf_writeres dtf_message_write(
    struct dicey_view_mut dest,
    const enum dtf_payload_kind kind,
    const uint32_t tid,
    const char *const path,
    const struct dicey_selector selector,
    const struct dicey_arg *const value
) {
    if (dutl_zstring_size(path) == DICEY_EOVERFLOW) {
        return (struct dtf_writeres) { .result = DICEY_EPATH_TOO_LONG };
    }

    const ptrdiff_t needed_len = dtf_message_estimate_size(kind, path, selector, value);

    if (needed_len < 0) {
        return (struct dtf_writeres) { .result = (enum dicey_error) needed_len };
    }

    const ptrdiff_t alloc_res = dicey_view_mut_ensure_cap(&dest, (size_t) needed_len);

    if (alloc_res < 0) {
        return (struct dtf_writeres) { .result = alloc_res, .size = (size_t) needed_len };
    }

    struct dtf_message *const msg = dest.data;

    const uint32_t trailer_size = (uint32_t) needed_len - (uint32_t) sizeof(struct dtf_message_head);

    ptrdiff_t result = message_header_write(&dest, kind, tid, trailer_size);
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

    const struct dicey_view dval = dicey_view_from_mut(dest);

    const struct dtf_valueres value_res = dtf_value_write(dest, value);
    if (value_res.result < 0) {
        result = value_res.result;

        goto fail;
    }

    assert(value_res.result == DICEY_OK); // no allocation should happen
    assert((char*) value_res.value == (char*) dval.data);

    (void) dval; // suppress unused warning

    // success: return the payload. Return the size as well, in case the caller wants to know how much was written
    // result will either be DICEY_OK or, if positive, the number of bytes that were allocated (aka size)
    // This allows the caller to free the payload if needed and/or detect allocations
    return (struct dtf_writeres) { .result = alloc_res, .start = msg, .size = (size_t) needed_len };

fail:
    if (alloc_res > 0) {
        free(msg);
    }

    return (struct dtf_writeres) { .result = result, .size = (size_t) needed_len };
}

ptrdiff_t dtf_message_estimate_size(
    const enum dtf_payload_kind kind,
    const char *const path,
    const struct dicey_selector selector,
    const struct dicey_arg *const value
) {
    if(!is_message(kind) || !path || !selector.trait || !selector.elem) {
        return DICEY_EINVAL;
    }

    // the value should always be present, except for GET messages
    if (!((kind == DTF_PAYLOAD_GET) ^ (bool) { value })) {
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

enum dtf_payload_kind dtf_payload_get_kind(const union dtf_payload payload) {
    if (!payload.header || is_kind_invalid(payload.header->kind)) {
        return DTF_PAYLOAD_INVALID;
    }

    return payload.header->kind;
}

ptrdiff_t dtf_payload_get_seq(const union dtf_payload payload) {
    if (!payload.header) {
        return DICEY_EINVAL;
    }

    return payload.header->seq;
}

struct dtf_loadres dtf_payload_load(struct dicey_view src) {
    struct dtf_loadres res = { .result = DICEY_OK, .remainder = src };

    // ensure we have at least the message kind
    if (!src.data || src.len < sizeof(uint32_t)) {
        res.result = DICEY_EAGAIN;

        return res;
    }

    struct dtf_payload_head head = { 0 };
    ptrdiff_t read_res = payload_header_read(&head, &(struct dicey_view) { .data = src.data, .len = sizeof head });

    if (read_res < 0) {
        res.result = read_res == DICEY_EOVERFLOW ? DICEY_EAGAIN : read_res;

        return res;
    }

    // get the base size of the message (fixed part)
    size_t needed_len = message_fixed_size(head.kind);
    assert(needed_len > 0U);

    if (needed_len > src.len) {
        res.result = DICEY_EAGAIN;
    
        return res;
    }

    // get the trailer, if any. Given that the trailer size is part of the fixed part, we know already that it's
    // available for the given message kind (or it's 0)
    const ptrdiff_t trailer_size = trailer_read_size(src, head.kind);

    if (trailer_size < 0) {
        res.result = trailer_size == DICEY_EOVERFLOW ? DICEY_EAGAIN : trailer_size;

        return res;
    }

    if (!dutl_size_add(&needed_len, needed_len, (size_t) trailer_size)) {
        res.result = DICEY_EOVERFLOW;

        return res;
    }

    if (needed_len > src.len) {
        res.result = DICEY_EAGAIN;

        return res;
    }

    // allocate the payload and then load it
    void *const payload = malloc(needed_len);
    if (!payload) {
        res.result = DICEY_ENOMEM;

        return res;
    }

    struct dicey_view_mut dest = { .data = payload, .len = needed_len };

    read_res = dicey_view_read(&res.remainder, dest);
    assert(read_res >=0);

    // success: return the payload and advance the pointer

    const ptrdiff_t payload_set = payload_from(&res.payload, head.kind, payload);
    assert(payload_set == DICEY_OK);
    (void) payload_set; // suppress unused warning

    return res;
}
