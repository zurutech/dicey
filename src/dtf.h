#include <sys/types.h>
#if !defined(LEL_H)
#define LEL_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// this enum uses same values as errno.h for comparable value. While the errno values are not defined by the C standard,
// they are defined by POSIX, Win32 and C++, and widely used. We use them here to make it easier to integrate with
// existing code.
enum dtf_error {
    DTF_OK = 0,
    DTF_EAGAIN    = -EAGAIN,
    DTF_ENOMEM    = -ENOMEM,
    DTF_EINVAL    = -EINVAL,
    DTF_EBADMSG   = -EBADMSG,
    DTF_EOVERFLOW = -EOVERFLOW,
};

const char* dtf_strerror(int errnum);

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

enum dtf_type {
    DTF_TYPE_INVALID = 0,
    DTF_TYPE_BOOL,
    DTF_TYPE_BYTE,
    DTF_TYPE_FLOAT,
    DTF_TYPE_INT,
    DTF_TYPE_ARRAY,
    DTF_TYPE_TUPLE,     // represented as array of variants
    DTF_TYPE_BYTES,     // optimized array of bytes
    DTF_TYPE_PATH,      // alias for string
    DTF_TYPE_SELECTOR,  
    DTF_TYPE_VARIANT,
};

#define DTF_BYTE_ARRAY_FIELDS \
    uint32_t len; \
    char data[];

struct dtf_array {
    uint32_t type;
    
    DTF_BYTE_ARRAY_FIELDS
};

struct dtf_bytes {
    DTF_BYTE_ARRAY_FIELDS
};

struct dtf_tuple {
    uint32_t len;
    char data[];
};

typedef uint8_t dtf_bool;
typedef int64_t dtf_int;
typedef double dtf_float;

struct dtf_value {
    uint32_t type;
    char data[];
};

struct dtf_view {
    uint32_t len;
    void *data;
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

    char data[];
};

#define DTF_MSG_LEN(MSG) ((sizeof (MSG)->head) + (MSG)->head.data_len)
#define DTF_MSG_PATH(MSG) ((struct dtf_view) { .len = (MSG)->head.path, .data = (MSG)->data })

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

struct dtf_craftres dtf_craft_message(enum dtf_msgkind kind, struct dtf_view path, struct dtf_view selector, struct dtf_view value);

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

struct dtf_loadres dtf_load(const char *data, size_t len);
struct dtf_bytes* dtf_sprintf(const char *fmt, ...);

#if defined(__cplusplus)
}
#endif

#endif // LEL_H
