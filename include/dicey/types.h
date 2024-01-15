#if !defined(KWHQWHOQKQ_TYPES_H)
#define KWHQWHOQKQ_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#if defined (__cplusplus)
extern "C" {
#endif

struct dicey_selector {
    const char *trait;
    const char *elem;
};

struct dicey_view {
    size_t len;
    const void *data;
};

struct dicey_view_mut {
    size_t len;
    void *data;
};

#define DICEY_NULL ((struct dicey_view_mut) { .len = 0, .data = NULL })
#define DICEY_CNULL ((struct dicey_view) { .len = 0, .data = NULL })

ptrdiff_t dicey_selector_size(struct dicey_selector sel);

static inline struct dicey_view dicey_view_from(const void *const data, const size_t len) {
    return (struct dicey_view) { .len = len, .data = data };
}

static inline struct dicey_view_mut dicey_view_mut_from(void *const data, const size_t len) {
    return (struct dicey_view_mut) { .len = len, .data = data };
}

ptrdiff_t dicey_view_mut_advance(struct dicey_view_mut *view, size_t offset);
ptrdiff_t dicey_view_mut_ensure_cap(struct dicey_view_mut *dest, size_t required);
ptrdiff_t dicey_view_mut_write(struct dicey_view_mut *dest, struct dicey_view view);
ptrdiff_t dicey_view_mut_write_chunks(struct dicey_view_mut *dest, const struct dicey_view *chunks, size_t nchunks);
ptrdiff_t dicey_view_mut_write_selector(struct dicey_view_mut *dest, struct dicey_selector sel); 
ptrdiff_t dicey_view_mut_write_zstring(struct dicey_view_mut *dest, const char *str);

#if defined(__cplusplus)
}
#endif


#endif // KWHQWHOQKQ_TYPES_H
