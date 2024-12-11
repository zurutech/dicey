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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlschemas.h>
#include <uv.h>

#include <dicey/dicey.h>

#include <util/base64.h>
#include <util/packet-xml.h>
#include <util/strext.h>

#if defined(DICEY_CC_IS_MSVC)
#pragma warning(disable : 4996)
#endif

static uv_once_t libxml_init_once = UV_ONCE_INIT;
xmlSchema *schema = NULL;

static struct xml_type_map {
    const char *name;
    enum dicey_type type;
} mappings[] = {
    {"unit",      DICEY_TYPE_UNIT    },
    { "bool",     DICEY_TYPE_BOOL    },
    { "byte",     DICEY_TYPE_BYTE    },
    { "float",    DICEY_TYPE_FLOAT   },
    { "i16",      DICEY_TYPE_INT16   },
    { "i32",      DICEY_TYPE_INT32   },
    { "i64",      DICEY_TYPE_INT64   },
    { "u16",      DICEY_TYPE_UINT16  },
    { "u32",      DICEY_TYPE_UINT32  },
    { "u64",      DICEY_TYPE_UINT64  },
    { "array",    DICEY_TYPE_ARRAY   },
    { "tuple",    DICEY_TYPE_TUPLE   },
    { "pair",     DICEY_TYPE_PAIR    },
    { "bytes",    DICEY_TYPE_BYTES   },
    { "string",   DICEY_TYPE_STR     },
    { "uuid",     DICEY_TYPE_UUID    },
    { "path",     DICEY_TYPE_PATH    },
    { "selector", DICEY_TYPE_SELECTOR},
    { "error",    DICEY_TYPE_ERROR   },
};

static void libxml_init(void) {
    extern const unsigned char schemas_packet_xsd[];
    extern const unsigned long long schemas_packet_xsd_size;

    LIBXML_TEST_VERSION;
    xmlInitParser();

    assert(schemas_packet_xsd_size <= INT_MAX);

    xmlSchemaParserCtxt *const ctxt =
        xmlSchemaNewMemParserCtxt((const char *) schemas_packet_xsd, (int) schemas_packet_xsd_size);
    schema = xmlSchemaParse(ctxt);
    xmlSchemaFreeParserCtxt(ctxt);

    if (!schema) {
        // we literally embed the schema, so this should never happen
        abort();
    }
}

struct buffer_cache {
    struct buffer {
        void *data;
        void (*destructor)(void *);
    } *buffers;
    size_t len;
    size_t cap;
};

#define BUFFER_CACHE_STARTCAP 8

static void buffer_cache_add(struct buffer_cache *const cache, void *const data, void (*const destructor)(void *)) {
    if (cache->len == cache->cap) {
        const size_t new_cap = cache->cap ? (cache->cap * 3) / 2 : BUFFER_CACHE_STARTCAP;

        struct buffer *const new_buffers = realloc(cache->buffers, new_cap * sizeof *new_buffers);
        if (!new_buffers) {
            // remove if this ever becomes part of an actual library, and not just a sample
            abort();
        }

        cache->buffers = new_buffers;
        cache->cap = new_cap;
    }

    cache->buffers[cache->len++] = (struct buffer) {
        .data = data,
        .destructor = destructor,
    };
}

static void buffer_cache_clear(struct buffer_cache *const cache) {
    if (cache->buffers) {
        const struct buffer *const end = cache->buffers + cache->len;
        for (struct buffer *buf = cache->buffers; buf < end; ++buf) {
            buf->destructor(buf->data);
        }

        free(cache->buffers);
    }

    *cache = (struct buffer_cache) { 0 };
}

struct selector_info {
    struct dicey_view trait, elem;
};

static bool split_selector(const char *const str, struct selector_info *const dest) {
    if (!str || !dest) {
        return false;
    }

    const char *const sep = strchr(str, ':');
    if (!sep) {
        return false;
    }

    const ptrdiff_t trait_len = sep - str;
    assert(trait_len >= 0);

    if (!trait_len) {
        return false;
    }

    const size_t elem_len = strlen(sep + 1);
    if (!elem_len) {
        return false;
    }

    *dest = (struct selector_info) {
        .trait =
            (struct dicey_view) {
                                 .data = str,
                                 .len = (size_t) trait_len,
                                 },
        .elem =
            (struct dicey_view) {
                                 .data = sep + 1,
                                 .len = elem_len,
                                 },
    };

    return true;
}

