#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtf.h"

static_assert(sizeof(uint32_t) <= sizeof(size_t), "uint32_t must fit in a size_t");
static_assert(sizeof(uint32_t) <= sizeof(ptrdiff_t), "uint32_t must fit in a ptrdiff_t");

static bool checked_add(uint32_t *const res, const uint32_t a, const uint32_t b) {
    const uint32_t sum = a + b;

    if (sum < a) {
        return false;
    }

    *res = sum;

    return true;
}

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
        assert(false);

    case DTF_MSGKIND_INVALID:
    case DTF_MSGKIND_HELLO:
    case DTF_MSGKIND_BYE:
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

static ptrdiff_t trailer_sizeof(const struct dtf_view *const views, const size_t nviews) {
    const struct dtf_view *const end = views + nviews;

    uint32_t trailer_size = sizeof views->len * nviews;

    for (const struct dtf_view *view = views; view != end; ++view) {
        if (!checked_add(&trailer_size, trailer_size, view->len)) {
            return DTF_EOVERFLOW;
        }
    }

    return trailer_size;
}

static void write_blob(void **const dest, const struct dtf_view view) {
    if (view.len) {
        memcpy(*dest, &view.len, sizeof view.len);
        *dest += sizeof view.len;

        memcpy(*dest, view.data, view.len);
        *dest += view.len;
    }
}

static void trailer_write(void *dest, const struct dtf_view *views, const size_t nviews) {
    const struct dtf_view *const end = views + nviews;

    for (const struct dtf_view *view = views; view != end; ++view) {
        write_blob(&dest, *view);
    }
}

struct dtf_craftres dtf_craft_message(
    const enum dtf_msgkind kind,
    const struct dtf_view path,
    const struct dtf_view selector,
    const struct dtf_view value
) {
    const ptrdiff_t needed_len = dtf_estimate_message_size(kind, path, selector, value);

    if (needed_len < 0) {
        return (struct dtf_craftres) { .result = (enum dtf_error) needed_len };
    }

    struct dtf_message *const msg = calloc((size_t) needed_len, 1U);
    if (!msg) {
        return (struct dtf_craftres) { .result = DTF_ENOMEM };
    }

    const struct dtf_view views[] = { path, selector, value };
    const size_t nviews = sizeof views / sizeof *views;

    trailer_write(msg->data, views, nviews);

    msg->head = (struct dtf_message_head) {
        .kind = kind,
        .data_len = (uint32_t) { needed_len } - fixed_sizeof(kind),
    };

    return (struct dtf_craftres) { .result = DTF_OK, .msg = msg };
}

ptrdiff_t dtf_estimate_message_size(
    const enum dtf_msgkind kind,
    const struct dtf_view path,
    const struct dtf_view selector,
    const struct dtf_view value
) {
    if(!is_message(kind)) {
        return DTF_EINVAL;
    }

    const struct dtf_view views[] = { path, selector, value };
    const size_t nviews = sizeof views / sizeof *views;
    
    const ptrdiff_t trailer_size = trailer_sizeof(views, nviews);

    if (trailer_size < 0) {
        return DTF_EOVERFLOW;
    }

    return (ptrdiff_t) { fixed_sizeof(kind) } + trailer_size;
}

struct dtf_loadres dtf_load_message(const char *const data, const size_t len) {
    struct dtf_loadres res = { .result = DTF_OK, .remainder = data };

    // ensure we have at least the message kind
    if (!data || len < sizeof(uint32_t)) {
        res.result = DTF_EAGAIN;

        return res;
    }

    // peek the msgkind value (common to all payloads) without advancing the pointer
    const enum dtf_msgkind kind = load_uint32(data);

    // check if the message kind is valid
    if (is_kind_invalid(kind)) {
        res.result = DTF_EBADMSG;

        return res;
    }

    // get the base size of the message (fixed part)
    size_t needed_len = fixed_sizeof(kind);
    assert(needed_len > 0U);

    if (needed_len > len) {
        res.result = DTF_EAGAIN;
    
        return res;
    }

    // get the trailer, if any. Given that the trailer size is part of the fixed part, we know already that it's
    // available for the given message kind (or it's 0)
    const size_t trailer_size = get_trailer_size(kind, data);

    // detect overflows
    if (trailer_size > SIZE_MAX - needed_len) {
        res.result = DTF_EOVERFLOW;

        return res;
    }

    needed_len += trailer_size;

    if (needed_len > len) {
        res.result = DTF_EAGAIN;

        return res;
    }

    // allocate the payload and then load it
    void *const payload = malloc(needed_len);
    if (!payload) {
        res.result = DTF_ENOMEM;

        return res;
    }

    memcpy(payload, data, needed_len);

    // success: return the payload and advance the pointer
    res.remainder += needed_len;

    loadres_set(&res, kind, payload);

    return res;
}

const char* dtf_strerror(const int errnum) {
    switch (errnum) {
    default:
        return "unknown error";

    case DTF_OK:
        return "success";

    case DTF_EAGAIN:
        return "not enough data";

    case DTF_ENOMEM:
        return "out of memory";

    case DTF_EINVAL:
        return "invalid argument";

    case DTF_EBADMSG:
        return "bad message";

    case DTF_EOVERFLOW:
        return "overflow";
    }
}