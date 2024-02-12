// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ZAKDPHUQGZ_PACKET_XML_H)
#define ZAKDPHUQGZ_PACKET_XML_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/dicey.h>

// Note: MSVC supports C11 now, but they clearly forgot to unflag flexible array members as an extension
// This pragma is here to teach MSVC to know its place
#if defined(_MSC_VER)
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