static bool str_to_bool(bool *const dest, const char *const str) {
    assert(dest);

    if (!str) {
        return false;
    }

    if (!strcmp(str, "true")) {
        *dest = true;
        return true;
    }

    if (!strcmp(str, "false")) {
        *dest = false;
        return true;
    }

    return false;
}

static bool str_to_double(double *const dest, const char *const str) {
    assert(dest);

    if (!str) {
        return false;
    }

    char *end = NULL;
    errno = 0;
    const double value = strtod(str, &end);
    if (end == str || *end || errno == ERANGE) {
        return false;
    }

    *dest = value;

    return true;
}

#define IMPL_STR_TO_INT(TYPE, TYPE_MIN, TYPE_MAX)                                                                      \
    static bool str_to_##TYPE(TYPE##_t *const dest, const char *const str) {                                           \
        assert(dest);                                                                                                  \
        if (!str) {                                                                                                    \
            return false;                                                                                              \
        }                                                                                                              \
        char *end = NULL;                                                                                              \
        errno = 0;                                                                                                     \
        const long long value = strtoll(str, &end, 10);                                                                \
        if (end == str || *end || value < (TYPE_MIN) || value > (TYPE_MAX) || errno == ERANGE) {                       \
            return false;                                                                                              \
        }                                                                                                              \
        *dest = (TYPE##_t) value;                                                                                      \
        return true;                                                                                                   \
    }

IMPL_STR_TO_INT(int16, INT16_MIN, INT16_MAX)
IMPL_STR_TO_INT(int32, INT32_MIN, INT32_MAX)
IMPL_STR_TO_INT(int64, INT64_MIN, INT64_MAX)

#define IMPL_STR_TO_UINT(TYPE, TYPE_MAX)                                                                               \
    static bool str_to_##TYPE(TYPE##_t *const dest, const char *const str) {                                           \
        if (!str) {                                                                                                    \
            return false;                                                                                              \
        }                                                                                                              \
        char *end = NULL;                                                                                              \
        errno = 0;                                                                                                     \
        const unsigned long long value = strtoull(str, &end, 10);                                                      \
        if (end == str || *end || value > (TYPE_MAX) || errno == ERANGE) {                                             \
            return false;                                                                                              \
        }                                                                                                              \
        *dest = (TYPE##_t) value;                                                                                      \
        return true;                                                                                                   \
    }

IMPL_STR_TO_UINT(uint8, UINT8_MAX)
IMPL_STR_TO_UINT(uint16, UINT16_MAX)
IMPL_STR_TO_UINT(uint32, UINT32_MAX)
IMPL_STR_TO_UINT(uint64, UINT64_MAX)

static enum dicey_type str_to_type(const char *const str) {
    if (!str) {
        return DICEY_TYPE_INVALID;
    }

    const struct xml_type_map *const end = mappings + sizeof mappings / sizeof *mappings;
    for (const struct xml_type_map *map = mappings; map < end; ++map) {
        if (!strcmp(str, map->name)) {
            return map->type;
        }
    }

    return DICEY_TYPE_INVALID;
}

static bool str_to_uuid(struct dicey_uuid *const dest, const char *const str) {
    assert(dest);

    if (!str) {
        return false;
    }

    const enum dicey_error err = dicey_uuid_from_string(dest, str);
    return !err;
}

static char *str_trimend(char *const str) {
    if (!str) {
        return NULL;
    }

    const size_t len = strlen(str);
    if (!len) {
        return str;
    }

    char *end = str + len - 1;
    while (end > str && isspace((int) *end)) {
        --end;
    }

    *end = '\0';

    return str;
}

static struct util_xml_error *xml_verror(const char *const msg, va_list args) {
    assert(msg);

    va_list args_copy;
    va_copy(args_copy, args);

    const size_t msg_size = vsnprintf(NULL, 0, msg, args_copy) + 1;
    va_end(args_copy);

    struct util_xml_error *const err = calloc(1, sizeof *err + msg_size);
    assert(err);

    vsnprintf(err->message, msg_size, msg, args);

    return err;
}

static struct util_xml_error *xml_error(const char *const msg, ...) {
    assert(msg);

    va_list args;
    va_start(args, msg);

    struct util_xml_error *const ret = xml_verror(msg, args);
    assert(ret);

