// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stddef.h>
#include <stdint.h>

#include <dicey/errors.h>
#include <dicey/views.h>

#include "trace.h"
#include "util.h"
#include "view-ops.h"

#include "writer.h"

static ptrdiff_t buffer_write(struct dicey_view_mut *const dest, const struct dicey_view data) {
    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t sizer_write(ptrdiff_t *const size, const struct dicey_view data) {
    if (!dutl_checked_add(size, *size, data.len)) {
        return TRACE(DICEY_EOVERFLOW);
    }

    return TRACE(DICEY_OK);
}

enum dtf_bytes_writer_kind dtf_bytes_writer_get_kind(const struct dtf_bytes_writer *const writer) {
    return writer->kind;
}

union dtf_bytes_writer_state dtf_bytes_writer_get_state(const struct dtf_bytes_writer *const writer) {
    return writer->state;
}

bool dtf_bytes_writer_is_valid(const struct dtf_bytes_writer *const writer) {
    switch (writer->kind) {
    case DTF_BYTES_WRITER_KIND_BUFFER:
        return writer->state.buffer.data;

    case DTF_BYTES_WRITER_KIND_SIZER:
        return true;

    default:
        return false;
    }
}

struct dtf_bytes_writer dtf_bytes_writer_new(const struct dicey_view_mut buffer) {
    return (struct dtf_bytes_writer) {
        .kind = DTF_BYTES_WRITER_KIND_BUFFER,
        .state = {
            .buffer = buffer,
        },
    };
}

struct dtf_bytes_writer dtf_bytes_writer_new_sizer(void) {
    return (struct dtf_bytes_writer) {
        .kind = DTF_BYTES_WRITER_KIND_SIZER,
        .state = {
            .size = 0,
        },
    };
}

ptrdiff_t dtf_bytes_writer_snapshot(const struct dtf_bytes_writer *const writer, struct dtf_bytes_writer *const clone) {
    // all writers support snapshotting for now
    *clone = *writer;

    return TRACE(DICEY_OK);
}

ptrdiff_t dtf_bytes_writer_write(struct dtf_bytes_writer *const writer, const struct dicey_view data) {
    switch (writer->kind) {
    case DTF_BYTES_WRITER_KIND_BUFFER:
        return buffer_write(&writer->state.buffer, data);

    case DTF_BYTES_WRITER_KIND_SIZER:
        return sizer_write(&writer->state.size, data);

    default:
        return TRACE(DICEY_EINVAL);
    }
}

ptrdiff_t dtf_bytes_writer_write_chunks(
    struct dtf_bytes_writer *const writer,
    const struct dicey_view *const chunks,
    const size_t                   nchunks
) {
    if (!dtf_bytes_writer_is_valid(writer) || !chunks) {
        return TRACE(DICEY_EINVAL);
    }

    const struct dicey_view *const end = chunks + nchunks;

    ptrdiff_t written_bytes = 0;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        const ptrdiff_t res = dtf_bytes_writer_write(writer, *chunk);
        if (res < 0) {
            return res;
        }

        if (!dutl_checked_add(&written_bytes, written_bytes, res)) {
            return TRACE(DICEY_EOVERFLOW);
        }
    }

    return written_bytes;
}

ptrdiff_t dtf_bytes_writer_write_selector(struct dtf_bytes_writer *const writer, const struct dicey_selector sel) {
    if (!dtf_bytes_writer_is_valid(writer) || !sel.trait || !sel.elem) {
        return TRACE(DICEY_EINVAL);
    }

    const ptrdiff_t trait_len = dutl_zstring_size(sel.trait);
    if (trait_len < 0) {
        return trait_len;
    }

    const ptrdiff_t elem_len = dutl_zstring_size(sel.elem);
    if (elem_len < 0) {
        return elem_len;
    }

    struct dicey_view chunks[] = {
        (struct dicey_view) {.data = (void *) sel.trait, .len = (size_t) trait_len},
        (struct dicey_view) { .data = (void *) sel.elem, .len = (size_t) elem_len },
    };

    return dtf_bytes_writer_write_chunks(writer, chunks, sizeof chunks / sizeof *chunks);
}

ptrdiff_t dtf_bytes_writer_write_zstring(struct dtf_bytes_writer *const writer, const char *const str) {
    const ptrdiff_t size = dutl_zstring_size(str);

    if (size < 0) {
        return size;
    }

    struct dicey_view data = {
        .data = (void *) str,
        .len = (uint32_t) size,
    };

    return dtf_bytes_writer_write(writer, data);
}
