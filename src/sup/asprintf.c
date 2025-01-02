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

#include "asprintf.h"

#if defined(DICEY_BUILD_ASPRINTF)

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **const dest, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    const int ret = vasprintf(dest, fmt, args);

    va_end(args);

    return ret;
}

int vasprintf(char **const dest, const char *const fmt, va_list args) {
    va_list args2;
    va_copy(args2, args);

    const int len = vsnprintf(NULL, 0, fmt, args2);

    va_end(args2);

    // ugly nonsense due to MSVC disliking assignments in conditionals
    if (len >= 0) {
        *dest = malloc(len + 1U);

        if (dest) {
            return vsnprintf(*dest, len + 1U, fmt, args);
        }
    }

    return -1;
}

#endif // DICEY_BUILD_ASPRINTF