    va_end(args);

    return ret;
}

static struct util_xml_error *xml_error_on(const xmlNode *const node, const char *const msg, ...) {
    assert(node && msg);

    va_list args;
    va_start(args, msg);

    struct util_xml_error *const ret = xml_verror(msg, args);
    assert(ret);

    ret->line = node->line;

    va_end(args);

    return ret;
}

static struct util_xml_error *xml_error_from_libxml(const xmlError *const err) {
    assert(err);

    struct util_xml_error *ret = xml_error_on(err->node, "%s", str_trimend(err->message));
    assert(ret);

    ret->col = err->int2;

    return ret;
}

static void xml_errors_add(struct util_xml_errors *const errors, const struct util_xml_error *const err) {
    assert(errors && err);

    if (errors->nerrs == errors->cap) {
        const size_t new_cap = errors->cap ? (errors->cap * 3) / 2 : 8;

        if (new_cap > UINT32_MAX) {
            // remove if this ever becomes part of an actual library, and not just a sample
            abort();
        }

        const struct util_xml_error **const new_errors = realloc(errors->errors, new_cap * sizeof *new_errors);
        if (!new_errors) {
            // remove if this ever becomes part of an actual library, and not just a sample
            abort();
        }

        errors->errors = new_errors;
        errors->cap = (uint32_t) new_cap;
    }

    errors->errors[errors->nerrs++] = err;
}

static void xml_validation_err_cb(void *ctx, const xmlError *err);

static struct util_xml_errors xml_validate_with_internal_schema(xmlDoc *const doc) {
    assert(schema);

    struct util_xml_errors errors = { 0 };

    xmlSchemaValidCtxt *const ctxt = xmlSchemaNewValidCtxt(schema);

    if (ctxt) {
        // the cast silences the fact that libxml2 switched a parameter to const in a recent version
        xmlSchemaSetValidStructuredErrors(ctxt, (xmlStructuredErrorFunc) &xml_validation_err_cb, &errors);

        const bool fail = xmlSchemaValidateDoc(ctxt, doc);
        xmlSchemaFreeValidCtxt(ctxt);

        assert(fail == (bool) { errors.errors });
        (void) fail; // suppress unused variable warning
    }

    return errors;
}

static void xml_validation_err_cb(void *const ctx, const xmlError *const err) {
    assert(ctx && err);

    xml_errors_add(ctx, xml_error_from_libxml(err));
}

static struct util_xml_error *parse_selector(
    struct dicey_selector *const dest,
    const char *const selstr,
    struct buffer_cache *const cache
) {
    assert(dest && cache);

    if (!selstr) {
        return xml_error("missing selector string");
    }

    struct selector_info info = { 0 };
    if (!split_selector(selstr, &info)) {
        return xml_error("invalid selector string: '%s'", selstr);
    }

    char *const trait = strndup(info.trait.data, info.trait.len);
    char *const elem = strndup(info.elem.data, info.elem.len);

    if (!trait || !elem) {
        abort(); // no way to recover from memory exhaustion here
    }

    buffer_cache_add(cache, trait, &free);
    buffer_cache_add(cache, elem, &free);

    *dest = (struct dicey_selector) {
        .trait = trait,
        .elem = elem,
    };

    return NULL;
}

static struct util_xml_error *xml_check_name(const xmlNode *const node, const char *const name) {
    assert(node && name);

    if (xmlStrcmp(node->name, (const xmlChar *) name)) {
        return xml_error_on(node, "expected '%s' element, got '%s'", name, (const char *) node->name);
    }

    return NULL;
}

static const xmlNode *xml_next(const xmlNode *node) {
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            const xmlNode *const ret = node;

            return ret;
        }

        node = node->next;
    }

    return NULL;
}

static const xmlNode *xml_advance(const xmlNode **node) {
    assert(node);

    const xmlNode *const ret = xml_next(*node);
    *node = ret ? ret->next : NULL;

    return ret;
}

static struct util_xml_error *xml_deduce_dicey_type(const xmlNode *const item, enum dicey_type *const dest) {
    assert(item);

    const enum dicey_type type = str_to_type((const char *) item->name);
    if (type == DICEY_TYPE_INVALID) {
        return xml_error_on(item, "invalid value type: '%s'", (const char *) item->name);
    }

    *dest = type;

    return NULL;
}

