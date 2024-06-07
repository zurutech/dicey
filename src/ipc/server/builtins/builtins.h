// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GQMLLUMVQC_BUILTINS_H)
#define GQMLLUMVQC_BUILTINS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "ipc/server/client-data.h"

#define DICEY_LENOF(ARR) (sizeof(ARR) / sizeof(ARR)[0])

struct dicey_builtin_context {
    struct dicey_registry *registry;

    // A scratchpad for the handler to use. This is a mutable view to an area of memory that can be used to store
    // temporary data. The scratchpad is owned by the server, can be reallocated and is not thread-safe.
    struct dicey_view_mut *scratchpad;
};

/**
 * @brief A function pointer type that describes the handler for a builtin operation.
 *
 */
typedef enum dicey_error dicey_registry_builtin_op(
    struct dicey_builtin_context *context,
    uint8_t opcode,
    struct dicey_client_data *client,
    const char *path,
    const struct dicey_element_entry *entry,
    const struct dicey_value *value,
    struct dicey_packet *response
);

struct dicey_default_element {
    const char *name;
    enum dicey_element_type type;
    const char *signature;
    bool readonly;
    uint8_t opcode;
};

struct dicey_default_object {
    const char *path;
    const char **traits;
};

struct dicey_default_trait {
    const char *name;
    const struct dicey_default_element *elements;
    size_t num_elements;
};

struct dicey_registry_builtin_set {
    const struct dicey_default_object *objects;
    size_t nobjects;

    const struct dicey_default_trait *traits;
    size_t ntraits;

    dicey_registry_builtin_op *handler;
};

// This is a struct that holds the opcode and the handler for a builtin operation.
// This function is used to get the handler for a builtin operation from a given element
struct dicey_registry_builtin_info {
    dicey_registry_builtin_op *handler;
    uint8_t opcode;
};

bool dicey_registry_get_builtin_info_for(
    const struct dicey_element_entry *elem,
    struct dicey_registry_builtin_info *target
);

enum dicey_error dicey_registry_populate_builtins(struct dicey_registry *registry);

#endif // GQMLLUMVQC_BUILTINS_H
