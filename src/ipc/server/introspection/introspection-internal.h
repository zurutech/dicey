// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(JTHRFVRFQF_INTERNAL_H)
#define JTHRFVRFQF_INTERNAL_H

#include <libxml/xmlstring.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

enum dicey_error introspection_check_element_exists(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_selector sel,
    struct dicey_packet *dest
);

enum dicey_error introspection_check_path_exists(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_packet *dest
);

enum dicey_error introspection_check_trait_exists(
    const struct dicey_registry *registry,
    const char *trait,
    struct dicey_packet *dest
);

enum dicey_error introspection_craft_filtered_elemlist(
    const struct dicey_registry *registry,
    const char *path,
    const char *trait_name,
    enum dicey_element_type op_kind,
    struct dicey_packet *dest
);

enum dicey_error introspection_craft_pathlist(const struct dicey_registry *registry, struct dicey_packet *dest);
enum dicey_error introspection_craft_traitlist(const struct dicey_registry *registry, struct dicey_packet *dest);

enum dicey_error introspection_dump_object(
    struct dicey_registry *registry,
    const char *path,
    struct dicey_packet *dest
);

enum dicey_error introspection_dump_xml(struct dicey_registry *registry, const char *path, struct dicey_packet *dest);

enum dicey_error introspection_init_builder(
    struct dicey_message_builder *builder,
    const char *path,
    const char *trait_name,
    const char *elem_name
);

enum dicey_error introspection_object_populate_xml(
    const struct dicey_registry *registry,
    const char *path,
    struct dicey_object *obj,
    const xmlChar **dest
);

#endif // JTHRFVRFQF_INTERNAL_H