static size_t xml_subelems_count(const xmlNode *const node) {
    size_t count = 0;
    for (const xmlNode *child = node->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            ++count;
        }
    }

    return count;
}

static uint32_t xml_try_get_seq(const xmlNode *const root) {
    uint32_t seq = 0;

    // attempt getting the 'seq' attribute
    xmlChar *const seq_str = xmlGetProp(root, (const xmlChar *) "seq");
    if (seq_str) {
        if (!str_to_uint32(&seq, (const char *) seq_str)) {
            seq = 0;
        }

        xmlFree(seq_str);
    }

    return seq;
}

static struct util_xml_error *xml_get_attribute(const xmlNode *const node, const char *const name, char **const dest) {
    assert(node && name && dest);

    xmlChar *const value = xmlGetProp(node, (const xmlChar *) name);
    if (!value) {
        return xml_error_on(node, "missing '%s' attribute", name);
    }

    *dest = (char *) value;

    return NULL;
}

static struct util_xml_error *xml_get_bye_reason(const xmlNode *const item, enum dicey_bye_reason *const dest) {
    assert(item && dest);

    char *value = NULL;
    struct util_xml_error *const err = xml_get_attribute(item, "reason", &value);
    if (err) {
        return err;
    }

    const enum dicey_bye_reason values[] = {
        DICEY_BYE_REASON_SHUTDOWN,
        DICEY_BYE_REASON_ERROR,
    };

    const enum dicey_bye_reason *const end = values + sizeof values / sizeof *values;

    const enum dicey_bye_reason *reason = values;
    for (; reason < end; ++reason) {
        if (!strcmp(value, dicey_bye_reason_to_string(*reason))) {
            break;
        }
    }

    xmlFree(value);
    if (reason == end) {
        return xml_error_on(item, "invalid 'bye_reason' attribute: '%s'", value);
    }

    *dest = *reason;
    return NULL;
}

static struct util_xml_error *xml_to_bye(
    struct dicey_packet *const dest,
    const uint32_t seq,
    const xmlNode *const bye
) {
    enum dicey_bye_reason reason = DICEY_BYE_REASON_INVALID;

    struct util_xml_error *const reason_err = xml_get_bye_reason(bye, &reason);
    if (reason_err) {
        return reason_err;
    }

    const enum dicey_error err = dicey_packet_bye(dest, seq, reason);

    return err ? xml_error_on(bye, "failed to create 'bye' packet: %s", dicey_error_msg(err)) : NULL;
}

static struct util_xml_error *xml_get_errcode(int16_t *const dest, const xmlNode *const errmsg) {
    char *value = NULL;
    struct util_xml_error *err = xml_get_attribute(errmsg, "code", &value);
    if (err) {
        return err;
    }

    const bool conv_ok = str_to_int16(dest, (const char *) value);
    if (!conv_ok) {
        err = xml_error_on(errmsg, "invalid 'code' attribute: '%s'", value);
    }

    xmlFree(value);

    return err;
}

static struct util_xml_error *xml_get_error(
    struct dicey_error_arg *const dest,
    const xmlNode *const errmsg,
    struct buffer_cache *const cache
) {
    assert(dest && errmsg && cache);

    struct util_xml_error *err = xml_get_errcode(&dest->code, errmsg);
    if (err) {
        return err;
    }

    xmlChar *const value = xmlNodeGetContent(errmsg);
    if (value) {
        buffer_cache_add(cache, value, xmlFree);

        dest->message = (const char *) value;
    }

    return NULL;
}

static struct util_xml_error *xml_get_op(const xmlNode *const item, enum dicey_op *const dest) {
    assert(item && dest);

    char *value = NULL;
    struct util_xml_error *err = xml_get_attribute(item, "op", &value);
    if (err) {
        return err;
    }

    const enum dicey_op values[] = {
        DICEY_OP_GET, DICEY_OP_SET, DICEY_OP_EXEC, DICEY_OP_SIGNAL, DICEY_OP_RESPONSE,
    };

    enum dicey_op found = DICEY_OP_INVALID;
    const enum dicey_op *const end = values + sizeof values / sizeof *values;

    for (const enum dicey_op *op = values; op < end; ++op) {
        if (!strcmp((char *) value, dicey_op_to_string(*op))) {
            found = *op;
            break;
        }
    }

