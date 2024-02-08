// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(QVHUCEDPLP_BASE64_H)
#define QVHUCEDPLP_BASE64_H

#include <stddef.h>
#include <stdint.h>

uint8_t *util_base64_decode(const char *src, size_t len, size_t *out_len);
char    *util_base64_encode(const uint8_t *src, size_t len, size_t *out_len);

#endif // QVHUCEDPLP_BASE64_H
