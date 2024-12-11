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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include <cJSON.h>

static enum dicey_type json_deduce_dicey_type(const cJSON *const item) {
    if (!item || cJSON_IsNull(item)) {
        return DICEY_TYPE_UNIT;
    }

    if (cJSON_IsArray(item)) {
        return DICEY_TYPE_ARRAY;
    }

    if (cJSON_IsObject(item)) {
        return DICEY_TYPE_TUPLE;
    }

    if (cJSON_IsString(item)) {
        return DICEY_TYPE_STR;
    }

    if (cJSON_IsNumber(item)) {
        return DICEY_TYPE_FLOAT;
    }

    if (cJSON_IsBool(item)) {
        return DICEY_TYPE_BOOL;
    }

    return DICEY_TYPE_INVALID;
}

static bool json_get_double(double *const dest, const cJSON *const item) {
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    *dest = cJSON_GetNumberValue(item);

    return true;
}

static bool json_get_double_item(double *const dest, const cJSON *const root, const char *const key) {
    const cJSON *const item = cJSON_GetObjectItem(root, key);
    if (!item) {
        return false;
    }

    return json_get_double(dest, item);
}

#define JSON_IMPL_GET_INT_ITEM(TYPE, TYPE_MIN, TYPE_MAX)                                                               \
    static bool json_get_##TYPE(TYPE##_t *const dest, const cJSON *const root, const char *const item) {               \
        double value = .0;                                                                                             \
        if (!json_get_double_item(&value, root, item)) {                                                               \
            return false;                                                                                              \
        }                                                                                                              \
        if (value < (TYPE_MIN) || value > (TYPE_MAX)) {                                                                \
            return false;                                                                                              \
        }                                                                                                              \
        *dest = (TYPE##_t) value;                                                                                      \
        return true;                                                                                                   \
    }

JSON_IMPL_GET_INT_ITEM(uint32, 0, UINT32_MAX)

static uint32_t json_try_get_seq(const cJSON *const root) {
    uint32_t seq = 0;
    return json_get_uint32(&seq, root, "seq") ? seq : 0;
}

static enum dicey_bye_reason json_to_bye_reason(const cJSON *const item) {
    const char *const value = cJSON_GetStringValue(item);

    const enum dicey_bye_reason values[] = {
        DICEY_BYE_REASON_SHUTDOWN,
        DICEY_BYE_REASON_ERROR,
    };

    const enum dicey_bye_reason *const end = values + sizeof values / sizeof *values;

    for (const enum dicey_bye_reason *reason = values; reason < end; ++reason) {
        if (!strcmp(value, dicey_bye_reason_to_string(*reason))) {
            return *reason;
        }
    }

    return DICEY_BYE_REASON_INVALID;
}

static enum dicey_error json_to_bye(struct dicey_packet *const dest, const cJSON *const bye) {
    const uint32_t seq = json_try_get_seq(bye);

    const cJSON *const reason_item = cJSON_GetObjectItem(bye, "reason");
    if (!reason_item) {
        return DICEY_EBADMSG;
    }

    const enum dicey_bye_reason reason = json_to_bye_reason(reason_item);
    if (reason == DICEY_BYE_REASON_INVALID) {
        return DICEY_EBADMSG;
    }

    return dicey_packet_bye(dest, seq, reason);
}

static enum dicey_op json_to_op(const cJSON *const item) {
    const char *const value = cJSON_GetStringValue(item);
    if (value) {
        const enum dicey_op values[] = {
            DICEY_OP_GET, DICEY_OP_SET, DICEY_OP_EXEC, DICEY_OP_SIGNAL, DICEY_OP_RESPONSE,
        };

        const enum dicey_op *const end = values + sizeof values / sizeof *values;

        for (const enum dicey_op *op = values; op < end; ++op) {
            if (!strcmp(value, dicey_op_to_string(*op))) {
                return *op;
            }
        }
    }

    return DICEY_OP_INVALID;
}

static enum dicey_error json_to_version(struct dicey_version *const dest, const cJSON *const version) {
    const char *const value = cJSON_GetStringValue(version);
    if (!value) {
        return DICEY_EBADMSG;
    }

    errno = 0;

    char *end = NULL;
    const unsigned long major = strtoul(value, &end, 10);
    if (end == value || *end != 'r' || errno == ERANGE || major > UINT16_MAX) {
        return DICEY_EBADMSG;
    }

    const unsigned long revision = strtoul(end + 1, &end, 10);
    if (end == value || *end || errno == ERANGE || revision > UINT16_MAX) {
        return DICEY_EBADMSG;
    }

    *dest = (struct dicey_version) {
        .major = (uint16_t) major,
        .revision = (uint16_t) revision,
    };

    return DICEY_OK;
}

static enum dicey_error json_to_hello(struct dicey_packet *const dest, const cJSON *const hello) {
    const uint32_t seq = json_try_get_seq(hello);

    const cJSON *const name_item = cJSON_GetObjectItem(hello, "version");
    if (!name_item) {
        return DICEY_EBADMSG;
    }

    struct dicey_version version = { 0 };
    const enum dicey_error res = json_to_version(&version, name_item);
    if (res) {
        return res;
    }

    return dicey_packet_hello(dest, seq, version);
}

