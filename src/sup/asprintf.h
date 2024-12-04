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

#if !defined(HFKNEIUTND_ASPRINTF_H)
#define HFKNEIUTND_ASPRINTF_H

#include "util.h"

#include "dicey_config.h"

#if defined(DICEY_IS_UNIX)
#define _GNU_SOURCE
#include <stdio.h>
#else

#include <stdarg.h>

#define DICEY_BUILD_ASPRINTF

int asprintf(char **const s, const char *const fmt, ...) DICEY_FORMAT(2, 3);
int vasprintf(char **const s, const char *const fmt, va_list ap);
#endif

#endif // HFKNEIUTND_ASPRINTF_H
