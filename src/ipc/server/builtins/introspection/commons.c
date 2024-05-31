// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>

#include "introspection-internal.h"

enum dicey_error introspection_init_builder(
    struct dicey_message_builder *const builder,
    const char *const path,
    const char *const trait_name,
    const char *const elem_name
) {
    assert(builder && path && trait_name && elem_name);

    enum dicey_error err = dicey_message_builder_init(builder);
    if (err) {
        return err;
    }

    err = dicey_message_builder_begin(builder, DICEY_OP_RESPONSE);
    if (err) {
        return err;
    }

    err = dicey_message_builder_set_path(builder, path);
    if (err) {
        return err;
    }

    return dicey_message_builder_set_selector(
        builder,
        (struct dicey_selector) {
            .trait = trait_name,
            .elem = elem_name,
        }
    );
}
