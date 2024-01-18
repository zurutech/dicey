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

#include "packet-args.h"

#define DEFAULT_VAL_CAP 16U

enum builder_state {
    BUILDER_STATE_IDLE = 0,
    BUILDER_STATE_PENDING,

    BUILDER_STATE_VALUE,

    BUILDER_STATE_TUPLE,
    BUILDER_STATE_ARRAY,
};

static enum dicey_error arglist_grow(struct _dicey_value_builder_list *const list) {
    const size_t new_cap = list->cap ? list->cap * 3U / 2U : DEFAULT_VAL_CAP;

    struct dicey_arg *const new_elems = realloc(list->elems, sizeof *new_elems * new_cap);
    if (!new_elems) {
        return DICEY_ENOMEM;
    }

    list->elems = new_elems;
    list->cap = new_cap;

    return DICEY_OK;
}

static enum builder_state builder_state_get(const void *const builder) {
    assert(builder);

    // all builders start with an integer state, or a struct which starts with an integer state
    return *(const int*) builder;
}

static void builder_state_set(const void *const builder, const enum builder_state state) {
    assert(builder);

    // all builders start with an integer state, or a struct which starts with an integer state
    *(int*) builder = state;
}

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

static bool selector_is_valid(const struct dicey_selector selector) {
    return selector.trait && selector.elem;
}

