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

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

#include <dicey/dicey.h>

#include <util/dumper.h>
#include <util/packet-dump.h>
#include <util/uuid.h>

static void dump_value(struct util_dumper *dumper, const struct dicey_value *value);

static void dump_list(struct util_dumper *const dumper, const struct dicey_list *const list) {
    assert(dumper && list);

    struct dicey_iterator iter = dicey_list_iter(list);
    for (size_t i = 0U; dicey_iterator_has_next(iter); ++i) {
        struct dicey_value item = { 0 };
        const enum dicey_error err = dicey_iterator_next(&iter, &item);
        assert(!err);
        (void) err; // silence unused warning

        util_dumper_printf(dumper, "[%zu] = ", i);
        dump_value(dumper, &item);
        util_dumper_newline(dumper);
    }
}

static void dump_array(struct util_dumper *const dumper, const struct dicey_value *const value) {
    assert(dumper && value);

    struct dicey_list list = { 0 };
    const enum dicey_error err = dicey_value_get_array(value, &list);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(dumper, "[%s]{", dicey_type_name(dicey_list_type(&list)));

    util_dumper_pad(dumper);

    dump_list(dumper, &list);

    util_dumper_unpad(dumper);

    util_dumper_printf(dumper, "}");
}

static void dump_bye(struct util_dumper *const dumper, const struct dicey_packet packet) {
    assert(dumper);

    struct dicey_bye bye = { 0 };
    const enum dicey_error err = dicey_packet_as_bye(packet, &bye);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(dumper, "bye { reason = %s }", dicey_bye_reason_to_string(bye.reason));
}

static void dump_hello(struct util_dumper *const dumper, const struct dicey_packet packet) {
    assert(dumper);

    struct dicey_hello hello = { 0 };
    const enum dicey_error err = dicey_packet_as_hello(packet, &hello);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(
        dumper, "hello { version = %" PRIu16 "r%" PRIu16 " }", hello.version.major, hello.version.revision
    );
}

static void dump_hex_short(struct util_dumper *const dumper, const void *const data, const size_t n) {
    assert(dumper);

    if (!data || !n) {
        util_dumper_printf(dumper, "null");
    } else {
        util_dumper_printf(dumper, "[ ");

        // if the view is 4 bytes or less, print the hex as separate bytes
        // otherwise, print b0 b1 .. bN-1 bN
        const uint8_t *bytes = data, *const end = bytes + n;

        if (n <= 16U) {
            while (bytes < end) {
                util_dumper_printf(dumper, "%02" PRIx8 " ", *bytes++);
            }
        } else {
            util_dumper_printf(
                dumper,
                "%02" PRIx8 " %02" PRIx8 " .. %02" PRIx8 " %02 " PRIx8,
                bytes[0],
                bytes[1],
                bytes[n - 2],
                bytes[n - 1]
            );
        }

        util_dumper_printf(dumper, "] (%zu bytes)", n);
    }
}

static void dump_pair(struct util_dumper *const dumper, const struct dicey_value *const value) {
    assert(dumper && value);

    struct dicey_pair pair = { 0 };
    const enum dicey_error err = dicey_value_get_pair(value, &pair);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(dumper, "{");
    util_dumper_pad(dumper);

    util_dumper_printf(dumper, "first = ");
    dump_value(dumper, &pair.first);
    util_dumper_newline(dumper);

    util_dumper_printf(dumper, "second = ");
    dump_value(dumper, &pair.second);
    util_dumper_newline(dumper);

    util_dumper_unpad(dumper);
    util_dumper_printf(dumper, "}");
}

static void dump_selector(struct util_dumper *const dumper, const struct dicey_selector selector) {
    assert(dumper);

    util_dumper_printf(dumper, "(%s:%s)", selector.trait, selector.elem);
}

static void dump_tuple(struct util_dumper *const dumper, const struct dicey_value *const value) {
    assert(dumper && value);

    struct dicey_list list = { 0 };
    const enum dicey_error err = dicey_value_get_tuple(value, &list);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(dumper, "(");

    util_dumper_pad(dumper);

    dump_list(dumper, &list);

    util_dumper_unpad(dumper);

    util_dumper_printf(dumper, ")");
}