static enum dicey_error json_to_selector(struct dicey_selector *const dest, const cJSON *const selector) {
    const char *const trait = cJSON_GetStringValue(cJSON_GetObjectItem(selector, "trait"));
    if (!trait) {
        return DICEY_EBADMSG;
    }

    const char *const elem = cJSON_GetStringValue(cJSON_GetObjectItem(selector, "elem"));
    if (!elem) {
        return DICEY_EBADMSG;
    }

    *dest = (struct dicey_selector) {
        .trait = trait,
        .elem = elem,
    };

    return DICEY_OK;
}

static enum dicey_error json_to_value(struct dicey_value_builder *const dest, const cJSON *const value) {
    struct dicey_arg arg = { 0 }; // for simple types
    arg.type = json_deduce_dicey_type(value);

    switch (arg.type) {
    case DICEY_TYPE_UNIT:
        break;

    case DICEY_TYPE_BOOL:
        arg.boolean = (bool) cJSON_IsTrue(value);
        break;

    case DICEY_TYPE_STR:
        arg.str = cJSON_GetStringValue(value);
        break;

    case DICEY_TYPE_FLOAT:
        arg.floating = cJSON_GetNumberValue(value);
        break;

    // for these two, we skip the arg and use list builders
    case DICEY_TYPE_ARRAY:
        {
            // an empty array is deduced as an empty list of unit
            const enum dicey_type type_of_first = json_deduce_dicey_type(cJSON_GetArrayItem(value, 0));

            enum dicey_error err = dicey_value_builder_array_start(dest, type_of_first);
            if (err) {
                return err;
            }

            struct dicey_value_builder item = { 0 };
            const int array_size = cJSON_GetArraySize(value);
            for (int i = 0; i < array_size; ++i) {
                const cJSON *const json_item = cJSON_GetArrayItem(value, i);

                err = dicey_value_builder_next(dest, &item);
                if (err) {
                    return err;
                }

                err = json_to_value(&item, json_item);
                if (err) {
                    return err;
                }
            }

            return dicey_value_builder_array_end(dest);
        }

    case DICEY_TYPE_TUPLE:
        {
            enum dicey_error err = dicey_value_builder_tuple_start(dest);
            if (err) {
                return err;
            }

            struct dicey_value_builder item = { 0 };
            cJSON *child = value->child;
            while (child) {
                err = dicey_value_builder_next(dest, &item);
                if (err) {
                    return err;
                }

                err = json_to_value(&item, child);
                if (err) {
                    return err;
                }

                child = child->next;
            }

            return dicey_value_builder_tuple_end(dest);
        }

    default:
        abort(); // unreachable
    }

    // for default values: use the arg
    return dicey_value_builder_set(dest, arg);
}

static enum dicey_error json_to_message(struct dicey_packet *const dest, const cJSON *const message) {
    const enum dicey_op op = json_to_op(cJSON_GetObjectItem(message, "op"));
    if (op == DICEY_OP_INVALID) {
        return DICEY_EBADMSG;
    }

    const uint32_t seq = json_try_get_seq(message);

    const char *const path = cJSON_GetStringValue(cJSON_GetObjectItem(message, "path"));
    if (!path) {
        return DICEY_EBADMSG;
    }

    struct dicey_selector selector = { 0 };
    const cJSON *const selector_item = cJSON_GetObjectItem(message, "selector");
    if (selector_item) {
        const enum dicey_error res = json_to_selector(&selector, selector_item);
        if (res) {
            return res;
        }
    }

    struct dicey_message_builder msgbuild = { 0 };
    enum dicey_error err = dicey_message_builder_init(&msgbuild);
    if (err) {
        return err;
    }

    err = dicey_message_builder_begin(&msgbuild, op);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_seq(&msgbuild, seq);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&msgbuild, path);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(&msgbuild, selector);
    if (err) {
        goto fail;
    }

    if (dicey_op_requires_payload(op)) {
        const cJSON *const value = cJSON_GetObjectItem(message, "value");
        if (!value) {
            err = DICEY_EBADMSG;
            goto fail;
        }

        struct dicey_value_builder valbuild = { 0 };
        err = dicey_message_builder_value_start(&msgbuild, &valbuild);
        if (err) {
            goto fail;
        }

        err = json_to_value(&valbuild, value);
        if (err) {
            goto fail;
        }

        err = dicey_message_builder_value_end(&msgbuild, &valbuild);
        if (err) {
            goto fail;
        }
    }

    return dicey_message_builder_build(&msgbuild, dest);

fail:
    dicey_message_builder_discard(&msgbuild);
    return err;
}

enum dicey_error util_json_to_dicey(struct dicey_packet *const dest, const void *const bytes, const size_t len) {
    cJSON *const root = cJSON_ParseWithLength((const char *) bytes, len);
    if (!root) {
        return DICEY_EINVAL;
    }

    enum dicey_error res = DICEY_EINVAL;

    const cJSON *const bye = cJSON_GetObjectItem(root, "bye");
    if (bye) {
        res = json_to_bye(dest, bye);
        goto exit;
    }

    const cJSON *const hello = cJSON_GetObjectItem(root, "hello");
    if (hello) {
        res = json_to_hello(dest, hello);
        goto exit;
    }

    const cJSON *const message = cJSON_GetObjectItem(root, "message");
    if (message) {
        res = json_to_message(dest, message);
        goto exit;
    }

exit:
    cJSON_Delete(root);

    return res;
}