static bool msg_builder_is_complete(const
 struct dicey_message_builder *const builder) {
    assert(builder);

    return builder->state == BUILDER_STATE_PENDING
        && builder->path 
        && selector_is_valid(builder->selector)
        && builder->type != DICEY_MESSAGE_TYPE_INVALID
        // get messages must not have a root, everything else does
        && (builder->type == DICEY_MESSAGE_TYPE_GET) ^ (bool) { builder->root }; 
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

static ptrdiff_t msgkind_to_dtf(const enum dicey_message_type kind) {
    switch (kind) {
    default:
        assert(false);

    case DICEY_MESSAGE_TYPE_INVALID:
        return DICEY_EINVAL;

    case DICEY_MESSAGE_TYPE_SET:
        return DTF_PAYLOAD_SET;

    case DICEY_MESSAGE_TYPE_GET:
        return DTF_PAYLOAD_GET;
        
    case DICEY_MESSAGE_TYPE_EXEC:
        return DTF_PAYLOAD_EXEC;

    case DICEY_MESSAGE_TYPE_EVENT:
        return DTF_PAYLOAD_EVENT;

    case DICEY_MESSAGE_TYPE_RESPONSE:
        return DTF_PAYLOAD_RESPONSE;
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

static bool valbuilder_is_valid(const struct dicey_value_builder *const builder) {
    return builder && builder->_root;
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

enum dicey_error dicey_message_builder_init(struct dicey_message_builder *const builder) {
    assert(builder);

    *builder = (struct dicey_message_builder) {0};

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_begin(
    struct dicey_message_builder *const builder,
    const enum dicey_message_type type
) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_IDLE) {
        return DICEY_EINVAL;
    }

    *builder = (struct dicey_message_builder) {
        .state = BUILDER_STATE_PENDING,
        .type = type,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_build(
    struct dicey_message_builder *const builder,
    struct dicey_packet *const packet
) {
    assert(builder && packet);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    if (!msg_builder_is_complete(builder)) {
        return DICEY_EAGAIN;
    }

    const ptrdiff_t payload_kind = msgkind_to_dtf(builder->type);
    if (payload_kind < 0) {
        return payload_kind;
    }
    
    const struct dtf_writeres craft_res = dtf_message_write(
        DICEY_NULL,
        (enum dtf_payload_kind) payload_kind,
        builder->seq,
        builder->path,
        builder->selector,
        builder->root
    );

    if (craft_res.result < 0) {
        return craft_res.result;
    }

    dicey_message_builder_discard(builder);

    *packet = (struct dicey_packet) {
        .payload = craft_res.start,
        .nbytes = craft_res.size,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_destroy(struct dicey_message_builder *const builder) {
    assert(builder);

    dicey_arg_free(builder->root);

    *builder = (struct dicey_message_builder) {0};

    return DICEY_OK;
}

void dicey_message_builder_discard(struct dicey_message_builder *const builder) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_IDLE) {
        dicey_arg_free(builder->root);

        *builder = (struct dicey_message_builder) {0};
    }
}

enum dicey_error dicey_message_builder_set_path(
    struct dicey_message_builder *const builder,
    const char *const path
) {
    assert(builder && path);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder->path = path;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_selector(
    struct dicey_message_builder *const builder,
    const struct dicey_selector selector
) {
    assert(builder && selector_is_valid(selector));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder->selector = selector;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *const builder, const uint32_t seq) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder->seq = seq;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_value(
    struct dicey_message_builder *const builder,
    const struct dicey_arg value
) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    struct dicey_value_builder value_builder = {0};

    enum dicey_error err = dicey_message_builder_value_start(builder, &value_builder);
    if (err != DICEY_OK) {
        return err;
    }

    err = dicey_value_builder_set(&value_builder, value);

    // always end the value builder
    const enum dicey_error end_err = dicey_message_builder_value_end(builder, &value_builder);

    return err ? err : end_err;
}

enum dicey_error dicey_message_builder_value_start(
    struct dicey_message_builder *const builder,
    struct dicey_value_builder *const value
) {
    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder_state_set(builder, BUILDER_STATE_VALUE);

    struct dicey_arg *const root = calloc(sizeof *value, 1U);
    if (!root) {
        return DICEY_ENOMEM;
    }

    dicey_arg_free(builder->root);

    builder->root = root;

    *value = (struct dicey_value_builder) {
        ._state = BUILDER_STATE_PENDING,
        ._root = root,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_value_end(
    struct dicey_message_builder *const builder,
    struct dicey_value_builder *const value
) {
    assert(builder && value);

    if (builder_state_get(builder) != BUILDER_STATE_VALUE) {
        return DICEY_EINVAL;
    }

    if (value->_state != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    *value = (struct dicey_value_builder) {0};

    return DICEY_OK;    
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

enum dicey_error dicey_value_builder_array_start(
    struct dicey_value_builder *const builder,
    const enum dicey_type type
) {
    assert(valbuilder_is_valid(builder) && dicey_type_is_valid(type));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder->_list = (struct _dicey_value_builder_list) {
        .type = DICEY_TYPE_ARRAY,
    };

    builder_state_set(builder, BUILDER_STATE_ARRAY);

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_ARRAY) {
        return DICEY_EINVAL;
    }

    const struct _dicey_value_builder_list *const list = &builder->_list;

    assert(!list->nitems || list->elems);

    *builder->_root = (struct dicey_arg) {
        .type = DICEY_TYPE_ARRAY,
        .array = {
            .type = list->type,
            .nitems = list->nitems,
            .elems = list->elems,
        },
    };

    *builder = (struct dicey_value_builder) { 0 };

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_next(
    struct dicey_value_builder *const builder,
    struct dicey_value_builder *const elem
) {
    assert(valbuilder_is_valid(builder) && elem);

    const int list_state = builder_state_get(builder);
    
    switch (list_state) {
    case BUILDER_STATE_ARRAY:
    case BUILDER_STATE_TUPLE:
        break;
        
    default:
        return DICEY_EINVAL;
    }

    struct _dicey_value_builder_list *const list = &builder->_list;

    if (list->nitems >= list->cap) {
        arglist_grow(list);
    }

    assert(list->nitems <= list->cap);

    struct dicey_arg *const elem_item = &list->elems[list->nitems++];

    if (list_state == BUILDER_STATE_ARRAY) {
        elem_item->type = list->type;
    }

    *elem = (struct dicey_value_builder) {
        ._state = BUILDER_STATE_PENDING,
        ._root = elem_item,
    };

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_set(
    struct dicey_value_builder *const builder,
    const struct dicey_arg value
) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    if (!dicey_type_is_valid(value.type)) {
        return DICEY_EINVAL;
    }

    const struct dicey_arg *const root = builder->_root;

    if (dicey_type_is_valid(root->type) && root->type != value.type) {
        return DICEY_EINVAL;
    }

    if (!dicey_arg_dup(builder->_root, &value)) {
        return DICEY_ENOMEM;
    }

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return DICEY_EINVAL;
    }

    builder->_list = (struct _dicey_value_builder_list) {
        .type = DICEY_TYPE_TUPLE,
    };

    builder_state_set(builder, BUILDER_STATE_TUPLE);

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_TUPLE) {
        return DICEY_EINVAL;
    }

    const struct _dicey_value_builder_list *const list = &builder->_list;

    assert(!list->nitems || list->elems);

    *builder->_root = (struct dicey_arg) {
        .type = DICEY_TYPE_TUPLE,
        .tuple = {
            .nitems = list->nitems,
            .elems = list->elems,
        },
    };

    *builder = (struct dicey_value_builder) { 0 };

    return DICEY_OK;
}