static void dump_value(struct util_dumper *const dumper, const struct dicey_value *const value) {
    assert(dumper && value);

    const enum dicey_type type = dicey_value_get_type(value);

    util_dumper_printf(dumper, "%s%s", dicey_type_name(type), dicey_type_is_container(type) ? "" : ":");

    switch (type) {
    default:
        assert(false);
        return;

    case DICEY_TYPE_UNIT:
        util_dumper_printf(dumper, "()");
        break;

    case DICEY_TYPE_BOOL:
        {
            bool dest;
            const enum dicey_error err = dicey_value_get_bool(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%s", dest ? "true" : "false");
            break;
        }

    case DICEY_TYPE_BYTE:
        {
            uint8_t dest;
            const enum dicey_error err = dicey_value_get_byte(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRIu8, dest);
            break;
        }

    case DICEY_TYPE_FLOAT:
        {
            double dest;
            const enum dicey_error err = dicey_value_get_float(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%f", dest);
            break;
        }

    case DICEY_TYPE_INT16:
        {
            int16_t dest;
            const enum dicey_error err = dicey_value_get_i16(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRId16, dest);
            break;
        }

    case DICEY_TYPE_INT32:
        {
            int32_t dest;
            const enum dicey_error err = dicey_value_get_i32(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRId32, dest);

            break;
        }

    case DICEY_TYPE_INT64:
        {
            int64_t dest;
            const enum dicey_error err = dicey_value_get_i64(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRId64, dest);

            break;
        }

    case DICEY_TYPE_UINT16:
        {
            uint16_t dest;
            const enum dicey_error err = dicey_value_get_u16(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRIu16, dest);

            break;
        }

    case DICEY_TYPE_UINT32:
        {
            uint32_t dest;
            const enum dicey_error err = dicey_value_get_u32(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRIu32, dest);

            break;
        }

    case DICEY_TYPE_UINT64:
        {
            uint64_t dest;
            const enum dicey_error err = dicey_value_get_u64(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%" PRIu64, dest);

            break;
        }

    case DICEY_TYPE_ARRAY:
        dump_array(dumper, value);

        break;

    case DICEY_TYPE_TUPLE:
        {
            dump_tuple(dumper, value);
            break;
        }

    case DICEY_TYPE_PAIR:
        {
            dump_pair(dumper, value);

            break;
        }

    case DICEY_TYPE_BYTES:
        {
            const uint8_t *dest;
            size_t nbytes;
            const enum dicey_error err = dicey_value_get_bytes(value, &dest, &nbytes);
            assert(!err);
            (void) err; // silence unused warning

            dump_hex_short(dumper, dest, nbytes);

            break;
        }

    case DICEY_TYPE_STR:
        {
            const char *dest;
            const enum dicey_error err = dicey_value_get_str(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "\"%s\"", dest);

            break;
        }

    case DICEY_TYPE_UUID:
        {
            struct dicey_uuid dest = { 0 };
            enum dicey_error err = dicey_value_get_uuid(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            char uuid_str[UTIL_UUID_STR_LEN] = { 0 };
            err = util_uuid_to_string(dest, uuid_str, sizeof uuid_str);
            assert(!err);

            util_dumper_printf(dumper, "%s", uuid_str);

            break;
        }

    case DICEY_TYPE_PATH:
        {
            const char *dest;
            const enum dicey_error err = dicey_value_get_path(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            util_dumper_printf(dumper, "%s", dest);

            break;
        }

    case DICEY_TYPE_SELECTOR:
        {
            struct dicey_selector dest;
            const enum dicey_error err = dicey_value_get_selector(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            dump_selector(dumper, dest);

            break;
        }

    case DICEY_TYPE_ERROR:
        {
            struct dicey_errmsg dest;
            const enum dicey_error err = dicey_value_get_error(value, &dest);
            assert(!err);
            (void) err; // silence unused warning

            if (dest.message) {
                util_dumper_printf(dumper, "(code = %" PRIi16 ", message = \"%s\")", dest.code, dest.message);
            } else {
                util_dumper_printf(dumper, "(code = %" PRIi16 ")", dest.code);
            }

            break;
        }
    }
}

static void dump_message(struct util_dumper *const dumper, const struct dicey_packet packet) {
    assert(dumper);

    struct dicey_message message = { 0 };
    const enum dicey_error err = dicey_packet_as_message(packet, &message);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printlnf(dumper, "message {");

    util_dumper_pad(dumper);

    util_dumper_printlnf(dumper, "kind = %s", dicey_op_to_string(message.type));
    util_dumper_printlnf(dumper, "path = \"%s\"", message.path);

    util_dumper_printf(dumper, "selector = ");
    dump_selector(dumper, message.selector);
    util_dumper_newline(dumper);

    if (dicey_op_requires_payload(message.type)) {
        util_dumper_printf(dumper, "value = ");
        dump_value(dumper, &message.value);
        util_dumper_newline(dumper);
    }

    util_dumper_unpad(dumper);
    util_dumper_printlnf(dumper, "}");
}

void util_dumper_dump_packet(struct util_dumper *const dumper, const struct dicey_packet packet) {
    assert(dumper && dicey_packet_is_valid(packet));

    uint32_t seq = 0U;
    const enum dicey_error err = dicey_packet_get_seq(packet, &seq);
    assert(!err);
    (void) err; // silence unused warning

    util_dumper_printf(dumper, "#%" PRIu32 " = ", seq);

    switch (dicey_packet_get_kind(packet)) {
    default:
        assert(false);
        return;

    case DICEY_PACKET_KIND_HELLO:
        dump_hello(dumper, packet);
        break;

    case DICEY_PACKET_KIND_BYE:
        dump_bye(dumper, packet);
        break;

    case DICEY_PACKET_KIND_MESSAGE:
        dump_message(dumper, packet);
        break;
    }
}

void util_dumper_dump_value(struct util_dumper *dumper, const struct dicey_value *value) {
    assert(dumper && value);

    dump_value(dumper, value);
    util_dumper_newline(dumper);
}
