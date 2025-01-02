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

#if !defined(DTF_DHCBDDHD_H)
#define DTF_DHCBDDHD_H

#if defined(__cplusplus)
#error "This header is not meant to be included from C++"
#endif

#include <stddef.h>
#include <stdint.h>

#include <dicey/core/builders.h>
#include <dicey/core/packet.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>

#include "to.h"

enum dtf_payload_kind {
    DTF_PAYLOAD_INVALID = DICEY_PACKET_KIND_INVALID,

    DTF_PAYLOAD_HELLO = DICEY_PACKET_KIND_HELLO,
    DTF_PAYLOAD_BYE = DICEY_PACKET_KIND_BYE,

    DTF_PAYLOAD_GET = DICEY_OP_GET,
    DTF_PAYLOAD_SET = DICEY_OP_SET,
    DTF_PAYLOAD_EXEC = DICEY_OP_EXEC,
    DTF_PAYLOAD_EVENT = DICEY_OP_SIGNAL,
    DTF_PAYLOAD_RESPONSE = DICEY_OP_RESPONSE,
};

static inline bool dtf_payload_kind_is_message(const enum dtf_payload_kind kind) {
    switch (kind) {
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return true;

    default:
        return false;
    }
}

struct dtf_result {
    ptrdiff_t result;
    size_t size;
    void *data;
};

struct dtf_result dtf_bye_write(struct dicey_view_mut dest, uint32_t seq, uint32_t reason);
struct dtf_result dtf_hello_write(struct dicey_view_mut dest, uint32_t seq, uint32_t version);

ptrdiff_t dtf_message_estimate_header_size(
    enum dtf_payload_kind kind,
    const char *path,
    struct dicey_selector selector
);

ptrdiff_t dtf_message_estimate_size(
    enum dtf_payload_kind kind,
    const char *path,
    struct dicey_selector selector,
    const struct dicey_arg *value
);

struct dtf_message_content {
    const char *path;
    struct dicey_selector selector;

    const struct dtf_value *value;
    size_t value_len;
};

ptrdiff_t dtf_message_get_content(const struct dtf_message *msg, size_t alloc_len, struct dtf_message_content *dest);

struct dtf_result dtf_message_write(
    struct dicey_view_mut dest,
    enum dtf_payload_kind kind,
    uint32_t seq,
    const char *path,
    struct dicey_selector selector,
    const struct dicey_arg *value
);

struct dtf_result dtf_message_write_with_raw_value(
    struct dicey_view_mut dest,
    enum dtf_payload_kind kind,
    uint32_t seq,
    const char *path,
    struct dicey_selector selector,
    struct dicey_view view
);

int dtf_selector_load_from(struct dicey_selector *selector, struct dicey_view src);

union dtf_payload {
    struct dtf_payload_head *header;

    struct dtf_message *msg;
    struct dtf_hello *hello;
    struct dtf_bye *bye;
};

enum dtf_payload_kind dtf_payload_get_kind(union dtf_payload msg);
ptrdiff_t dtf_payload_get_seq(union dtf_payload msg);

struct dtf_result dtf_payload_load(union dtf_payload *dest, struct dicey_view *src);

enum dicey_error dtf_payload_set_seq(union dtf_payload msg, uint32_t seq);

#endif // DTF_DHCBDDHD_H
