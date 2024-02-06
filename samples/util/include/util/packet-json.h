// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(WYABUAXOJS_PACKET_JSON_H)
#define WYABUAXOJS_PACKET_JSON_H

#include <dicey/dicey.h>

enum dicey_error util_json_to_dicey(struct dicey_packet *dest, const void *bytes, size_t len);

#endif // WYABUAXOJS_PACKET_JSON_H
