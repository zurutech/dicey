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
#include <stdbool.h>
#include <stddef.h>

#include <complex.h>
#include <libxml/tree.h>

#include <dicey/core/errors.h>
#include <dicey/core/hashset.h>
#include <dicey/core/hashtable.h>
#include <dicey/core/packet.h>
#include <dicey/ipc/registry.h>
#include <dicey/ipc/traits.h>

#include "ipc/server/registry-internal.h"
#include "sup/trace.h"
#include "sup/util.h"

#include "introspection-internal.h"

#define NAME_ATTR BAD_CAST "name"
#define OBJECT_ELEM BAD_CAST "object"
#define OPERATION_ELEM BAD_CAST "operation"
#define PATH_ATTR BAD_CAST "path"
#define PROPERTY_ELEM BAD_CAST "property"
#define READ_ONLY_ATT BAD_CAST "read-only"
#define SIGNAL_ELEM BAD_CAST "signal"
#define SIGNATURE_ATTR BAD_CAST "signature"
#define TRAIT_ELEM BAD_CAST "trait"
#define XML_MODEL_PI BAD_CAST "xml-model"

#define XML_MODEL_XSD_CONTENT                                                                                          \
    BAD_CAST "href=href=\"object.xsd\" "                                                                               \
             "type=\"application/xml\" "                                                                               \
             "schematypens=\"http://www.w3.org/2001/XMLSchema\""

static enum dicey_error elems_dump_xml(const struct dicey_hashtable *const elems, xmlNode *const tnode) {
    assert(elems && tnode);

    struct dicey_hashtable_iter iter = dicey_hashtable_iter_start(elems);

    const char *elem_name = NULL;
    void *elem_ptr = NULL;
    while (dicey_hashtable_iter_next(&iter, &elem_name, &elem_ptr)) {
        const struct dicey_element *const elem = elem_ptr;

        const xmlChar *ename = NULL;

        switch (elem->type) {
        case DICEY_ELEMENT_TYPE_OPERATION:
            ename = OPERATION_ELEM;
            break;

        case DICEY_ELEMENT_TYPE_PROPERTY:
            ename = PROPERTY_ELEM;
            break;

        case DICEY_ELEMENT_TYPE_SIGNAL:
            ename = SIGNAL_ELEM;
            break;

        default:
            assert(false); // malformed trait
            break;
        }

        xmlNode *const enode = xmlNewChild(tnode, NULL, ename, NULL);
        if (!enode) {
            return TRACE(DICEY_ENOMEM);
        }

        if (!xmlNewProp(enode, NAME_ATTR, BAD_CAST elem_name)) {
            return TRACE(DICEY_ENOMEM);
        }

        if (!xmlNewProp(enode, SIGNATURE_ATTR, BAD_CAST elem->signature)) {
            return TRACE(DICEY_ENOMEM);
        }

        if (elem->type == DICEY_ELEMENT_TYPE_PROPERTY && elem->flags & DICEY_ELEMENT_READONLY) {
            if (!xmlNewProp(enode, READ_ONLY_ATT, BAD_CAST "true")) {
                return TRACE(DICEY_ENOMEM);
            }
        }
    }

    return DICEY_OK;
}

static enum dicey_error object_dump_xml(
    const struct dicey_registry *const registry,
    const char *const path,
    struct dicey_object *const obj,
    xmlChar **const dest
) {
    xmlDoc *const doc = xmlNewDoc(BAD_CAST "1.0");
    if (!doc) {
        return TRACE(DICEY_ENOMEM);
    }

    xmlNode *const obj_node = xmlNewNode(NULL, OBJECT_ELEM);
    if (!obj_node) {
        xmlFreeDoc(doc);
        return TRACE(DICEY_ENOMEM);
    }

    DICEY_UNUSED(xmlDocSetRootElement(doc, obj_node));

    xmlNode *const docPI = xmlNewDocPI(doc, XML_MODEL_PI, XML_MODEL_XSD_CONTENT);
    if (!docPI) {
        xmlFreeDoc(doc);

        return TRACE(DICEY_ENOMEM);
    }

    // for some arcane reason you still need to attach the PI to the doc using some hacky APIs... OK I guess?
    if (!xmlAddPrevSibling(obj_node, docPI)) {
        xmlFreeDoc(doc);

        return TRACE(DICEY_ENOMEM);
    }

    if (!xmlNewProp(obj_node, PATH_ATTR, BAD_CAST path)) {
        xmlFreeDoc(doc);
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_hashset_iter iter = dicey_hashset_iter_start(obj->traits);

    const char *trait_name = NULL;
    while (dicey_hashset_iter_next(&iter, &trait_name)) {
        const struct dicey_trait *const trait = dicey_registry_get_trait(registry, trait_name);
        assert(trait); // malformed object

        xmlNode *const tnode = xmlNewChild(obj_node, NULL, TRAIT_ELEM, NULL);
        if (!tnode) {
            xmlFreeDoc(doc);

            return TRACE(DICEY_ENOMEM);
        }

        if (!xmlNewProp(tnode, NAME_ATTR, BAD_CAST trait_name)) {
            xmlFreeDoc(doc);

            return TRACE(DICEY_ENOMEM);
        }

        const enum dicey_error err = elems_dump_xml(trait->elems, tnode);
        if (err) {
            xmlFreeDoc(doc);

            return TRACE(err);
        }
    }

    xmlDocDumpFormatMemory(doc, dest, NULL, 0);
    assert(*dest);

    xmlFreeDoc(doc);

    return DICEY_OK;
}

enum dicey_error introspection_object_populate_xml(
    const struct dicey_registry *const registry,
    const char *const path,
    struct dicey_object *const obj,
    const xmlChar **const dest
) {
    assert(path && obj && dest);

    if (!obj->cached_xml) {
        xmlChar *xml = NULL;
        const enum dicey_error err = object_dump_xml(registry, path, obj, &xml);
        if (err) {
            return err;
        }

        obj->cached_xml = xml;
    }

    *dest = obj->cached_xml;

    return DICEY_OK;
}
