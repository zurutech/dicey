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

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dicey/core/data-info.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/views.h>

#include "dtf/dtf.h"

#include "sup/trace.h"
#include "sup/view-ops.h"

static bool is_valid_forward(const enum dicey_op from, const enum dicey_op to) {
    const bool from_is_get = from == DICEY_OP_GET, to_is_get = to == DICEY_OP_GET;

    return from_is_get == to_is_get; // GETs can only be forwarded to GETs, and nothing can forward as GETs
}

static enum dicey_op msgkind_from_dtf(const ptrdiff_t kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
    case DTF_PAYLOAD_HELLO:
    case DTF_PAYLOAD_BYE:
        return DICEY_OP_INVALID;

    case DTF_PAYLOAD_SET:
        return DICEY_OP_SET;

    case DTF_PAYLOAD_GET:
        return DICEY_OP_GET;

    case DTF_PAYLOAD_EXEC:
        return DICEY_OP_EXEC;

    case DTF_PAYLOAD_EVENT:
        return DICEY_OP_EVENT;

    case DTF_PAYLOAD_RESPONSE:
        return DICEY_OP_RESPONSE;
    }
}

static enum dicey_packet_kind pktkind_from_dtf(const enum dtf_payload_kind kind) {
    switch (kind) {
    default:
        assert(false);

    case DTF_PAYLOAD_INVALID:
        return DICEY_PACKET_KIND_INVALID;

    case DTF_PAYLOAD_HELLO:
        return DICEY_PACKET_KIND_HELLO;

    case DTF_PAYLOAD_BYE:
        return DICEY_PACKET_KIND_BYE;

    case DTF_PAYLOAD_SET:
    case DTF_PAYLOAD_GET:
    case DTF_PAYLOAD_EXEC:
    case DTF_PAYLOAD_EVENT:
    case DTF_PAYLOAD_RESPONSE:
        return DICEY_PACKET_KIND_MESSAGE;
    }
}

static enum dicey_error validate_bye(const struct dicey_packet packet) {
    return dicey_packet_as_bye(packet, &(struct dicey_bye) { 0 });
}

static enum dicey_error validate_value(const struct dicey_value *value);

static enum dicey_error validate_value_list(const struct dicey_list *const list) {
    assert(list);

    struct dicey_iterator iter = dicey_list_iter(list);

    struct dicey_value value = { 0 };

    while (dicey_iterator_has_next(iter)) {
        const enum dicey_error next_err = dicey_iterator_next(&iter, &value);
        if (next_err) {
            return next_err;
        }

        const enum dicey_error validate_err = validate_value(&value);
        if (validate_err) {
            return validate_err;
        }
    }

    return DICEY_OK;
}

static enum dicey_error validate_value(const struct dicey_value *const value) {
    assert(value);

    const enum dicey_type type = dicey_value_get_type(value);

    switch (type) {
    default:
        return TRACE(DICEY_EINVAL);

    case DICEY_TYPE_UNIT:
    case DICEY_TYPE_BOOL:
    case DICEY_TYPE_BYTE:
    case DICEY_TYPE_FLOAT:
    case DICEY_TYPE_INT16:
    case DICEY_TYPE_INT32:
    case DICEY_TYPE_INT64:
    case DICEY_TYPE_UINT16:
    case DICEY_TYPE_UINT32:
    case DICEY_TYPE_UINT64:
    case DICEY_TYPE_UUID:
        // all fixed-size types require no validation except size validation
        return DICEY_OK;

    case DICEY_TYPE_BYTES:
        {
            struct dtf_probed_bytes bytes = value->_data.bytes;

            if ((bytes.data && bytes.len) || (!bytes.data && !bytes.len)) {
                return DICEY_OK;
            } else {
                return TRACE(DICEY_EINVAL);
            }
        }

    case DICEY_TYPE_ARRAY:
    case DICEY_TYPE_TUPLE:
        {
            struct dicey_list list = { 0 };

            const enum dicey_error as_list_err =
                type == DICEY_TYPE_ARRAY ? dicey_value_get_array(value, &list) : dicey_value_get_tuple(value, &list);

            return as_list_err ? as_list_err : validate_value_list(&list);
        }

    case DICEY_TYPE_PAIR:
        {
            struct dicey_pair pair = { 0 };

            const enum dicey_error as_pair_err = dicey_value_get_pair(value, &pair);
            if (as_pair_err) {
                return as_pair_err;
            }

            const enum dicey_error validate_first_err = validate_value(&pair.first);
            if (validate_first_err) {
                return validate_first_err;
            }

            return validate_value(&pair.second);
        }

    case DICEY_TYPE_STR:
        return DICEY_OK; // the null string is valid and a zero-length string

    case DICEY_TYPE_PATH:
        return TRACE(value->_data.str ? DICEY_OK : DICEY_EINVAL);

    case DICEY_TYPE_SELECTOR:
        return TRACE(dicey_selector_is_valid(value->_data.selector) ? DICEY_OK : DICEY_EINVAL);

    case DICEY_TYPE_ERROR:
        // errors are always valid, codes are arbitrary and strings may be omitted
        return DICEY_OK;
    }
}

