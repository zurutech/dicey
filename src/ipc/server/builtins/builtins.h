// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GQMLLUMVQC_BUILTINS_H)
#define GQMLLUMVQC_BUILTINS_H

#include <stdbool.h>
#include <stddef.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/registry.h>

#define DICEY_LENOF(ARR) (sizeof(ARR) / sizeof(ARR)[0])

struct dicey_default_element {
    const char *name;
    enum dicey_element_type type;
    const char *signature;
    bool readonly;
    int tag;
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
};

enum dicey_error dicey_registry_populate_builtins(struct dicey_registry *registry);

#endif // GQMLLUMVQC_BUILTINS_H
