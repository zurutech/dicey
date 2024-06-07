// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(NICMLMFZJN_ELEMDESCR_H)
#define NICMLMFZJN_ELEMDESCR_H

#include <dicey/core/errors.h>
#include <dicey/core/type.h>
#include <dicey/core/views.h>

// Simple internal functions that format a (path, sel) pair into a path#trait:elem string
// The _to version attempts to reuse a buffer instead of allocating a brand new one

char *dicey_element_descriptor_format(const char *path, struct dicey_selector sel);
char *dicey_element_descriptor_format_to(struct dicey_view_mut *dest, const char *path, struct dicey_selector sel);

#endif // NICMLMFZJN_ELEMDESCR_H
