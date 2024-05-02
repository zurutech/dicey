// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ZYTIHTXNPP_INTROSPECTION_H)
#define ZYTIHTXNPP_INTROSPECTION_H

#include <dicey/core/errors.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

/**
 * object "/dicey/registry" : dicey.Registry
 */
#define DICEY_REGISTRY_PATH "/dicey/registry"

/**
 * trait dicey.Introspection {
 *     ro XML: string
 * }
 */

#define DICEY_INTROSPECTION_TRAIT_NAME "dicey.Introspection"

#define DICEY_INTROSPECTION_XML_PROP_NAME "XML"
#define DICEY_INTROSPECTION_XML_PROP_SIG "s"

/**
 * trait dicey.Registry {
 *     ro Objects: [@] // a list of object paths
 *     ro Traits: [@]  // a list of trait object paths
 *
 *     ElementExists: (@%) -> b // given a path and selector, return true if the element exists
 *     PathExists: @ -> b // takes a path, returns true if it exists. Nicer than attempting to get data from a
 * non-existing path and handling failure TraitExists: s -> b // takes a path, returns true if such a trait exists
 *
 *     // TODO: add signals for object creation and deletion
 * }
 */

#define DICEY_REGISTRY_TRAIT_NAME "dicey.Registry"

#define DICEY_REGISTRY_OBJECTS_PROP_NAME "Objects"
#define DICEY_REGISTRY_OBJECTS_PROP_SIG "[@]"

#define DICEY_REGISTRY_TRAITS_PROP_NAME "Traits"
#define DICEY_REGISTRY_TRAITS_PROP_SIG "[@]"

#define DICEY_REGISTRY_ELEMENT_EXISTS_OP_NAME "ElementExists"
#define DICEY_REGISTRY_ELEMENT_EXISTS_OP_SIG "(@%) -> b"

#define DICEY_REGISTRY_PATH_EXISTS_OP_NAME "PathExists"
#define DICEY_REGISTRY_PATH_EXISTS_OP_SIG "@ -> b"

#define DICEY_REGISTRY_TRAIT_EXISTS_OP_NAME "TraitExists"
#define DICEY_REGISTRY_TRAIT_EXISTS_OP_SIG "s -> b"

/**
 * trait dicey.Trait {
 *     ro Properties: [(sbs)] // array of (name, read-only, signature)
 *     ro Signals: [(ss)] // array of (name, signature)
 *     ro Operations: [(ss)] // array of (name, signature)
 * }
 */

#define DICEY_TRAIT_TRAIT_NAME "dicey.Trait"

#define DICEY_TRAIT_PROPERTIES_PROP_NAME "Properties"
#define DICEY_TRAIT_PROPERTIES_PROP_SIG "[(sbs)]"

#define DICEY_TRAIT_SIGNALS_PROP_NAME "Signals"
#define DICEY_TRAIT_SIGNALS_PROP_SIG "[(ss)]"

#define DICEY_TRAIT_OPERATIONS_PROP_NAME "Operations"
#define DICEY_TRAIT_OPERATIONS_PROP_SIG "[(ss)]"

enum dicey_error dicey_registry_populate_defaults(struct dicey_registry *registry);

#endif // ZYTIHTXNPP_INTROSPECTION_H
