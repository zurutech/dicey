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
#include <dicey/value.h>
#include <dicey/views.h>

#include "to.h"
#include "writer.h"

#define DTF_SIZE_DYNAMIC PTRDIFF_MAX
#define DICEY_VARIANT_ID 'v'

union dtf_probed_data {
    dicey_bool boolean;
    dicey_byte byte;

    dicey_float floating;

    dicey_i16 i16;
    dicey_i32 i32;
    dicey_i64 i64;

    dicey_u16 u16;
    dicey_u32 u32;
    dicey_u64 u64;

    struct dtf_probed_list {
        uint16_t inner_type;
        uint16_t nitems;
        struct dicey_view data;
    } list;

    struct dtf_probed_bytes {
        uint32_t len;
        const uint8_t *data;
    } bytes;

    const char *str;// for str, path
    struct dicey_selector selector;

    struct dicey_errmsg error;
};

struct dtf_probed_value {
    enum dicey_type type;
    union dtf_probed_data data;
};

ptrdiff_t dtf_selector_from(struct dicey_selector *sel, struct dicey_view *src);

struct dtf_valueres {
    ptrdiff_t result;
    size_t size;
    struct dtf_value* value;
};

ptrdiff_t dtf_value_estimate_size(const struct dicey_arg *item);

ptrdiff_t dtf_value_probe(struct dicey_view *src, struct dtf_probed_value *info);
ptrdiff_t dtf_value_probe_as(enum dicey_type type, struct dicey_view *src, union dtf_probed_data *info);

struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dicey_arg *item);
ptrdiff_t dtf_value_write_to(struct dtf_bytes_writer *writer, const struct dicey_arg *item);

#endif // CNHZVJKDMF_DTF_VALUE_H
