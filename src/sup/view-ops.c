// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/core/errors.h>
#include <dicey/core/views.h>

#include "trace.h"
#include "unsafe.h"
#include "util.h"

#include "view-ops.h"

ptrdiff_t dicey_view_advance(struct dicey_view *const view, const ptrdiff_t offset) {
    if (!view || !view->data || offset < 0) {
        return TRACE(DICEY_EINVAL);
    }

    if ((size_t) offset > view->len) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *view = (struct dicey_view) {
        .data = (char *) view->data + offset,
        .len = view->len - (size_t) offset,
    };

    return offset;
}

ptrdiff_t dicey_view_as_zstring(struct dicey_view *const view, const char **const str) {
    if (!view || !view->data || !str) {
        return TRACE(DICEY_EINVAL);
    }

    size_t size = strnlen(view->data, view->len);
    if (size == view->len) {
        return TRACE(DICEY_EINVAL);
    }

    if (!dutl_checked_add(&size, size, 1)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    if (size > PTRDIFF_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *str = view->data;

    return dicey_view_advance(view, (ptrdiff_t) size);
}

ptrdiff_t dicey_view_read(struct dicey_view *const view, const struct dicey_view_mut dest) {
    if (!view || !view->data || !dest.data) {
        return TRACE(DICEY_EINVAL);
    }

    if (dest.len > view->len) {
        return TRACE(DICEY_EAGAIN);
    }

    if (dest.len > PTRDIFF_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    dunsafe_read_bytes(dest, &(const void *) { view->data });

    return dicey_view_advance(view, (ptrdiff_t) dest.len);
}

ptrdiff_t dicey_view_take(struct dicey_view *const view, const ptrdiff_t nbytes, struct dicey_view *const slice) {
    if (!view || !view->data || !slice || nbytes < 0) {
        return TRACE(DICEY_EINVAL);
    }

    if ((size_t) nbytes > view->len) {
        return TRACE(DICEY_EAGAIN);
    }

    *slice = (struct dicey_view) {
        .data = view->data,
        .len = nbytes,
    };

    return dicey_view_advance(view, nbytes);
}

ptrdiff_t dicey_view_mut_advance(struct dicey_view_mut *const view, const ptrdiff_t offset) {
    if (!view || !view->data || offset < 0) {
        return TRACE(DICEY_EINVAL);
    }

    if ((size_t) offset > view->len) {
        return TRACE(DICEY_EOVERFLOW);
    }

    *view = (struct dicey_view_mut) {
        .data = (char *) view->data + offset,
        .len = view->len - offset,
    };

    return offset;
}

ptrdiff_t dicey_view_mut_ensure_cap(struct dicey_view_mut *const dest, const size_t required) {
    if (dest->len < required) {
        if (dest->data) {
            return TRACE(DICEY_EAGAIN);
        }

        if (required > PTRDIFF_MAX) {
            return TRACE(DICEY_EOVERFLOW);
        }

        // if, and only if, the buffer is NULL, we allocate a new one
        void *new_alloc = calloc(required, 1U);
        if (!new_alloc) {
            return TRACE(DICEY_ENOMEM);
        }

        *dest = (struct dicey_view_mut) {
            .data = new_alloc,
            .len = required,
        };

        return required;
    }

    return DICEY_OK;
}

ptrdiff_t dicey_view_mut_write(struct dicey_view_mut *const dest, const struct dicey_view view) {
    if (!dest || !dest->data || !view.data) {
        return TRACE(DICEY_EINVAL);
    }

    if (dest->len < view.len) {
        return TRACE(DICEY_EOVERFLOW);
    }

    dunsafe_write_bytes(&(void *) { dest->data }, view);

    return dicey_view_mut_advance(dest, view.len);
}

ptrdiff_t dicey_view_mut_write_chunks(
    struct dicey_view_mut *const dest,
    const struct dicey_view *const chunks,
    const size_t nchunks
) {
    if (!dest || !dest->data || !chunks) {
        return TRACE(DICEY_EINVAL);
    }

    const struct dicey_view *const end = chunks + nchunks;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        const ptrdiff_t res = dicey_view_mut_write(dest, *chunk);
        if (res < 0) {
            return res;
        }
    }

    return DICEY_OK;
}

ptrdiff_t dicey_view_mut_write_zstring(struct dicey_view_mut *const dest, const char *const str) {
    const ptrdiff_t size = dutl_zstring_size(str);

    if (size < 0) {
        return size;
    }

    struct dicey_view data = {
        .data = (void *) str,
        .len = (uint32_t) size,
    };

    return dicey_view_mut_write(dest, data);
}
