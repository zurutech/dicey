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

#if !defined(ZAKDPHUQGZ_PACKET_XML_H)
#define ZAKDPHUQGZ_PACKET_XML_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/dicey.h>

// Note: MSVC supports C11 now, but they clearly forgot to unflag flexible array members as an extension
// This pragma is here to teach MSVC to know its place
#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4200)
#endif

struct util_xml_error {
    int line;
    int col; // 0 when not available

    char message[];
};

struct util_xml_errors {
    uint32_t nerrs;
    uint32_t cap;
    const struct util_xml_error **errors;
};

void util_xml_errors_deinit(struct util_xml_errors *errs);

struct util_xml_errors util_xml_to_dicey(struct dicey_packet *dest, const void *bytes, size_t len);

#endif // ZAKDPHUQGZ_PACKET_XML_H
