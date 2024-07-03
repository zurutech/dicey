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

#if !defined(VTDIZRGWKE_DUMPER_H)
#define VTDIZRGWKE_DUMPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define DEFAULT_PAD 4U

struct util_dumper {
    FILE *dest;
    unsigned pad;
    char padchar;
    bool newline;
};

static inline struct util_dumper util_dumper_for(FILE *const dest) {
    return (struct util_dumper) {
        .dest = dest,
        .pad = 0U,
        .padchar = ' ',
        .newline = true,
    };
}

void util_dumper_dump_hex(struct util_dumper *dump, const void *data, size_t size);
void util_dumper_indent(const struct util_dumper *dumper);
void util_dumper_newline(struct util_dumper *dumper);
void util_dumper_pad(struct util_dumper *dumper);
void util_dumper_printf(struct util_dumper *dumper, const char *fmt, ...);
void util_dumper_printlnf(struct util_dumper *dumper, const char *fmt, ...);
void util_dumper_reset_pad(struct util_dumper *dumper);
void util_dumper_unpad(struct util_dumper *dumper);

#endif // VTDIZRGWKE_DUMPER_H
