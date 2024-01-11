#if !defined(IVJHUOXLEC_UTIL_H)
#define IVJHUOXLEC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#if defined (__cplusplus)
extern "C" {
#endif

ptrdiff_t dutl_buffer_sizeof(struct dicey_view view);
bool dutl_u32_add(uint32_t *res, uint32_t a, uint32_t b);
void dutl_write_buffer(void **dest, struct dicey_view view);
void dutl_write_bytes(void **dest, struct dicey_view view);
void dutl_write_chunks(void **dest, const struct dicey_view *chunks, size_t nchunks);
ptrdiff_t dutl_zstring_sizeof(const char *str);


#if defined(__cplusplus)
}
#endif


#endif // IVJHUOXLEC_UTIL_H
