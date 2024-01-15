#include "dtf-to.h"
#include <sys/types.h>
#if !defined(DTF_DHCBDDHD_H)
#define DTF_DHCBDDHD_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#include "dtf-value.h"

enum dtf_payload_kind {
    DTF_PAYLOAD_INVALID = 0x00,
    DTF_PAYLOAD_HELLO   = 0x01,
    DTF_PAYLOAD_BYE     = 0x02,

    DTF_PAYLOAD_GET      = 0x10,
    DTF_PAYLOAD_SET      = 0x11,
    DTF_PAYLOAD_EXEC     = 0x12,
    DTF_PAYLOAD_EVENT    = 0x13,
    DTF_PAYLOAD_RESPONSE = 0x14,
};

static inline bool dtf_payload_kind_is_message(enum dtf_payload_kind kind) {
    return kind >= DTF_PAYLOAD_GET;
}

struct dtf_msgres {
    ptrdiff_t result;
    size_t size;
    struct dtf_message* msg;
};

ptrdiff_t dtf_message_estimate_size(
    enum dtf_payload_kind kind,
    const char *path,
    struct dicey_selector selector,
    const struct dtf_item *value
);

ptrdiff_t dtf_message_get_size(const struct dtf_message *msg);

struct dtf_msgres dtf_message_write(
    struct dicey_view_mut dest,
    enum dtf_payload_kind kind,
    uint32_t seq,
    const char *path,
    struct dicey_selector selector,
    const struct dtf_item* value
);

int dtf_selector_load_from(struct dicey_selector *selector, struct dicey_view src);

union dtf_payload {
    struct dtf_payload_head* header;
    
    struct dtf_message* msg;
    struct dtf_hello* hello;
    struct dtf_bye* bye;
};

enum dtf_payload_kind dtf_payload_get_kind(union dtf_payload msg); 
ptrdiff_t dtf_payload_get_seq(union dtf_payload msg);

struct dtf_loadres {
    // result will be:
    // 0 if the message is invalid
    // >0, and a valid value of dtf_msgkind if the message is valid
    // <0, and a valid value of dtf_error if an error occured
    ptrdiff_t result;
    struct dicey_view remainder;

    union dtf_payload payload;
};

struct dtf_loadres dtf_payload_load(struct dicey_view src);

#if defined(_MSC_VER)
#pragma warning(default: 4200)
#endif

#endif // DTF_DHCBDDHD_H
