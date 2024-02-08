// open source, not our code

/*
 * Base64 encoding/decoding (RFC1341)
 * Originally copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>, distributed under the terms of the BSD license.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <util/base64.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
char *util_base64_encode(const uint8_t *const src, const size_t len, size_t *const out_len) {
    if (!src) {
        return NULL;
    }

    size_t olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    olen += olen / 72;             /* line feeds */
    ++olen;                        /* nul termination */

    if (olen < len) {
        return NULL; /* integer overflow */
    }

    char *const out = malloc(olen);
    if (!out) {
        return NULL;
    }

    const uint8_t *const end = src + len, *in = src;
    char                *pos = out;

    int line_len = 0;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];

        in += 3;

        line_len += 4;
        if (line_len >= 72) {
            *pos++ = '\n';
            line_len = 0;
        }
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];

        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }

        *pos++ = '=';
        line_len += 4;
    }

    if (line_len) {
        *pos++ = '\n';
    }

    *pos = '\0';

    if (out_len) {
        *out_len = pos - out;
    }

    return out;
}

/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
uint8_t *util_base64_decode(const char *const src, const size_t len, size_t *const out_len) {
    if (!src) {
        return NULL;
    }

    int pad = 0;

    uint8_t dtable[256];
    memset(dtable, 0x80, sizeof dtable);

    for (size_t i = 0; i < sizeof base64_table - 1; ++i) {
        dtable[(ptrdiff_t) base64_table[i]] = (uint8_t) i;
    }

    dtable['='] = 0;

    size_t count = 0;
    for (size_t i = 0; i < len; ++i) {
        if (dtable[(ptrdiff_t) src[i]] != 0x80) {
            ++count;
        }
    }

    if (!count || count % 4) {
        return NULL;
    }

    size_t   olen = count / 4 * 3;
    uint8_t *out, *pos;
    pos = out = malloc(olen);

    if (!out) {
        return NULL;
    }

    count = 0;
    uint8_t block[4];
    for (size_t i = 0; i < len; ++i) {
        uint8_t tmp = dtable[(ptrdiff_t) src[i]];
        if (tmp == 0x80) {
            continue;
        }

        if (src[i] == '=') {
            ++pad;
        }

        block[count++] = tmp;

        if (count == 4) {
            *pos++ = (block[0] << 2) | (block[1] >> 4);
            *pos++ = (block[1] << 4) | (block[2] >> 2);
            *pos++ = (block[2] << 6) | block[3];

            count = 0;
            if (pad) {
                if (pad == 1) {
                    --pos;
                } else if (pad == 2) {
                    pos -= 2;
                } else {
                    /* Invalid padding */
                    free(out);
                    return NULL;
                }

                break;
            }
        }
    }

    *out_len = pos - out;

    return out;
}