    if (found == DICEY_OP_INVALID) {
        err = xml_error_on(item, "invalid 'op' attribute: '%s'", value);
    }

    xmlFree(value);

    *dest = found;

    return err;
}

static struct util_xml_error *xml_get_path(
    const char **const dest,
    const xmlNode *const item,
    struct buffer_cache *const cache
) {
    assert(item && dest && cache);

    struct util_xml_error *err = xml_check_name(item, "path");
    if (err) {
        return err;
    }

    xmlChar *const value = xmlNodeGetContent(item);
    if (!value) {
        return xml_error_on(item, "missing 'path' content");
    }

    char *dup = strdup((const char *) value);

    if (dup) {
        buffer_cache_add(cache, dup, &free);
        *dest = dup;
    } else {
        abort(); // again, no way to recover from memory exhaustion here
    }

    xmlFree(value);

    return NULL;
}

static struct util_xml_error *xml_get_selector(
    struct dicey_selector *const dest,
    const xmlNode *const item,
    struct buffer_cache *const cache
) {
    assert(item && dest && cache);

    struct util_xml_error *err = xml_check_name(item, "selector");
    if (err) {
        return err;
    }

    xmlChar *const value = xmlNodeGetContent(item);
    if (!value) {
        return xml_error_on(item, "missing 'selector' content");
    }

    err = parse_selector(dest, (const char *) value, cache);
    xmlFree(value);

    return err;
}

static struct util_xml_error *xml_get_version(struct dicey_version *const dest, const xmlNode *const hello) {
    assert(dest && hello);

    char *value = NULL;
    struct util_xml_error *err = xml_get_attribute(hello, "version", &value);
    if (err) {
        return err;
    }

    errno = 0;

    char *end = NULL;
    const unsigned long major = strtoul(value, &end, 10);
    if (end == value || *end != 'r' || errno == ERANGE || major > UINT16_MAX) {
        err = xml_error_on(hello, "invalid 'hello:version' attribute: '%s'", value);
        goto exit;
    }

    const unsigned long revision = strtoul(end + 1, &end, 10);
    if (end == value || *end || errno == ERANGE || revision > UINT16_MAX) {
        err = xml_error_on(hello, "invalid 'hello:version' attribute: '%s'", value);
        goto exit;
    }

    *dest = (struct dicey_version) {
        .major = (uint16_t) major,
        .revision = (uint16_t) revision,
    };

exit:
    xmlFree(value);

    return err;
}

static struct util_xml_error *xml_to_value(
    struct dicey_value_builder *dest,
    const xmlNode *value,
    struct buffer_cache *cache
);

static struct util_xml_error *xml_to_list(
    struct dicey_value_builder *const list_builder,
    const xmlNode *child,
    struct buffer_cache *const cache
) {
    for (; child; child = xml_next(child->next)) {
        struct dicey_value_builder item_builder = { 0 };
        const enum dicey_error next_err = dicey_value_builder_next(list_builder, &item_builder);
        if (next_err) {
            return xml_error_on(child, "failed to start building list item: %s", dicey_error_msg(next_err));
        }

        struct util_xml_error *const err = xml_to_value(&item_builder, child, cache);
        if (err) {
            return err;
        }
    }

    return NULL;
}

static struct util_xml_error *xml_to_array(
    struct dicey_value_builder *const array_builder,
    const xmlNode *const array,
    struct buffer_cache *const cache
) {
    char *typename = NULL;
    struct util_xml_error *err = xml_get_attribute(array, "type", &typename);
    if (err) {
        return err;
    }

    buffer_cache_add(cache, typename, xmlFree);

    const enum dicey_type type = str_to_type(typename);

    if (type == DICEY_TYPE_INVALID) {
        return xml_error_on(array, "invalid 'type' attribute: '%s'", typename);
    }

    const enum dicey_error start_err = dicey_value_builder_array_start(array_builder, type);
    if (start_err) {
        return xml_error_on(array, "failed to start building array: %s", dicey_error_msg(start_err));
    }

    err = xml_to_list(array_builder, xml_next(array->children), cache);
    if (err) {
        return err;
    }

    const enum dicey_error end_err = dicey_value_builder_array_end(array_builder);
    if (end_err) {
        return xml_error_on(array, "failed to end building array: %s", dicey_error_msg(end_err));
    }

    return NULL;
}

