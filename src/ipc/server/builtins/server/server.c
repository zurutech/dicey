// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stdbool.h>

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

static enum dicey_error handle_sub_operation(
    struct dicey_builtin_context *const context,
    const uint8_t opcode,
    struct dicey_client_data *const client,
    const char *const path,
    const struct dicey_element_entry *const entry,
    const struct dicey_value *const value,
    struct dicey_packet *const response
) {
    (void) value;

    assert(context && client && path && entry && value && response && dicey_selector_is_valid(entry->sel));

    if (entry->element->type != DICEY_ELEMENT_TYPE_SIGNAL) {
        return TRACE(DICEY_EINVAL);
    }

    // do not allocate the same stuff a billion times. use the scratchpad.
    struct dicey_view_mut *const scratchpad = context->scratchpad;
    assert(scratchpad);

    enum dicey_error err = dicey_element_descriptor_format_to(scratchpad, path, entry->sel);
    if (err) {
        return err;
    }

    const char *const elemdescr = scratchpad->data;

    switch (opcode) {
    case SERVER_OP_EVENT_SUBSCRIBE:
        return dicey_client_data_subscribe(client, elemdescr);

    case SERVER_OP_EVENT_UNSUBSCRIBE:
        // do not trace the result of this operation, the ENOENT should be reported to the client without blocking
        // the server
        return dicey_client_data_unsubscribe(client, elemdescr) ? DICEY_OK : DICEY_ENOENT;

    default:
        assert(false);
        return TRACE(DICEY_EINVAL);
    }
}

const struct dicey_registry_builtin_set dicey_registry_server_builtins = {
    .objects = server_objects,
    .nobjects = DICEY_LENOF(server_objects),

    .traits = server_traits,
    .ntraits = DICEY_LENOF(server_traits),

    .handler = &handle_sub_operation,
};
