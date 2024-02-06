// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/builders.h>
#include <dicey/errors.h>
#include <dicey/packet.h>
#include <dicey/type.h>
#include <dicey/value.h>
#include <dicey/views.h>

#include "dtf/dtf.h"

#include "packet-args.h"
#include "trace.h"
#include "view-ops.h"

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
        return TRACE(DICEY_ENOMEM);
    }

    list->elems = new_elems;
    list->cap = new_cap;

    return DICEY_OK;
}

static enum builder_state builder_state_get(const void *const builder) {
    assert(builder);

    // all builders start with an integer state, or a struct which starts with an integer state
    return *(const int *) builder;
}

static void builder_state_set(const void *const builder, const enum builder_state state) {
    assert(builder);

    // all builders start with an integer state, or a struct which starts with an integer state
    *(int *) builder = state;
}

static bool msgbuilder_is_complete(const struct dicey_message_builder *const builder) {
    assert(builder);

    return builder->_state == BUILDER_STATE_PENDING
           // the path must always be set in order for a builder to be valid
           && builder->_path
           // the selector must be valid
           && dicey_selector_is_valid(builder->_selector)
           // validate that the operation is not junk
           && dicey_op_is_valid(builder->_type)
           // get messages must not have a root, everything else does
           && (builder->_type == DICEY_OP_GET) != (bool) { builder->_root };
}

static ptrdiff_t msgkind_to_dtf(const enum dicey_op kind) {
    switch (kind) {
    default:
        assert(false);

    case DICEY_OP_INVALID:
        return TRACE(DICEY_EINVAL);

    case DICEY_OP_SET:
        return DTF_PAYLOAD_SET;

    case DICEY_OP_GET:
        return DTF_PAYLOAD_GET;

    case DICEY_OP_EXEC:
        return DTF_PAYLOAD_EXEC;

    case DICEY_OP_EVENT:
        return DTF_PAYLOAD_EVENT;

    case DICEY_OP_RESPONSE:
        return DTF_PAYLOAD_RESPONSE;
    }
}

#if !defined(NDEBUG)

static bool valbuilder_is_valid(const struct dicey_value_builder *const builder) {
    return builder && builder->_root;
}

#endif

enum dicey_error dicey_message_builder_init(struct dicey_message_builder *const builder) {
    assert(builder);

    *builder = (struct dicey_message_builder) { 0 };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_begin(struct dicey_message_builder *const builder, const enum dicey_op op) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_IDLE) {
        return TRACE(DICEY_EINVAL);
    }

    *builder = (struct dicey_message_builder) {
        ._state = BUILDER_STATE_PENDING,
        ._type = op,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_build(
    struct dicey_message_builder *const builder,
    struct dicey_packet *const          packet
) {
    assert(builder && packet);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    if (!msgbuilder_is_complete(builder)) {
        return TRACE(DICEY_EAGAIN);
    }

    const ptrdiff_t payload_kind = msgkind_to_dtf(builder->_type);
    if (payload_kind < 0) {
        return payload_kind;
    }

    const struct dtf_result craft_res = dtf_message_write(
        DICEY_NULL,
        (enum dtf_payload_kind) payload_kind,
        builder->_seq,
        builder->_path,
        builder->_selector,
        builder->_root
    );

    if (craft_res.result < 0) {
        return craft_res.result;
    }

    dicey_message_builder_discard(builder);

    *packet = (struct dicey_packet) {
        .payload = craft_res.data,
        .nbytes = craft_res.size,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_destroy(struct dicey_message_builder *const builder) {
    assert(builder);

    dicey_arg_free(builder->_root);

    *builder = (struct dicey_message_builder) { 0 };

    return DICEY_OK;
}

void dicey_message_builder_discard(struct dicey_message_builder *const builder) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_IDLE) {
        dicey_arg_free(builder->_root);

        *builder = (struct dicey_message_builder) { 0 };
    }
}

enum dicey_error dicey_message_builder_set_path(struct dicey_message_builder *const builder, const char *const path) {
    assert(builder && path);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder->_path = path;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_selector(
    struct dicey_message_builder *const builder,
    const struct dicey_selector         selector
) {
    assert(builder && dicey_selector_is_valid(selector));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder->_selector = selector;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *const builder, const uint32_t seq) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder->_seq = seq;

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_set_value(
    struct dicey_message_builder *const builder,
    const struct dicey_arg              value
) {
    assert(builder);

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_value_builder value_builder = { 0 };

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
    struct dicey_value_builder *const   value
) {
    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder_state_set(builder, BUILDER_STATE_VALUE);

    struct dicey_arg *const root = calloc(sizeof(struct dicey_arg), 1U);
    if (!root) {
        return TRACE(DICEY_ENOMEM);
    }

    dicey_arg_free(builder->_root);

    builder->_root = root;
    builder->_borrowed_to = value;

    *value = (struct dicey_value_builder) {
        ._state = BUILDER_STATE_PENDING,
        ._root = root,
    };

    return DICEY_OK;
}

enum dicey_error dicey_message_builder_value_end(
    struct dicey_message_builder *const builder,
    struct dicey_value_builder *const   value
) {
    assert(builder && value);

    if (builder_state_get(builder) != BUILDER_STATE_VALUE) {
        return TRACE(DICEY_EINVAL);
    }

    if (value != builder->_borrowed_to) {
        return TRACE(DICEY_EINVAL);
    }

    *value = (struct dicey_value_builder) { 0 };
    builder->_state = BUILDER_STATE_PENDING;

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_array_start(
    struct dicey_value_builder *const builder,
    const enum dicey_type             type
) {
    assert(valbuilder_is_valid(builder) && dicey_type_is_valid(type));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder->_list = (struct _dicey_value_builder_list) {
        .type = type,
    };

    builder_state_set(builder, BUILDER_STATE_ARRAY);

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_ARRAY) {
        return TRACE(DICEY_EINVAL);
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
        return TRACE(DICEY_EINVAL);
    }

    struct _dicey_value_builder_list *const list = &builder->_list;

    if (list->nitems >= list->cap) {
        arglist_grow(list);
    }

    assert(list->nitems <= list->cap);

    struct dicey_arg *const elem_item = &list->elems[list->nitems++];

    *elem_item = (struct dicey_arg) { 0 };

    if (list_state == BUILDER_STATE_ARRAY) {
        elem_item->type = list->type;
    }

    *elem = (struct dicey_value_builder) {
        ._state = BUILDER_STATE_PENDING,
        ._root = elem_item,
    };

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_set(struct dicey_value_builder *const builder, const struct dicey_arg value) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    if (!dicey_type_is_valid(value.type)) {
        return TRACE(DICEY_EINVAL);
    }

    const struct dicey_arg *const root = builder->_root;

    if (dicey_type_is_valid(root->type) && root->type != value.type) {
        return TRACE(DICEY_EVALUE_TYPE_MISMATCH);
    }

    // free any previously set value
    dicey_arg_free_contents(root);

    if (!dicey_arg_dup(builder->_root, &value)) {
        return TRACE(DICEY_ENOMEM);
    }

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_PENDING) {
        return TRACE(DICEY_EINVAL);
    }

    builder->_list = (struct _dicey_value_builder_list) { 0 };

    builder_state_set(builder, BUILDER_STATE_TUPLE);

    return DICEY_OK;
}

enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *const builder) {
    assert(valbuilder_is_valid(builder));

    if (builder_state_get(builder) != BUILDER_STATE_TUPLE) {
        return TRACE(DICEY_EINVAL);
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