static struct util_xml_error *xml_to_hello(
    struct dicey_packet *const dest,
    const uint32_t seq,
    const xmlNode *const hello
) {
    struct dicey_version version = { 0 };
    struct util_xml_error *const err = xml_get_version(&version, hello);
    if (err) {
        return err;
    }

    const enum dicey_error craft_err = dicey_packet_hello(dest, seq, version);
    if (craft_err) {
        return xml_error_on(hello, "failed to create 'hello' packet: %s", dicey_error_msg(craft_err));
    }

    return NULL;
}

static struct util_xml_error *xml_to_pair(
    struct dicey_value_builder *const dest,
    const xmlNode *const pair,
    struct buffer_cache *const cache
) {
    const enum dicey_error start_err = dicey_value_builder_pair_start(dest);
    if (start_err) {
        return xml_error_on(pair, "failed to start building pair: %s", dicey_error_msg(start_err));
    }

    struct util_xml_error *const list_err = xml_to_list(dest, xml_next(pair->children), cache);
    if (list_err) {
        return list_err;
    }

    const enum dicey_error end_err = dicey_value_builder_pair_end(dest);
    if (end_err) {
        return xml_error_on(pair, "failed to end building pair: %s", dicey_error_msg(end_err));
    }

    return NULL;
}

static struct util_xml_error *xml_to_tuple(
    struct dicey_value_builder *const dest,
    const xmlNode *const tuple,
    struct buffer_cache *const cache
) {
    const enum dicey_error start_err = dicey_value_builder_tuple_start(dest);
    if (start_err) {
        return xml_error_on(tuple, "failed to start building tuple: %s", dicey_error_msg(start_err));
    }

    struct util_xml_error *const list_err = xml_to_list(dest, xml_next(tuple->children), cache);
    if (list_err) {
        return list_err;
    }

    const enum dicey_error end_err = dicey_value_builder_tuple_end(dest);
    if (end_err) {
        return xml_error_on(tuple, "failed to end building tuple: %s", dicey_error_msg(end_err));
    }

    return NULL;
}

static struct util_xml_error *xml_to_value(
    struct dicey_value_builder *const dest,
    const xmlNode *const value,
    struct buffer_cache *const cache
) {
    struct dicey_arg arg = { 0 }; // for simple types

    struct util_xml_error *err = xml_deduce_dicey_type(value, &arg.type);
    if (err) {
        return err;
    }

    xmlChar *const xcontent = xmlNodeGetContent(value->children);
    const char *const content = (const char *) xcontent;

    size_t content_len = 0U;
    if (xcontent) {
        buffer_cache_add(cache, xcontent, xmlFree);
        content_len = xmlStrlen(xcontent);
    }

    // cast to const char* to make our life easier

    switch (arg.type) {
    case DICEY_TYPE_UNIT:
        break;

    case DICEY_TYPE_BOOL:
        {
            bool val = false;
            if (!str_to_bool(&val, content)) {
                return xml_error_on(value, "invalid boolean value: '%s'", content);
            }

            arg.boolean = val;
        }

        break;

    case DICEY_TYPE_BYTE:
        if (!str_to_uint8(&arg.byte, content)) {
            return xml_error_on(value, "invalid byte value: '%s'", content);
        }

        break;

    case DICEY_TYPE_FLOAT:
        if (!str_to_double(&arg.floating, content)) {
            return xml_error_on(value, "invalid float value: '%s'", content);
        }

        break;

    case DICEY_TYPE_INT16:
        if (!str_to_int16(&arg.i16, content)) {
            return xml_error_on(value, "invalid int16 value: '%s'", content);
        }

        break;

    case DICEY_TYPE_INT32:
        if (!str_to_int32(&arg.i32, content)) {
            return xml_error_on(value, "invalid int32 value: '%s'", content);
        }

        break;

    case DICEY_TYPE_INT64:
        if (!str_to_int64(&arg.i64, content)) {
            return xml_error_on(value, "invalid int64 value: '%s'", content);
        }

        break;

    case DICEY_TYPE_UINT16:
        if (!str_to_uint16(&arg.u16, content)) {
            return xml_error_on(value, "invalid uint16 value: '%s'", content);
        }

        break;

    case DICEY_TYPE_UINT32:
        if (!str_to_uint32(&arg.u32, content)) {
            return xml_error_on(value, "invalid uint32 value: '%s'", content);
        }

        break;

    case DICEY_TYPE_UINT64:
        if (!str_to_uint64(&arg.u64, content)) {
            return xml_error_on(value, "invalid uint64 value: '%s'", content);
        }

        break;

    // for these three, we skip the arg and use list builders
    case DICEY_TYPE_ARRAY:
        return xml_to_array(dest, value, cache);

    case DICEY_TYPE_TUPLE:
        return xml_to_tuple(dest, value, cache);

    case DICEY_TYPE_PAIR:
        return xml_to_pair(dest, value, cache);

    case DICEY_TYPE_BYTES:
        {
            size_t len = 0;
            uint8_t *const data = util_base64_decode(content, content_len, &len);

            if (!data) {
                return xml_error_on(value, "invalid base64 value: '%s'", content);
            }

            buffer_cache_add(cache, data, &free);

            if (len > UINT32_MAX) {
                return xml_error_on(value, "base64 value too large (> 4GiB)");
            }

            arg.bytes = (struct dicey_bytes_arg) {
                .data = data,
                .len = (uint32_t) len,
            };

            break;
        }

    case DICEY_TYPE_STR:
    case DICEY_TYPE_PATH:
        if (!content) {
            return xml_error_on(value, "missing content for string or path");
        }

        arg.str = content;

        break;

    case DICEY_TYPE_UUID:
        if (!content) {
            return xml_error_on(value, "missing content for UUID");
        }

        if (!str_to_uuid(&arg.uuid, content)) {
            return xml_error_on(value, "invalid UUID value: '%s'", content);
        }

        break;

    case DICEY_TYPE_SELECTOR:
        {
            err = parse_selector(&arg.selector, content, cache);
            if (err) {
                return err;
            }

            break;
        }

    case DICEY_TYPE_ERROR:
        {
            err = xml_get_error(&arg.error, value, cache);
            if (err) {
                return err;
            }

            break;
        }

    default:
        abort(); // unreachable
    }

    // for default values: use the arg
    const enum dicey_error set_err = dicey_value_builder_set(dest, arg);
    if (set_err) {
        return xml_error_on(value, "failed to set value: %s", dicey_error_msg(set_err));
    }

    return NULL;
}

