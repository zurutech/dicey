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

#if !defined(GQWOPCFNUH_UNSAFE_H)
#define GQWOPCFNUH_UNSAFE_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/core/views.h>

#if defined(__cplusplus)
extern "C" {
#endif

// We assume that the current platform doesn't have a pathological implementation of pointers, aka zeroing their memory
// actually sets them to null. If this is not the case, somewhere else there are macros to stop the build
#define ZERO_ARRAY(BASE, LEN) memset((BASE), 0, sizeof *(BASE) * (LEN))

void dunsafe_read_bytes(const struct dicey_view_mut dest, const void **src);
void dunsafe_write_bytes(void **dest, struct dicey_view view);

#if defined(__cplusplus)
}
#endif

#endif // GQWOPCFNUH_UNSAFE_H
