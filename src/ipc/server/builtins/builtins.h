/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

struct dicey_builtin_context {
    struct dicey_registry *registry;

    // A scratchpad for the handler to use. This is a mutable view to an area of memory that can be used to store
    // temporary data. The scratchpad is owned by the server, can be reallocated and is not thread-safe.
    struct dicey_view_mut *scratchpad;
};

bool dicey_builtin_context_is_valid(const struct dicey_builtin_context *ctx);

/**
 * @brief The request for a builtin operation.
 * @note  The handler can "steal" the source packet by zeroing it out, to keep the runtime from freeing it. This is
 *        useful when the handler wants to keep the packet around for later use.
 */
struct dicey_builtin_request {
    uint8_t opcode;
    struct dicey_client_data *client;
    const char *path;
    const struct dicey_element_entry *entry;
    struct dicey_packet *source;
    const struct dicey_value *value;
};

bool dicey_builtin_request_is_valid(const struct dicey_builtin_request *request);

/**
 * @brief A function pointer type that describes the handler for a builtin operation.
 * @note  The handler can "steal" the request packet by zeroing it out, to keep the runtime from freeing it. This is
 *        useful when the handler wants to keep the packet around for later use.
 * @return If positive, the state of the client after executing the builtin operation (usually RUNNING)
 *         If negative, an error code of enum dicey_error
 */
typedef ptrdiff_t dicey_registry_builtin_op(
    struct dicey_builtin_context *ctx,
    struct dicey_builtin_request *request,
    struct dicey_packet *response
);

struct dicey_default_element {
    const char *name;
    enum dicey_element_type type;
    const char *signature;
    int flags;
    uint8_t opcode;
};

struct dicey_default_object {
    const char *path;
    const char *const *traits;
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

bool dicey_registry_get_builtin_info_for(struct dicey_element_entry elem, struct dicey_registry_builtin_info *target);

enum dicey_error dicey_registry_populate_builtins(struct dicey_registry *registry);

#endif // GQMLLUMVQC_BUILTINS_H
