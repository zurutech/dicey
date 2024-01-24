#if !defined(DTF_DHCBDDHD_H)
#define DTF_DHCBDDHD_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <stddef.h>
#include <stdint.h>

#include <dicey/builders.h>
#include <dicey/packet.h>
#include <dicey/value.h>
#include <dicey/views.h>

#include "to.h"

enum dtf_payload_kind {
    DTF_PAYLOAD_INVALID = DICEY_PACKET_KIND_INVALID,

    DTF_PAYLOAD_HELLO   = DICEY_PACKET_KIND_HELLO,
    DTF_PAYLOAD_BYE     = DICEY_PACKET_KIND_BYE,

    DTF_PAYLOAD_GET      = DICEY_MESSAGE_TYPE_GET,
    DTF_PAYLOAD_SET      = DICEY_MESSAGE_TYPE_SET,
    DTF_PAYLOAD_EXEC     = DICEY_MESSAGE_TYPE_EXEC,
    DTF_PAYLOAD_EVENT    = DICEY_MESSAGE_TYPE_EVENT,
    DTF_PAYLOAD_RESPONSE = DICEY_MESSAGE_TYPE_RESPONSE,
};

static inline bool dtf_payload_kind_is_message(enum dtf_payload_kind kind) {
    return kind >= DTF_PAYLOAD_GET;
}

struct dtf_result {
    ptrdiff_t result;
    size_t size;
    void* data;
};

struct dtf_result dtf_bye_write(struct dicey_view_mut dest, uint32_t seq, uint32_t reason);
struct dtf_result dtf_hello_write(struct dicey_view_mut dest, uint32_t seq, uint32_t version);

ptrdiff_t dtf_message_estimate_size(
    enum dtf_payload_kind kind,
    const char *path,
    struct dicey_selector selector,
    const struct dicey_arg *value
);

ptrdiff_t dtf_message_get_size(const struct dtf_message *msg);
ptrdiff_t dtf_message_get_trailer_size(const struct dtf_message *msg);

struct dtf_message_content {
    const char *path;
    struct dicey_selector selector;
    
    const struct dtf_value *value;
    size_t value_len;
};

ptrdiff_t dtf_message_get_content(const struct dtf_message *msg, size_t alloc_len, struct dtf_message_content *dest);

ptrdiff_t dtf_message_get_path(const struct dtf_message *msg, size_t alloc_len, const char **dest);
ptrdiff_t dtf_message_get_selector(const struct dtf_message *msg, size_t alloc_len, struct dicey_selector *dest);
ptrdiff_t dtf_message_get_value(const struct dtf_message *msg, size_t alloc_len, struct dicey_arg *dest);

struct dtf_result dtf_message_write(
    struct dicey_view_mut dest,
    enum dtf_payload_kind kind,
    uint32_t seq,
    const char *path,
    struct dicey_selector selector,
    const struct dicey_arg* value
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

struct dtf_result dtf_payload_load(union dtf_payload *dest, struct dicey_view *src);

#endif // DTF_DHCBDDHD_H
