#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/errors.h>
#include <dicey/packet.h>
#include <dicey/types.h>

#include "dtf/dtf.h"

static bool bye_reason_is_valid(const enum dicey_bye_reason reason) {
    switch (reason) {
    default:
        assert(false);

    case DICEY_BYE_REASON_INVALID:
        return false;

    case DICEY_BYE_REASON_SHUTDOWN:
    case DICEY_BYE_REASON_ERROR:
        return true;
    }
}

static enum dicey_message_type msgkind_from_dtf(const ptrdiff_t kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
    case DTF_PAYLOAD_HELLO:
    case DTF_PAYLOAD_BYE:
        return DICEY_MESSAGE_TYPE_INVALID;

    case DTF_PAYLOAD_SET:
        return DICEY_MESSAGE_TYPE_SET;

    case DTF_PAYLOAD_GET:
        return DICEY_MESSAGE_TYPE_GET;

    case DTF_PAYLOAD_EXEC:
        return DICEY_MESSAGE_TYPE_EXEC;

    case DTF_PAYLOAD_EVENT:
        return DICEY_MESSAGE_TYPE_EVENT;

    case DTF_PAYLOAD_RESPONSE:
        return DICEY_MESSAGE_TYPE_RESPONSE;
    }
}

static bool packet_is_valid(const struct dicey_packet *const packet) {
    assert(packet);

    return packet->payload && packet->nbytes;
}

static enum dicey_packet_kind pktkind_from_dtf(const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
        return DICEY_PACKET_KIND_INVALID;

    case DTF_PAYLOAD_HELLO:
        return DICEY_PACKET_KIND_HELLO;

    case DTF_PAYLOAD_BYE:
        return DICEY_PACKET_KIND_BYE;

    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return DICEY_PACKET_KIND_MESSAGE;
    }
}

static struct dicey_version version_from_dtf(const uint32_t version) {
    return (struct dicey_version) {
        .major = (uint16_t) (version >> (sizeof(uint16_t) * CHAR_BIT)),
        .revision = (uint16_t) (version & UINT16_MAX),
    };
}

static uint32_t version_to_dtf(const struct dicey_version version) {
    return (uint32_t) (version.major << (sizeof(uint16_t) * CHAR_BIT)) | version.revision;
}

enum dicey_error dicey_packet_as_bye(const struct dicey_packet packet, struct dicey_bye *const bye) {
    assert(packet_is_valid(&packet) && bye);

    const union dtf_payload *const payload = packet.payload;

    if (dtf_payload_get_kind(*payload) != DTF_PAYLOAD_BYE) {
        return DICEY_EINVAL;
    }

    const uint32_t reason = payload->bye->reason;

    if (!bye_reason_is_valid(reason)) {
        return DICEY_EINVAL;
    }

