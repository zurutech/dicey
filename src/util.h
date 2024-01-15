#if !defined(IVJHUOXLEC_UTIL_H)
#define IVJHUOXLEC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#if defined (__cplusplus)
extern "C" {
#endif

bool dutl_size_add(size_t *res, size_t a, size_t b);
bool dutl_ssize_add(ptrdiff_t *res, ptrdiff_t a, ptrdiff_t b);
bool dutl_u32_add(uint32_t *res, uint32_t a, uint32_t b);
ptrdiff_t dutl_zstring_size(const char *str);

#if defined(__cplusplus)
}
#endif


#endif // IVJHUOXLEC_UTIL_H
