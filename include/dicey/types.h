#if !defined(KWHQWHOQKQ_TYPES_H)
#define KWHQWHOQKQ_TYPES_H

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

static inline struct dicey_view dicey_view_from(const void *const data, const size_t len) {
    return (struct dicey_view) { .len = len, .data = data };
}

static inline struct dicey_view_mut dicey_view_mut_from(void *const data, const size_t len) {
    return (struct dicey_view_mut) { .len = len, .data = data };
}

#if defined(__cplusplus)
}
#endif


#endif // KWHQWHOQKQ_TYPES_H
