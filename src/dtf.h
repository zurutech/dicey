#include <sys/types.h>
#if !defined(DTF_DHCBDDHD_H)
#define DTF_DHCBDDHD_H

#if defined(__cplusplus)
#  error "This header is not meant to be included from C++"
#endif

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/types.h>

#if defined(_MSC_VER)
#pragma warning(disable: 4200)
#endif

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

#define DTF_PAYLOAD_HEAD \
    uint32_t kind; \
    uint32_t tid;

struct dtf_payload_head {
    DTF_PAYLOAD_HEAD
};

struct dtf_message_head {
    DTF_PAYLOAD_HEAD

    uint32_t data_len;
};

struct dtf_message {
    struct dtf_message_head head;

    uint8_t data[];
};

#define DTF_MSG_LEN(MSG) ((sizeof (MSG)->head) + (MSG)->head.data_len)
#define DTF_MSG_PATH(MSG) ((struct dicey_view) { .len = (MSG)->head.path, .data = (MSG)->data })

struct dtf_hello {
    DTF_PAYLOAD_HEAD

    uint32_t version;
    uint32_t id;
};

struct dtf_bye {
    DTF_PAYLOAD_HEAD
};

struct dtf_craftres {
    int result;
    struct dtf_message* msg;
};

struct dtf_craftres dtf_craft_message(enum dtf_msgkind kind, const char *path, struct dicey_selector selector, struct dicey_view value);
struct dtf_craftres dtf_craft_message_to(struct dicey_view_mut dest, enum dtf_msgkind kind, const char *path, struct dicey_selector selector, struct dicey_view value);

int dtf_craft_selector_to(struct dicey_view_mut dest, struct dicey_selector selector);

ptrdiff_t dtf_estimate_message_size(enum dtf_msgkind kind, const char *path, struct dicey_selector selector, struct dicey_view value);

int dtf_load_selector_from(struct dicey_selector *selector, struct dicey_view src);

struct dtf_loadres {
    // result will be:
    // 0 if the message is invalid
    // >0, and a valid value of dtf_msgkind if the message is valid
    // <0, and a valid value of dtf_error if an erro occured
    int result;
    const char *remainder;

    union {
        struct dtf_message* msg;
        struct dtf_hello* hello;
        struct dtf_bye* bye;
    };
};

struct dtf_loadres dtf_load_message(const char *data, size_t len);

#if defined(_MSC_VER)
#pragma warning(default: 4200)
#endif

#endif // DTF_DHCBDDHD_H
