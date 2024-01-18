#if !defined(CNHZVJKDMF_DTF_VALUE_H)
#define CNHZVJKDMF_DTF_VALUE_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/builders.h>
#include <dicey/packet.h>
#include <dicey/types.h>

#include "to.h"

#define DTF_SIZE_DYNAMIC PTRDIFF_MAX

struct dtf_valueres {
    ptrdiff_t result;
    size_t size;
    struct dtf_value* value;
};

ptrdiff_t dtf_value_estimate_size(const struct dicey_arg *item);
struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dicey_arg *item);

#endif // CNHZVJKDMF_DTF_VALUE_H
