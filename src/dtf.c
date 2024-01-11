#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "dtf.h"
#include "util.h"

static_assert(sizeof(uint32_t) <= sizeof(size_t), "uint32_t must fit in a size_t");
static_assert(sizeof(uint32_t) <= sizeof(ptrdiff_t), "uint32_t must fit in a ptrdiff_t");

static size_t fixed_sizeof(const enum dtf_msgkind kind) {
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

static ptrdiff_t selector_sizeof(const struct dicey_selector selector) {
    const ptrdiff_t trait_len = dutl_zstring_sizeof(selector.trait); 
    if (trait_len < 0) {
        return trait_len;
    }

    const ptrdiff_t elem_len = dutl_zstring_sizeof(selector.elem);
    if (elem_len < 0) {
        return elem_len;
    }

    if (trait_len > (ptrdiff_t) UINT32_MAX - elem_len) {
        return DICEY_EOVERFLOW;
    }

    return trait_len + elem_len;
}

static void selector_write(void **const dest, const struct dicey_selector sel) {
    // todo: check for overflow, for now we assume that the caller has done its job
    struct dicey_view chunks[] = {
        (struct dicey_view) { .data = (void*) sel.trait, .len = strlen(sel.trait) + 1U },
        (struct dicey_view) { .data = (void*) sel.elem, .len = strlen(sel.elem) + 1U },
    };

    dutl_write_chunks(dest, chunks, 2U);
}

static int view_fromstr(struct dicey_view *view, const char *const str) {
    const ptrdiff_t size = dutl_zstring_sizeof(str);

    if (size < 0) {
        return (int) size;
    }

    *view = (struct dicey_view) { .data = (void*) str, .len = (uint32_t) size};

    return DICEY_OK;
}

struct dtf_craftres dtf_craft_message(
    const enum dtf_msgkind kind,
    const char *path,
    const struct dicey_selector selector,
    const struct dicey_view value
) {
    const ptrdiff_t needed_len = dtf_estimate_message_size(kind, path, selector, value);

    if (needed_len < 0) {
        return (struct dtf_craftres) { .result = (enum dicey_error) needed_len };
    }

    struct dtf_message *const msg = calloc((size_t) needed_len, 1U);
    if (!msg) {
        return (struct dtf_craftres) { .result = DICEY_ENOMEM };
    }

    return dtf_craft_message_to(
        dicey_view_mut_from(msg, (size_t) needed_len),
        kind,
        path,
        selector,
        value
    );
}

struct dtf_craftres dtf_craft_message_to(
    const struct dicey_view_mut dest,
    const enum dtf_msgkind kind,
    const char *const path,
    const struct dicey_selector selector,
    const struct dicey_view value
) {
    const ptrdiff_t needed_len = dtf_estimate_message_size(kind, path, selector, value);

    if (needed_len < 0) {
        return (struct dtf_craftres) { .result = (enum dicey_error) needed_len };
    }

    if (dest.len > PTRDIFF_MAX || needed_len > (ptrdiff_t) dest.len) {
        return (struct dtf_craftres) { .result = DICEY_EOVERFLOW };
    }

    struct dtf_message *const msg = dest.data;
    assert(msg);

    void *trailer = msg->data;

    struct dicey_view path_view = {0};

    if (view_fromstr(&path_view, path) == DICEY_EOVERFLOW) {
        return (struct dtf_craftres) { .result = DICEY_EPATH_TOO_LONG };
    }

    dutl_write_bytes(&trailer, path_view);
    selector_write(&trailer, selector);
    dutl_write_buffer(&trailer, value);

    const uint32_t trailer_size = (uint32_t) needed_len - (uint32_t) fixed_sizeof(kind);

    msg->head = (struct dtf_message_head) {
        .kind = kind,
        .data_len = trailer_size,
    };

    return (struct dtf_craftres) { .result = DICEY_OK, .msg = msg };
}

int dtf_craft_selector_to(struct dicey_view_mut dest, struct dicey_selector selector) {
    const ptrdiff_t needed_len = selector_sizeof(selector);

    if (needed_len < 0 || dest.len > PTRDIFF_MAX || needed_len > (ptrdiff_t) dest.len) {
        return DICEY_EOVERFLOW;
    }

    selector_write(&(void *) { dest.data }, selector);

    return DICEY_OK;
}

ptrdiff_t dtf_estimate_message_size(
    const enum dtf_msgkind kind,
    const char *const path,
    const struct dicey_selector selector,
    const struct dicey_view value
) {
    if(!is_message(kind)) {
        return DICEY_EINVAL;
    }

    uint32_t total_size = (uint32_t) fixed_sizeof(kind);

    const ptrdiff_t sizes[] = {
        dutl_zstring_sizeof(path),
        selector_sizeof(selector),
        dutl_buffer_sizeof(value)
    };

    const ptrdiff_t *end = sizes + sizeof sizes / sizeof *sizes;

    for (const ptrdiff_t *size = sizes; size != end; ++size) {
        if (*size < 0 || !dutl_u32_add(&total_size, total_size, (uint32_t) *size)) {
            return DICEY_EOVERFLOW;
        }
    }

    return total_size;
}

struct dtf_loadres dtf_load_message(const char *const data, const size_t len) {
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
    size_t needed_len = fixed_sizeof(kind);
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
