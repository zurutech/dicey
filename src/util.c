#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/types.h>

#include "util.h"

ptrdiff_t dutl_buffer_sizeof(const struct dicey_view view) {
    if (view.len > UINT32_MAX) {
        return DICEY_EOVERFLOW;
    }

    uint32_t len = (uint32_t) view.len;

    uint32_t size = sizeof len;

    if (!dutl_u32_add(&size, size, len)) {
        return DICEY_EOVERFLOW;
    }

    return size;    
}

bool dutl_u32_add(uint32_t *const res, const uint32_t a, const uint32_t b) {
    const uint32_t sum = a + b;

    if (sum < a) {
        return false;
    }

    *res = sum;

    return true;
}

void dutl_write_buffer(void **const dest, struct dicey_view view) {
    if (view.len) {
        assert(view.len <= UINT32_MAX);

        uint32_t len = (uint32_t) view.len;

        dutl_write_bytes(dest, (struct dicey_view) { .data = &len, .len = sizeof len });
        dutl_write_bytes(dest, view);
    }
}

void dutl_write_bytes(void **const dest, const struct dicey_view view) {
    memcpy(*dest, view.data, view.len);

    *dest = (uint8_t*) *dest + view.len;
}

void dutl_write_chunks(void **const dest, const struct dicey_view *const chunks, const size_t nchunks) {
    const struct dicey_view *const end = chunks + nchunks;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        dutl_write_bytes(dest, *chunk);
    }
}

ptrdiff_t dutl_zstring_sizeof(const char *const str) {
    const size_t len = strlen(str);

    if (len > UINT32_MAX) {
        return DICEY_EOVERFLOW;
    }

    return (ptrdiff_t) len + 1U;
}
