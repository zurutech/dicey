#if !defined(WMDYUOXBZM_WRITER_H)
#define WMDYUOXBZM_WRITER_H

#include <stddef.h>

#include <dicey/types.h>

struct dtf_bytes_writer {
    void *context;
    ptrdiff_t (*write)(void *context, struct dicey_view data);
};

struct dtf_bytes_writer dtf_bytes_writer_new(struct dicey_view_mut *buffer);
struct dtf_bytes_writer dtf_bytes_writer_new_sizer(ptrdiff_t *size);

bool dtf_bytes_writer_is_valid(struct dtf_bytes_writer writer);
ptrdiff_t dtf_bytes_writer_write(struct dtf_bytes_writer writer, struct dicey_view data);

ptrdiff_t dtf_bytes_writer_write_chunks(
    struct dtf_bytes_writer writer,
    const struct dicey_view *chunks,
    size_t nchunks
);

ptrdiff_t dtf_bytes_writer_write_selector(struct dtf_bytes_writer writer, struct dicey_selector sel);
ptrdiff_t dtf_bytes_writer_write_zstring(struct dtf_bytes_writer writer, const char *const str);

#endif // WMDYUOXBZM_WRITER_H
