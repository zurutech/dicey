// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(FNMCVSLICR_REGISTRY_INTERNAL_H)
#define FNMCVSLICR_REGISTRY_INTERNAL_H

#include <dicey/ipc/registry.h>

struct dicey_object *dicey_registry_get_object_mut(const struct dicey_registry *registry, const char *path);

#endif // FNMCVSLICR_REGISTRY_INTERNAL_H