static enum dicey_error validate_message(const struct dicey_packet packet) {
    struct dicey_message message = { 0 };

    const enum dicey_error as_message_err = dicey_packet_as_message(packet, &message);
    if (as_message_err) {
        return as_message_err;
    }

    return dicey_op_requires_payload(message.type) ? validate_value(&message.value) : DICEY_OK;
}

static struct dicey_version version_from_dtf(const uint32_t version) {
    return (struct dicey_version) {
        .major = (uint16_t) (version >> (sizeof(uint16_t) * CHAR_BIT)),
        .revision = (uint16_t) (version & UINT16_MAX),
    };
}

static uint32_t version_to_dtf(const struct dicey_version version) {
    return (uint32_t) (version.major << (sizeof(uint16_t) * CHAR_BIT)) | version.revision;
}

bool dicey_bye_reason_is_valid(const enum dicey_bye_reason reason) {
    switch (reason) {
    default:
    case DICEY_BYE_REASON_INVALID:
        return false;

    case DICEY_BYE_REASON_SHUTDOWN:
    case DICEY_BYE_REASON_ERROR:
        return true;
    }
}

const char *dicey_bye_reason_to_string(const enum dicey_bye_reason reason) {
    switch (reason) {
    default:
    case DICEY_BYE_REASON_INVALID:
        return ">>invalid<<";

    case DICEY_BYE_REASON_SHUTDOWN:
        return "SHUTDOWN";

    case DICEY_BYE_REASON_ERROR:
        return "ERROR";
    }
}

bool dicey_op_is_valid(const enum dicey_op type) {
    switch (type) {
    default:
        return false;

    case DICEY_OP_GET:
    case DICEY_OP_SET:
    case DICEY_OP_EXEC:
    case DICEY_OP_EVENT:
    case DICEY_OP_RESPONSE:
        return true;
    }
}

bool dicey_op_requires_payload(const enum dicey_op kind) {
    switch (kind) {
    default:
        return false;

    case DICEY_OP_SET:
    case DICEY_OP_EXEC:
    case DICEY_OP_EVENT:
    case DICEY_OP_RESPONSE:
        return true;
    }
}

const char *dicey_op_to_string(const enum dicey_op type) {
    switch (type) {
    default:
    case DICEY_OP_INVALID:
        return ">>invalid<<";

    case DICEY_OP_GET:
        return "GET";

    case DICEY_OP_SET:
        return "SET";

    case DICEY_OP_EXEC:
        return "EXEC";

    case DICEY_OP_EVENT:
        return "EVENT";

    case DICEY_OP_RESPONSE:
        return "RESPONSE";
    }
}

enum dicey_error dicey_packet_as_bye(const struct dicey_packet packet, struct dicey_bye *const bye) {
    assert(dicey_packet_is_valid(packet) && bye);

    const union dtf_payload payload = { .header = packet.payload };

    if (dtf_payload_get_kind(payload) != DTF_PAYLOAD_BYE) {
        return TRACE(DICEY_EINVAL);
    }

    const uint32_t reason = payload.bye->reason;

    if (!dicey_bye_reason_is_valid(reason)) {
        return TRACE(DICEY_EINVAL);
    }

