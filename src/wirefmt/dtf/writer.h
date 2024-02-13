// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(WMDYUOXBZM_WRITER_H)
#define WMDYUOXBZM_WRITER_H

#include <stddef.h>

#include <dicey/internal/views.h>
#include <dicey/value.h>

enum dtf_bytes_writer_kind {
    DTF_BYTES_WRITER_KIND_INVALID,

    DTF_BYTES_WRITER_KIND_BUFFER, // writes on struct dicey_view_mut *
    DTF_BYTES_WRITER_KIND_SIZER,  // dummy writer that just counts bytes
};

union dtf_bytes_writer_state {
    struct dicey_view_mut buffer; // for DTF_BYTES_WRITER_KIND_BUFFER
    ptrdiff_t size;               // for DTF_BYTES_WRITER_KIND_SIZER
};

struct dtf_bytes_writer {
    enum dtf_bytes_writer_kind kind;
    union dtf_bytes_writer_state state;
};

struct dtf_bytes_writer dtf_bytes_writer_new(struct dicey_view_mut buffer);
struct dtf_bytes_writer dtf_bytes_writer_new_sizer(void);

enum dtf_bytes_writer_kind dtf_bytes_writer_get_kind(const struct dtf_bytes_writer *writer);
union dtf_bytes_writer_state dtf_bytes_writer_get_state(const struct dtf_bytes_writer *writer);
bool dtf_bytes_writer_is_valid(const struct dtf_bytes_writer *writer);
ptrdiff_t dtf_bytes_writer_snapshot(const struct dtf_bytes_writer *writer, struct dtf_bytes_writer *clone);
ptrdiff_t dtf_bytes_writer_write(struct dtf_bytes_writer *writer, struct dicey_view data);

ptrdiff_t dtf_bytes_writer_write_chunks(
    struct dtf_bytes_writer *writer,
    const struct dicey_view *chunks,
    size_t nchunks
);

ptrdiff_t dtf_bytes_writer_write_selector(struct dtf_bytes_writer *writer, struct dicey_selector sel);
ptrdiff_t dtf_bytes_writer_write_zstring(struct dtf_bytes_writer *writer, const char *const str);

#endif // WMDYUOXBZM_WRITER_H
