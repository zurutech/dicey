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

#if !defined(MIFVOFWBUL_GETOPT_WIN_H)
#define MIFVOFWBUL_GETOPT_WIN_H

#include <dicey/dicey.h>

#if defined(DICEY_IS_UNIX)

#include <unistd.h>

#else

int getopt(const int nargc, char *const nargv[], const char *const ostr);

extern char *optarg;
extern int optind, opterr, optopt;
extern int optreset;

#endif

#endif // MIFVOFWBUL_GETOPT_WIN_H