static struct util_xml_error *xml_to_message(
    struct dicey_packet *const dest,
    const uint32_t seq,
    const xmlNode *const message
) {
    struct dicey_message_builder msgbuild = { 0 };

    enum dicey_op op = DICEY_OP_INVALID;

    struct util_xml_error *err = xml_get_op(message, &op);
    if (err) {
        return err;
    }

    // find first child
    const xmlNode *child = xml_next(message->children);
    if (!child) {
        return xml_error_on(message, "missing 'path' in message");
    }

    struct buffer_cache cache = { 0 };

    const char *path = NULL;

    err = xml_get_path(&path, xml_advance(&child), &cache);
    if (err) {
        goto fail;
    }

    assert(path);

    if (!child) {
        err = xml_error_on(message, "missing 'selector' in message");
        goto fail;
    }

    struct dicey_selector selector = { 0 };
    err = xml_get_selector(&selector, xml_advance(&child), &cache);
    if (err) {
        goto fail;
    }

    enum dicey_error dicey_err = dicey_message_builder_init(&msgbuild);
    if (dicey_err) {
        err = xml_error_on(message, "failed to initialize message builder: %s", dicey_error_msg(dicey_err));
        goto fail;
    }

    dicey_err = dicey_message_builder_begin(&msgbuild, op);
    if (dicey_err) {
        err = xml_error_on(message, "failed to begin message: %s", dicey_error_msg(dicey_err));
        goto fail;
    }

    dicey_err = dicey_message_builder_set_seq(&msgbuild, seq);
    if (dicey_err) {
        err = xml_error_on(message, "failed to set sequence number: %s", dicey_error_msg(dicey_err));
        goto fail;
    }

    dicey_err = dicey_message_builder_set_path(&msgbuild, path);
    if (dicey_err) {
        err = xml_error_on(message, "failed to set path: %s", dicey_error_msg(dicey_err));
        goto fail;
    }

    dicey_err = dicey_message_builder_set_selector(&msgbuild, selector);
    if (dicey_err) {
        err = xml_error_on(message, "failed to set selector: %s", dicey_error_msg(dicey_err));
        goto fail;
    }

    if (dicey_op_requires_payload(op)) {
        const xmlNode *const value = xml_advance(&child);

        if (!value) {
            err = xml_error_on(message, "missing value in message");
            goto fail;
        }

        err = xml_check_name(value, "value");
        if (err) {
            goto fail;
        }

        if (xml_subelems_count(value) != 1) {
            err = xml_error_on(value, "expected exactly one child element in message value");
            goto fail;
        }

        struct dicey_value_builder valbuild = { 0 };
        dicey_err = dicey_message_builder_value_start(&msgbuild, &valbuild);
        if (dicey_err) {
            err = xml_error_on(value, "failed to start building value: %s", dicey_error_msg(dicey_err));
            goto fail;
        }

        err = xml_to_value(&valbuild, xml_next(value->children), &cache);
        if (err) {
            goto fail;
        }

        dicey_err = dicey_message_builder_value_end(&msgbuild, &valbuild);
        if (dicey_err) {
            err = xml_error_on(value, "failed to end building value: %s", dicey_error_msg(dicey_err));
            goto fail;
        }
    }

    // check there are no more elements. This should be taken into account by the XSD anyway
    const xmlNode *const spurious = xml_next(child->next);
    if (spurious) {
        err = xml_error_on(spurious, "unexpected child element(s) in message");
        goto fail;
    }

    dicey_err = dicey_message_builder_build(&msgbuild, dest);
    if (dicey_err) {
        err = xml_error_on(message, "failed to build message: %s", dicey_error_msg(dicey_err));
    }

fail:
    dicey_message_builder_discard(&msgbuild);
    buffer_cache_clear(&cache);

    return err;
}

