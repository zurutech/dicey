// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stdbool.h>

#include <dicey/ipc/traits.h>

#include "../builtins.h"

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

const struct dicey_registry_builtin_set dicey_registry_server_builtins = {
    .objects = server_objects,
    .nobjects = DICEY_LENOF(server_objects),

    .traits = server_traits,
    .ntraits = DICEY_LENOF(server_traits),
};
