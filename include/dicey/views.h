#if !defined(KWHQWHOQKQ_TYPES_H)
#define KWHQWHOQKQ_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#if defined (__cplusplus)
extern "C" {
#endif

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

ptrdiff_t dicey_view_advance(struct dicey_view *view, ptrdiff_t offset);
ptrdiff_t dicey_view_as_zstring(struct dicey_view *view, const char **str);

static inline struct dicey_view dicey_view_from(const void *const data, const size_t len) {
    return (struct dicey_view) { .len = len, .data = data };
}

static inline struct dicey_view dicey_view_from_mut(const struct dicey_view_mut view) {
    return (struct dicey_view) { .len = view.len, .data = view.data };
}

static inline bool dicey_view_is_valid(const struct dicey_view view) {
    return view.data;
}

static inline bool dicey_view_is_empty(const struct dicey_view view) {
    return !dicey_view_is_valid(view) || view.len == 0;
}

ptrdiff_t dicey_view_read(struct dicey_view *view, struct dicey_view_mut dest);
ptrdiff_t dicey_view_take(struct dicey_view *view, ptrdiff_t nbytes, struct dicey_view *slice);

ptrdiff_t dicey_view_mut_advance(struct dicey_view_mut *view, ptrdiff_t offset);
ptrdiff_t dicey_view_mut_ensure_cap(struct dicey_view_mut *dest, size_t required);

static inline struct dicey_view_mut dicey_view_mut_from(void *const data, const size_t len) {
    return (struct dicey_view_mut) { .len = len, .data = data };
}

static inline bool dicey_view_mut_is_valid(const struct dicey_view_mut view) {
    return view.data;
}

static inline bool dicey_view_mut_is_empty(const struct dicey_view_mut view) {
    return !dicey_view_mut_is_valid(view) || view.len == 0;
}

ptrdiff_t dicey_view_mut_write(struct dicey_view_mut *dest, struct dicey_view view);
ptrdiff_t dicey_view_mut_write_chunks(struct dicey_view_mut *dest, const struct dicey_view *chunks, size_t nchunks);
ptrdiff_t dicey_view_mut_write_zstring(struct dicey_view_mut *dest, const char *str);

#if defined(__cplusplus)
}
#endif


#endif // KWHQWHOQKQ_TYPES_H
