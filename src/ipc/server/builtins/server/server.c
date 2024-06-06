// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>
#include <dicey/ipc/traits.h>

#include "ipc/elemdescr.h"
#include "ipc/server/builtins/builtins.h"
#include "ipc/server/client-data.h"

#include "sup/trace.h"

#include "server.h"

enum server_op {
    SERVER_OP_EVENT_SUBSCRIBE = 0,
    SERVER_OP_EVENT_UNSUBSCRIBE,
};

static const struct dicey_default_element em_elements[] = {
    {
     .name = DICEY_EVENTMANAGER_SUBSCRIBE_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_EVENTMANAGER_SUBSCRIBE_OP_SIG,
     .opcode = SERVER_OP_EVENT_SUBSCRIBE,
     },
    {
     .name = DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_NAME,
     .type = DICEY_ELEMENT_TYPE_OPERATION,
     .signature = DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_SIG,
     .opcode = SERVER_OP_EVENT_UNSUBSCRIBE,
     },
};

static const struct dicey_default_object server_objects[] = {
    {
     .path = DICEY_SERVER_PATH,
     .traits = (const char *[]) { DICEY_EVENTMANAGER_TRAIT_NAME, NULL },
     },
};

static const struct dicey_default_trait server_traits[] = {
    {.name = DICEY_EVENTMANAGER_TRAIT_NAME, .elements = em_elements, .num_elements = DICEY_LENOF(em_elements)},
};

static enum dicey_error unit_message_for(
    struct dicey_packet *const dest,
    const char *const path,
    const struct dicey_selector sel
) {
    assert(dest && path && dicey_selector_is_valid(sel));

    struct dicey_message_builder builder = { 0 };

    enum dicey_error err = dicey_message_builder_init(&builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_RESPONSE);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&builder, path);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(&builder, sel);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_value(&builder, (struct dicey_arg) { .type = DICEY_TYPE_UNIT });
    if (err) {
        goto fail;
    }

    return dicey_message_builder_build(&builder, dest);

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

static enum dicey_error extract_path_sel(
    const struct dicey_value *const value,
    const char **const path,
    struct dicey_selector *const sel
) {
    assert(value && path && sel);

    struct dicey_list lst = { 0 };

    enum dicey_error err = dicey_value_get_tuple(value, &lst);
    if (err) {
        return err;
    }

    struct dicey_iterator it = dicey_list_iter(&lst);
    struct dicey_value elem = { 0 };

    err = dicey_iterator_next(&it, &elem);
    if (err) {
        return err;
    }

    err = dicey_value_get_path(&elem, path);
    if (err) {
        return err;
    }

    err = dicey_iterator_next(&it, &elem);
    if (err) {
        return err;
    }

    err = dicey_value_get_selector(&elem, sel);
    if (err) {
        return err;
    }

    if (dicey_iterator_has_next(it)) {
        return TRACE(DICEY_ESIGNATURE_MISMATCH);
    }

    return DICEY_OK;
}

static enum dicey_error handle_sub_operation(
    struct dicey_builtin_context *const context,
    const uint8_t opcode,
    struct dicey_client_data *const client,
    const char *const src_path,
    const struct dicey_element_entry *const src_entry,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    (void) src_path;
    (void) src_entry;

    assert(context && client && value && response);
    const char *path = NULL;
    struct dicey_selector sel = { 0 };

    enum dicey_error err = extract_path_sel(value, &path, &sel);
    if (err) {
        return err;
    }

    const struct dicey_element *elem = dicey_registry_get_element(context->registry, path, sel.trait, sel.elem);

    if (!elem || elem->type != DICEY_ELEMENT_TYPE_SIGNAL) {
        return TRACE(DICEY_EINVAL);
    }

    // do not allocate the same stuff a billion times. use the scratchpad.
    struct dicey_view_mut *const scratchpad = context->scratchpad;
    assert(scratchpad);

    const char *const elemdescr = dicey_element_descriptor_format_to(scratchpad, path, sel);
    if (!elemdescr) {
        return TRACE(DICEY_ENOMEM);
    }

    switch (opcode) {
    case SERVER_OP_EVENT_SUBSCRIBE:
        err = dicey_client_data_subscribe(client, elemdescr);
        break;

    case SERVER_OP_EVENT_UNSUBSCRIBE:
        // do not trace the result of this operation, the ENOENT should be reported to the client without blocking
        // the server
        err = dicey_client_data_unsubscribe(client, elemdescr) ? DICEY_OK : DICEY_ENOENT;

        break;

    default:
        assert(false);
        return TRACE(DICEY_EINVAL);
    }

    if (err) {
        return err;
    }

    return unit_message_for(response, path, sel);
}

const struct dicey_registry_builtin_set dicey_registry_server_builtins = {
    .objects = server_objects,
    .nobjects = DICEY_LENOF(server_objects),

    .traits = server_traits,
    .ntraits = DICEY_LENOF(server_traits),

    .handler = &handle_sub_operation,
};
