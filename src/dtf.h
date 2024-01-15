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

enum dtf_msgkind {
    DTF_MSGKIND_INVALID = 0x00,
    DTF_MSGKIND_HELLO   = 0x01,
    DTF_MSGKIND_BYE     = 0x02,

    DTF_MSGKIND_GET      = 0x10,
    DTF_MSGKIND_SET      = 0x11,
    DTF_MSGKIND_EXEC     = 0x12,
    DTF_MSGKIND_EVENT    = 0x13,
    DTF_MSGKIND_RESPONSE = 0x14,
};

#define DTF_MSG_LEN(MSG) ((sizeof (MSG)->head) + (MSG)->head.data_len)
#define DTF_MSG_PATH(MSG) ((struct dicey_view) { .len = (MSG)->head.path, .data = (MSG)->data })

struct dtf_msgres {
    ptrdiff_t result;
    size_t size;
    struct dtf_message* msg;
};

ptrdiff_t dtf_message_estimate_size(
    enum dtf_msgkind kind,
    const char *path,
    struct dicey_selector selector,
    const struct dtf_item *value
);

struct dtf_msgres dtf_message_write(
    struct dicey_view_mut dest,
    enum dtf_msgkind kind,
    uint32_t tid,
    const char *path,
    struct dicey_selector selector,
    const struct dtf_item* value
);

int dtf_selector_load_from(struct dicey_selector *selector, struct dicey_view src);

struct dtf_loadres {
    // result will be:
    // 0 if the message is invalid
    // >0, and a valid value of dtf_msgkind if the message is valid
    // <0, and a valid value of dtf_error if an error occured
    ptrdiff_t result;
    const char *remainder;

    union {
        struct dtf_message* msg;
        struct dtf_hello* hello;
        struct dtf_bye* bye;
    };
};

struct dtf_loadres dtf_payload_load(const char *data, size_t len);

#if defined(_MSC_VER)
#pragma warning(default: 4200)
#endif

#endif // DTF_DHCBDDHD_H
