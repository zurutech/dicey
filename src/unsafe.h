#if !defined(GQWOPCFNUH_UNSAFE_H)
#define GQWOPCFNUH_UNSAFE_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#if defined (__cplusplus)
extern "C" {
#endif

void dunsafe_write_bytes(void **dest, struct dicey_view view);
void dunsafe_write_chunks(void **dest, const struct dicey_view *chunks, size_t nchunks);
void dunsafe_write_double(void **dest, double value);
void dunsafe_write_i64(void **dest, int64_t value);
void dunsafe_write_u8(void **dest, uint8_t value);
void dunsafe_write_u16(void **dest, uint16_t value);
void dunsafe_write_u32(void **dest, uint32_t value);

#if defined(__cplusplus)
}
#endif


#endif // GQWOPCFNUH_UNSAFE_H
