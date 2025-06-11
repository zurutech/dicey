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

#if !defined(FNMCVSLICR_REGISTRY_INTERNAL_H)
#define FNMCVSLICR_REGISTRY_INTERNAL_H

#include <stdarg.h>

#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/views.h>
#include <dicey/ipc/registry.h>

#include "sup/util.h"

struct dicey_object {
    struct dicey_hashset *traits; /**< A set containing the names of traits that this object implements. */

    const char *main_path; /**< The path the object was created at. This is the "main" path, which may be different from
                              the aliased path if the object has been aliased. */

    struct dicey_hashset
        *aliases; /**< A set of aliases for this object. This is a set of paths that point to the same object. */

    void *cached_xml; /**< A cached XML representation of the object. Internal, do not use. Lazily generated */
};

struct dicey_registry {
    // note: While the paths are technically hierarchical, this has zero to no effect on the actual implementation.
    //       The paths are simply used as a way to identify objects and traits, and "directory-style" access is not
    //       of much use ATM. If this ever becomes useful, it's simple to implement - just swap the hashtable for a
    //       sorted tree or something similar.
    struct dicey_hashtable *paths;

    struct dicey_hashtable *traits;

    // scratchpad buffer used when crafting strings. Non thread-safe like all the rest of the registry.
    struct dicey_view_mut buffer;
};

// formats a string (ideally a path) using the given buffer. The buffer is reallocated and updated if necessary
char *dicey_metaname_format(const char *fmt, ...) DICEY_FORMAT(1, 2);
char *dicey_metaname_format_to(struct dicey_view_mut *buffer, const char *fmt, ...) DICEY_FORMAT(2, 3);
char *dicey_metaname_vformat_to(struct dicey_view_mut *buffer, const char *fmt, va_list args);

// formats a string (ideally a path) using an internal buffer. The buffer is reallocated if necessary
const char *dicey_registry_format_metaname(struct dicey_registry *registry, const char *fmt, ...) DICEY_FORMAT(2, 3);

struct dicey_object *dicey_registry_get_object_mut(const struct dicey_registry *registry, const char *path);

#endif // FNMCVSLICR_REGISTRY_INTERNAL_H