    *bye = (struct dicey_bye) {
        .reason = reason,
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_as_hello(const struct dicey_packet packet, struct dicey_hello *const hello) {
    assert(dicey_packet_is_valid(packet) && hello);

    const union dtf_payload payload = { .header = packet.payload };

    if (dtf_payload_get_kind(payload) != DTF_PAYLOAD_HELLO) {
        return TRACE(DICEY_EINVAL);
    }

    *hello = (struct dicey_hello) {
        .version = version_from_dtf(payload.hello->version),
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_as_message(const struct dicey_packet packet, struct dicey_message *const message) {
    assert(dicey_packet_is_valid(packet) && message);

    const union dtf_payload payload = { .header = packet.payload };

    const enum dtf_payload_kind pl_kind = dtf_payload_get_kind(payload);

    if (!dtf_payload_kind_is_message(pl_kind)) {
        return TRACE(DICEY_EINVAL);
    }

    const enum dicey_op type = msgkind_from_dtf(pl_kind);
    if (type == DICEY_OP_INVALID) {
        return TRACE(DICEY_EINVAL);
    }

    const struct dtf_message *const msg = payload.msg;

    struct dtf_message_content content = { 0 };

    const ptrdiff_t content_res = dtf_message_get_content(msg, packet.nbytes, &content);
    if (content_res < 0) {
        return content_res;
    }

    *message = (struct dicey_message) {
        .type = type,
        .path = content.path,
        .selector = content.selector,
    };

    if (content.value) {
        if (!dicey_op_requires_payload(type)) {
            return TRACE(DICEY_EBADMSG);
        }

        struct dtf_probed_value value = { 0 };
        struct dicey_view value_view = { .data = content.value, .len = content.value_len };

        const ptrdiff_t probed_bytes = dtf_value_probe(&value_view, &value);
        if (probed_bytes < 0) {
            return probed_bytes;
        }

        if (value_view.len) {
            return TRACE(DICEY_EINVAL);
        }

        message->value = (struct dicey_value) {
            ._type = value.type,
            ._data = value.data,
        };
    }

    return DICEY_OK;
}

enum dicey_error dicey_packet_forward_message(
    struct dicey_packet *const dest,
    const struct dicey_packet old,
    const uint32_t seq,
    const enum dicey_op type,
    const char *const path,
    const struct dicey_selector selector
) {
    if (!(dest && path && dicey_op_is_valid(type) && dicey_selector_is_valid(selector))) {
        return TRACE(DICEY_EINVAL);
    }

    if (!dicey_packet_is_valid(old)) {
        return TRACE(DICEY_EBADMSG);
    }

    struct dicey_message msg = { 0 };
    if (dicey_packet_as_message(old, &msg) != DICEY_OK) {
        return TRACE(DICEY_EBADMSG);
    }

    if (!is_valid_forward(msg.type, type)) {
        return TRACE(DICEY_EINVAL);
    }

    const enum dtf_payload_kind dtf_kind = (enum dtf_payload_kind) type;

    const ptrdiff_t old_header_size = dtf_message_estimate_header_size(dtf_kind, msg.path, msg.selector);
    if (old_header_size < 0) {
        return old_header_size;
    }

    assert((size_t) old_header_size <= old.nbytes);

    const size_t value_size = old.nbytes - (size_t) old_header_size;

    const struct dtf_result craft_res = dtf_message_write_with_raw_value(
        DICEY_NULL,
        dtf_kind,
        seq,
        path,
        selector,
        // this is fine - we can always do pointer arithmetic on char* pointers, and this will just be memcpy'd
        dicey_view_from((char *) old.payload + old_header_size, value_size)
    );

    if (craft_res.result < 0) {
        return craft_res.result;
    }

    *dest = (struct dicey_packet) {
        .payload = craft_res.data,
        .nbytes = craft_res.size,
    };

    return DICEY_OK;
}

enum dicey_error dicey_packet_bye(
    struct dicey_packet *const dest,
    const uint32_t seq,
    const enum dicey_bye_reason reason
) {
    assert(dest && dicey_bye_reason_is_valid(reason));

    struct dtf_bye *const bye = calloc(1U, sizeof *bye);
    if (!bye) {
        return TRACE(DICEY_ENOMEM);
    }

    const struct dtf_result write_res = dtf_bye_write(
        (struct dicey_view_mut) {
            .data = bye,
            .len = sizeof *bye,
        },
        seq,
        reason
    );

    if (write_res.result < 0) {
        assert(write_res.result != DICEY_EOVERFLOW);
        free(bye);

        return write_res.result;
    }

    *dest = (struct dicey_packet) {
        .payload = bye,
        .nbytes = sizeof *bye,
    };

    return DICEY_OK;
}

void dicey_packet_deinit(struct dicey_packet *const packet) {
    if (packet) {
        // not UB: the payload is always allocated with {c,m}alloc so it's originally void*
        free((void *) packet->payload);

        *packet = (struct dicey_packet) { 0 };
    }
}

enum dicey_error dicey_packet_dump(const struct dicey_packet packet, void **const data, size_t *const nbytes) {
    assert(dicey_packet_is_valid(packet) && data && *data && nbytes);

    struct dicey_view src = { .data = packet.payload, .len = packet.nbytes };
    struct dicey_view_mut dest = { .data = *data, .len = *nbytes };

    const ptrdiff_t dump_err = dicey_view_mut_write(&dest, src);
    if (dump_err < 0) {
        return dump_err;
    }

    *data = dest.data;
    *nbytes = dest.len;

    return DICEY_OK;
}

enum dicey_packet_kind dicey_packet_get_kind(const struct dicey_packet packet) {
    assert(dicey_packet_is_valid(packet));

    const enum dtf_payload_kind dtf_kind = dtf_payload_get_kind((union dtf_payload) {
        .header = packet.payload,
    });

    return pktkind_from_dtf(dtf_kind);
}

enum dicey_error dicey_packet_get_seq(const struct dicey_packet packet, uint32_t *const seq) {
    assert(dicey_packet_is_valid(packet) && seq);

    const ptrdiff_t seq_val = dtf_payload_get_seq((union dtf_payload) {
        .header = packet.payload,
    });

    if (seq_val < 0) {
        return seq_val;
    }

    *seq = (uint32_t) seq_val;

    return DICEY_OK;
}

enum dicey_error dicey_packet_set_seq(const struct dicey_packet packet, const uint32_t seq) {
    assert(dicey_packet_is_valid(packet));

    const ptrdiff_t set_seq_res = dtf_payload_set_seq(
        (union dtf_payload) {
            .header = packet.payload,
        },
        seq
    );

    return set_seq_res < 0 ? set_seq_res : DICEY_OK;
}

enum dicey_error dicey_packet_hello(
    struct dicey_packet *const dest,
    const uint32_t seq,
    const struct dicey_version version
) {
    assert(dest);

    struct dtf_hello *const hello = calloc(1U, sizeof *hello);
    if (!hello) {
        return TRACE(DICEY_ENOMEM);
    }

    const struct dtf_result write_res = dtf_hello_write(
        (struct dicey_view_mut) {
            .data = hello,
            .len = sizeof *hello,
        },
        seq,
        version_to_dtf(version)
    );

    if (write_res.result < 0) {
        assert(write_res.result != DICEY_EOVERFLOW);
        free(hello);

        return write_res.result;
    }

    *dest = (struct dicey_packet) {
        .payload = hello,
        .nbytes = sizeof *hello,
    };

    return DICEY_OK;
}

bool dicey_packet_is_valid(const struct dicey_packet packet) {
    return packet.payload && packet.nbytes;
}

bool dicey_packet_kind_is_valid(const enum dicey_packet_kind kind) {
    switch (kind) {
    default:
    case DICEY_PACKET_KIND_INVALID:
        return false;

    case DICEY_PACKET_KIND_HELLO:
    case DICEY_PACKET_KIND_BYE:
    case DICEY_PACKET_KIND_MESSAGE:
        return true;
    }
}

DICEY_EXPORT const char *dicey_packet_kind_to_string(enum dicey_packet_kind kind) {
    switch (kind) {
    default:
    case DICEY_PACKET_KIND_INVALID:
        return ">>invalid<<";

    case DICEY_PACKET_KIND_HELLO:
        return "HELLO";

    case DICEY_PACKET_KIND_BYE:
        return "BYE";

    case DICEY_PACKET_KIND_MESSAGE:
        return "MESSAGE";
    }
}

enum dicey_error dicey_packet_load(struct dicey_packet *const packet, const void **const data, size_t *const nbytes) {
    if (!(packet && data && *data && nbytes)) {
        return TRACE(DICEY_EINVAL);
    }

    struct dicey_view src = {
        .data = *data,
        .len = *nbytes,
    };

    union dtf_payload payload = { 0 };
    const struct dtf_result load_res = dtf_payload_load(&payload, &src);
    if (load_res.result < 0) {
        return load_res.result;
    }

    assert(load_res.size && load_res.data);

    enum dicey_error err = DICEY_OK;

    const enum dicey_packet_kind kind = pktkind_from_dtf(dtf_payload_get_kind(payload));
    if (!dicey_packet_kind_is_valid(kind)) {
        err = TRACE(DICEY_EBADMSG);
        goto fail;
    }

    *packet = (struct dicey_packet) {
        .payload = load_res.data,
        .nbytes = load_res.size,
    };

#if !defined(DICEY_NO_VALIDATION)
    enum dicey_error validate_err = DICEY_OK;

    switch (kind) {
    case DICEY_PACKET_KIND_BYE:
        validate_err = validate_bye(*packet);
        break;

    case DICEY_PACKET_KIND_HELLO:
        break;

    case DICEY_PACKET_KIND_MESSAGE:
        validate_err = validate_message(*packet);
        break;

    default:
        assert(false); // unreachable or bug
    }

    if (validate_err) {
        // it's useless to report stuff like "invalid message" to the message - the validate_xxx functions reuse the
        // public API, so in this context it means the packet we just loaded is malformed
        err = validate_err == DICEY_EINVAL ? TRACE(DICEY_EBADMSG) : validate_err;
        goto fail;
    }
#endif

    *data = src.data;
    *nbytes = src.len;

    return err;

fail:
    free(load_res.data);
    *packet = (struct dicey_packet) { 0 };

    return err;
}