    *bye = (struct dicey_bye) {
        .reason = reason,
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_as_hello(const struct dicey_packet packet, struct dicey_hello *const hello) {
    assert(packet_is_valid(&packet) && hello);

    const union dtf_payload *const payload = packet.payload;

    if (dtf_payload_get_kind(*payload) != DTF_PAYLOAD_HELLO) {
        return DICEY_EINVAL;
    }

    *hello = (struct dicey_hello) {
        .version = version_from_dtf(payload->hello->version),
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_as_message(const struct dicey_packet packet, struct dicey_message *const message) {
    assert(packet_is_valid(&packet) && message);

    const union dtf_payload *const payload = packet.payload;

    const enum dtf_payload_kind pl_kind = dtf_payload_get_kind(*payload);
    
    if (!dtf_payload_kind_is_message(pl_kind)) {
        return DICEY_EINVAL;
    }

    const enum dicey_message_type type = msgkind_from_dtf(pl_kind);
    if (type == DICEY_MESSAGE_TYPE_INVALID) {
        return DICEY_EINVAL;
    }

    const struct dtf_message *const msg = payload->msg;

    struct dtf_message_content content = {0};

    const ptrdiff_t content_res = dtf_message_get_content(msg, packet.nbytes, &content);
    if (content_res < 0) {
        return content_res;
    }

    *message = (struct dicey_message) {
        .type = type,
        .path = content.path,
        .selector = content.selector,
        .value = {0}, // todo
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_bye(struct dicey_packet *const dest, const uint32_t seq, const enum dicey_bye_reason reason) {
    assert(dest && bye_reason_is_valid(reason));

    struct dtf_bye *const bye = calloc(sizeof *bye, 1U);
    if (!bye) {
        return DICEY_ENOMEM;
    }
    
    const struct dtf_writeres write_res = dtf_bye_write(
        (struct dicey_view_mut) {
            .data = bye,
            .len = sizeof *bye,
        },
        seq,
        reason
    );

    if (write_res.result < 0) {
        free(bye);

        return write_res.result;
    }

    *dest = (struct dicey_packet) {
        .payload = bye,
        .nbytes = sizeof *bye,
    };

    return DICEY_OK;
}

void dicey_packet_deinit(struct dicey_packet *const packet) {
    if (packet) {
        // not UB: the payload is always allocated with {c,m}alloc so it's originally void*
        free((void*) packet->payload);

        *packet = (struct dicey_packet) {0};
    }
}

enum dicey_packet_kind dicey_packet_get_kind(const struct dicey_packet packet) {
    assert(packet_is_valid(&packet));

    const enum dtf_payload_kind dtf_kind = dtf_payload_get_kind((union dtf_payload) {
        .header = packet.payload,
    });

    return pktkind_from_dtf(dtf_kind);
}

enum dicey_error dicey_packet_get_seq(const struct dicey_packet packet, uint32_t *const seq) {
    assert(packet_is_valid(&packet) && seq);

    const ptrdiff_t seq_val = dtf_payload_get_seq((union dtf_payload) {
        .header = packet.payload,
    });

    if (seq_val < 0) {
        return seq_val;
    }

    *seq = (uint32_t) seq_val;

    return DICEY_OK;
}

enum dicey_error dicey_packet_hello(struct dicey_packet *const dest, const uint32_t seq, const struct dicey_version version) {
    assert(dest);

    struct dtf_hello *const hello = calloc(sizeof *hello, 1U);
    if (!hello) {
        return DICEY_ENOMEM;
    }

    const struct dtf_writeres write_res = dtf_hello_write(
        (struct dicey_view_mut) {
            .data = hello,
            .len = sizeof *hello,
        },
        seq,
        version_to_dtf(version)
    );

    if (write_res.result < 0) {
        free(hello);

        return write_res.result;
    }

    *dest = (struct dicey_packet) {
        .payload = hello,
        .nbytes = sizeof *hello,
    };

    return DICEY_OK;
}

bool dicey_type_is_valid(const enum dicey_type type) {
    switch (type) {
    case DICEY_TYPE_UNIT:
    case DICEY_TYPE_BOOL:
    case DICEY_TYPE_FLOAT:
    case DICEY_TYPE_INT:

    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_TUPLE:
    case DICEY_TYPE_PAIR:
    case DICEY_TYPE_BYTES:
    case DICEY_TYPE_STR:

    case DICEY_TYPE_PATH:
    case DICEY_TYPE_SELECTOR:

    case DICEY_TYPE_ERROR:
        return true;

    default:
        return false;
    }
}

const char* dicey_type_name(const enum dicey_type type) {
    switch (type) {
    default:
        assert(false);
        return NULL;
    
    case DICEY_TYPE_INVALID:
        return "invalid";
    
    case DICEY_TYPE_UNIT:
        return "unit";
    
    case DICEY_TYPE_BOOL:
        return "bool";
    
    case DICEY_TYPE_FLOAT:
        return "float";

    case DICEY_TYPE_INT:
        return "int";
    
    case DICEY_TYPE_ARRAY:
        return "array";

    case DICEY_TYPE_PAIR:
        return "pair";

    case DICEY_TYPE_TUPLE:
        return "tuple";

    case DICEY_TYPE_BYTES:
        return "bytes";

    case DICEY_TYPE_STR:
        return "str";

    case DICEY_TYPE_PATH:
        return "path";

    case DICEY_TYPE_SELECTOR:
        return "selector";

    case DICEY_TYPE_ERROR:
        return "error";
    }
}