static struct util_xml_error *xml_to_packet(struct dicey_packet *const dest, const xmlNode *const node) {
    if (xmlStrcmp(node->name, (const xmlChar *) "packet")) {
        return xml_error_on(node, "expected 'packet' element, got '%s'", (const char *) node->name);
    }

    const uint32_t seq = xml_try_get_seq(node);

    if (xml_subelems_count(node) != 1) {
        return xml_error_on(node, "expected exactly one child element (bye, hello, message)");
    }

    const xmlNode *const child = xml_next(node->children);
    assert(child);

    if (xmlStrcmp(child->name, (const xmlChar *) "bye") == 0) {
        return xml_to_bye(dest, seq, child);
    } else if (xmlStrcmp(child->name, (const xmlChar *) "hello") == 0) {
        return xml_to_hello(dest, seq, child);
    } else if (xmlStrcmp(child->name, (const xmlChar *) "message") == 0) {
        return xml_to_message(dest, seq, child);
    } else {
        return xml_error_on(
            child, "expected 'bye', 'hello' or 'message' element, got '%s'", (const char *) child->name
        );
    }
}

void util_xml_errors_deinit(struct util_xml_errors *const errs) {
    if (errs) {
        if (errs->errors) {
            const struct util_xml_error **const end = errs->errors + errs->nerrs;
            for (const struct util_xml_error **err = errs->errors; err < end; ++err) {
                // this originally came from malloc, so this cast is _always_ legal
                free((void *) *err);
            }

            free(errs->errors);
        }

        *errs = (struct util_xml_errors) { 0 };
    }
}

struct util_xml_errors util_xml_to_dicey(struct dicey_packet *const dest, const void *const bytes, const size_t len) {
    uv_once(&libxml_init_once, &libxml_init);

    struct util_xml_errors errs = { 0 };

    if (!bytes || !len || len > INT_MAX) {
        xml_errors_add(&errs, xml_error(NULL, "invalid input"));
        return errs;
    }

    xmlDoc *const doc = xmlReadMemory((const char *) bytes, (int) len, NULL, NULL, 0);
    if (!doc) {
        xml_errors_add(&errs, xml_error_from_libxml(xmlGetLastError()));

        return errs;
    }

    errs = xml_validate_with_internal_schema(doc);
    if (errs.errors) {
        goto exit;
    }

    xmlNode *const root = xmlDocGetRootElement(doc);
    if (!root) {
        xml_errors_add(&errs, xml_error(NULL, "empty document"));
        goto exit;
    }

    const struct util_xml_error *const packet_err = xml_to_packet(dest, root);
    if (packet_err) {
        xml_errors_add(&errs, packet_err);
    }

exit:
    xmlFreeDoc(doc);

    return errs;
}
