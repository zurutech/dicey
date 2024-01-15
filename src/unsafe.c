#include <stdint.h>
#include <string.h>

#include <dicey/types.h>

#include "unsafe.h"

#define TRIVIAL_WRITE_IMPL(NAME, TYPE) \
    void dunsafe_write_##NAME(void **const dest, const TYPE value) { \
        dunsafe_write_bytes(dest, (struct dicey_view) { .data = &value, .len = sizeof value }); \
    }

void dunsafe_write_bytes(void **const dest, const struct dicey_view view) {
    memcpy(*dest, view.data, view.len);

    *dest = (uint8_t*) *dest + view.len;
}

void dunsafe_write_chunks(void **const dest, const struct dicey_view *const chunks, const size_t nchunks) {
    const struct dicey_view *const end = chunks + nchunks;

    for (const struct dicey_view *chunk = chunks; chunk < end; ++chunk) {
        dunsafe_write_bytes(dest, *chunk);
    }
}

TRIVIAL_WRITE_IMPL(i64, int64_t)
TRIVIAL_WRITE_IMPL(u8, uint8_t)
TRIVIAL_WRITE_IMPL(u16, uint16_t)
TRIVIAL_WRITE_IMPL(u32, uint32_t)
