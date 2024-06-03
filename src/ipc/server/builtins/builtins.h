// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GQMLLUMVQC_BUILTINS_H)
#define GQMLLUMVQC_BUILTINS_H

#include <stdbool.h>
#include <stddef.h>

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#define DICEY_LENOF(ARR) (sizeof(ARR) / sizeof(ARR)[0])

typedef enum dicey_error dicey_registry_builtin_op(
    struct dicey_registry *registry,
    uint8_t opcode,
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
