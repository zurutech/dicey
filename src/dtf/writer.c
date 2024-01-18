#include <stddef.h>
#include <stdint.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "util.h"

#include "writer.h"

static ptrdiff_t buffer_write(void *const context, const struct dicey_view data) {
    struct dicey_view_mut *const dest = context;

    return dicey_view_mut_write(dest, data);
}

static ptrdiff_t sizer_write(void *const context, const struct dicey_view data) {
    ptrdiff_t *const size = context;

    if (!dutl_ssize_add(size, *size, data.len)) {
        return DICEY_EOVERFLOW;
    }

    return DICEY_OK;
}

bool dtf_bytes_writer_is_valid(const struct dtf_bytes_writer writer) {
    return writer.write && writer.context;
}

struct dtf_bytes_writer dtf_bytes_writer_new(struct dicey_view_mut *const buffer) {
    return (struct dtf_bytes_writer) {
        .context = buffer,
        .write = buffer_write,
    };
}
    

struct dtf_bytes_writer dtf_bytes_writer_new_sizer(ptrdiff_t *const size) {
    return (struct dtf_bytes_writer) {
        .context = size,
        .write = sizer_write,
    };
}

ptrdiff_t dtf_bytes_writer_write(const struct dtf_bytes_writer writer, const struct dicey_view data) {
    return writer.write(writer.context, data);
}

ptrdiff_t dtf_bytes_writer_write_chunks(
    const struct dtf_bytes_writer writer,
    const struct dicey_view *const chunks,
    const size_t nchunks
) {
    if (!dtf_bytes_writer_is_valid(writer) || !chunks) {
        return DICEY_EINVAL;
    }

    const struct dicey_view *const end = chunks + nchunks;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        const ptrdiff_t res = dtf_bytes_writer_write(writer, *chunk);
        if (res < 0) {
            return res;
        }
    }

    return DICEY_OK;
}

ptrdiff_t dtf_bytes_writer_write_selector(const struct dtf_bytes_writer writer, const struct dicey_selector sel) {
    if (!dtf_bytes_writer_is_valid(writer) || !sel.trait || !sel.elem) {
        return DICEY_EINVAL;
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
        (struct dicey_view) { .data = (void*) sel.trait, .len = (size_t) trait_len },
        (struct dicey_view) { .data = (void*) sel.elem, .len = (size_t) elem_len },
    };

    return dtf_bytes_writer_write_chunks(writer, chunks, sizeof chunks / sizeof *chunks);
}

ptrdiff_t dtf_bytes_writer_write_zstring(const struct dtf_bytes_writer writer, const char *const str) {
    const ptrdiff_t size = dutl_zstring_size(str);

    if (size < 0) {
        return size;
    }

    struct dicey_view data = {
        .data = (void*) str,
        .len = (uint32_t) size,
    };

    return dtf_bytes_writer_write(writer, data);
} 
