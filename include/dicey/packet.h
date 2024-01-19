#include <stddef.h>
#if !defined(XYDQQUJZAI_PACKET_H)
#define XYDQQUJZAI_PACKET_H

#include <stdint.h>

#include "errors.h"
#include "types.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef uint8_t dicey_bool;
typedef int64_t dicey_int;
typedef double dicey_float;

enum dicey_bye_reason {
    DICEY_BYE_REASON_INVALID = 0,
    DICEY_BYE_REASON_SHUTDOWN = 1,
    DICEY_BYE_REASON_ERROR = 3,
};

enum dicey_packet_kind {
    DICEY_PACKET_KIND_INVALID = 0,
        
    DICEY_PACKET_KIND_HELLO,
    DICEY_PACKET_KIND_BYE,
    DICEY_PACKET_KIND_MESSAGE,
};

enum dicey_message_type {
    DICEY_MESSAGE_TYPE_INVALID = 0,

    DICEY_MESSAGE_TYPE_GET = 0x10,
    DICEY_MESSAGE_TYPE_SET,
    DICEY_MESSAGE_TYPE_EXEC,
    DICEY_MESSAGE_TYPE_EVENT,
    DICEY_MESSAGE_TYPE_RESPONSE,
};

enum dicey_type {
    DICEY_TYPE_INVALID = 0,

    DICEY_TYPE_UNIT  = 'u',
    DICEY_TYPE_BOOL  = 'b',
    DICEY_TYPE_FLOAT = 'f',
    DICEY_TYPE_INT   = 'i',

    DICEY_TYPE_ARRAY = '[', // an array of elements
    DICEY_TYPE_TUPLE = '(', // an array of variable elements
    DICEY_TYPE_PAIR  = '{', // specialised tuple of two elements
    DICEY_TYPE_BYTES = 'y', // optimized array of bytes
    DICEY_TYPE_STR   = 's', // alias for a null terminated byte array

    DICEY_TYPE_PATH     = '@', // alias for str
    DICEY_TYPE_SELECTOR = '%', // an optimized tuple of two strings

    DICEY_TYPE_ERROR = 'e',
};

bool dicey_type_is_valid(enum dicey_type type);
const char* dicey_type_name(enum dicey_type type);

struct dicey_version {
    uint16_t major;
    uint16_t revision;
};

struct dicey_bye {
    enum dicey_bye_reason reason;
};

struct dicey_hello {
    struct dicey_version version;
};

struct dicey_value {
    enum dicey_type type;

    const struct dtf_value *_data;
};

struct dicey_message {
    enum dicey_message_type type;
    const char *path;
    struct dicey_selector selector;
    struct dicey_value value;

    const struct dtf_message *_data;
};

struct dicey_packet {
    void *payload;
    size_t nbytes;
};

enum dicey_error dicey_packet_as_bye(struct dicey_packet packet, struct dicey_bye *bye);
enum dicey_error dicey_packet_as_hello(struct dicey_packet packet, struct dicey_hello *hello);
enum dicey_error dicey_packet_as_message(struct dicey_packet packet, struct dicey_message *message);
void dicey_packet_deinit(struct dicey_packet *packet);
enum dicey_packet_kind dicey_packet_get_kind(struct dicey_packet packet);
enum dicey_error dicey_packet_get_seq(struct dicey_packet packet, uint32_t *seq);

enum dicey_error dicey_packet_bye(struct dicey_packet *dest, uint32_t seq, enum dicey_bye_reason reason);
enum dicey_error dicey_packet_hello(struct dicey_packet *dest, uint32_t seq, struct dicey_version version);

#if defined(__cplusplus)
}
#endif


#endif // XYDQQUJZAI_PACKET_H
