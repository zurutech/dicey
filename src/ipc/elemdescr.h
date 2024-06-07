/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
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
