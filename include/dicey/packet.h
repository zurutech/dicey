#if !defined(XYDQQUJZAI_PACKET_H)
#define XYDQQUJZAI_PACKET_H

#include <stddef.h>
#include <stdint.h>

#include "errors.h"
#include "value.h"
#include "views.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum dicey_bye_reason {
    DICEY_BYE_REASON_INVALID = 0,
    DICEY_BYE_REASON_SHUTDOWN = 1,
    DICEY_BYE_REASON_ERROR = 3,
};

const char* dicey_bye_reason_to_string(enum dicey_bye_reason reason);

enum dicey_op {
    DICEY_OP_INVALID = 0,

    DICEY_OP_GET      = '<',
    DICEY_OP_SET      = '>',
    DICEY_OP_EXEC     = '?',
    DICEY_OP_EVENT    = '!',
    DICEY_OP_RESPONSE = ':',
};

const char* dicey_message_type_to_string(enum dicey_op type);

enum dicey_packet_kind {
    DICEY_PACKET_KIND_INVALID = 0,
        
    DICEY_PACKET_KIND_HELLO,
    DICEY_PACKET_KIND_BYE,
    DICEY_PACKET_KIND_MESSAGE,
};

bool dicey_packet_kind_is_valid(enum dicey_packet_kind kind);

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

struct dicey_message {
    enum dicey_op type;
    const char *path;
    struct dicey_selector selector;
    struct dicey_value value;
};

struct dicey_packet {
    void *payload;
    size_t nbytes;
};

enum dicey_error dicey_packet_load(struct dicey_packet *packet, const void **data, size_t *nbytes);

enum dicey_error dicey_packet_as_bye(struct dicey_packet packet, struct dicey_bye *bye);
enum dicey_error dicey_packet_as_hello(struct dicey_packet packet, struct dicey_hello *hello);
enum dicey_error dicey_packet_as_message(struct dicey_packet packet, struct dicey_message *message);
void dicey_packet_deinit(struct dicey_packet *packet);
enum dicey_error dicey_packet_dump(struct dicey_packet packet, void **data, size_t *nbytes);
enum dicey_packet_kind dicey_packet_get_kind(struct dicey_packet packet);
enum dicey_error dicey_packet_get_seq(struct dicey_packet packet, uint32_t *seq);
bool dicey_packet_is_valid(struct dicey_packet packet);

enum dicey_error dicey_packet_bye(struct dicey_packet *dest, uint32_t seq, enum dicey_bye_reason reason);
enum dicey_error dicey_packet_hello(struct dicey_packet *dest, uint32_t seq, struct dicey_version version);

#if defined(__cplusplus)
}
#endif


#endif // XYDQQUJZAI_PACKET_H
