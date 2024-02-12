// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <dicey/dicey.h>

#include <util/dumper.h>

#define DEFAULT_PAD 4U

#define HEX_LINE_BYTES 16U
#define HEX_GROUP_BYTES 2U

static bool is_ascii_printable(const int c) {
    return c >= ' ' && c <= '~';
}

static void dumper_vprintf(struct util_dumper *const dumper, const char *const fmt, va_list args) {
    if (dumper->newline) {
        util_dumper_indent(dumper);
        dumper->newline = false;
    }

    vfprintf(dumper->dest, fmt, args);
}

static bool next_chunk(struct dicey_view *const from, struct dicey_view *const chunk, const size_t chunk_size) {
    if (from->len == 0U) {
        return false;
    }

    *chunk = (struct dicey_view) {
        .data = from->data,
        .len = from->len < chunk_size ? from->len : chunk_size,
    };

    *from = (struct dicey_view) {
        .data = (const uint8_t *) from->data + chunk->len,
        .len = from->len - chunk->len,
    };

    return true;
}

void util_dumper_dump_hex(struct util_dumper *const dumper, const void *const data, const size_t size) {
    struct dicey_view left = { .data = data, .len = size }, chunk = { 0 };

    uintptr_t i = 0U;

    while (next_chunk(&left, &chunk, HEX_LINE_BYTES)) {
        const uint8_t *const bytes = chunk.data;
        const size_t         n = chunk.len;

        for (size_t j = 0U; j < HEX_LINE_BYTES; ++j) {
            const bool line_start = j % HEX_LINE_BYTES == 0U;
            if (line_start) {
                util_dumper_printf(dumper, "%08" PRIxPTR ":", i);
            }

            if (j % HEX_GROUP_BYTES == 0U) {
                util_dumper_printf(dumper, " ");
            }

            if (j < n) {
                util_dumper_printf(dumper, "%02" PRIx8, bytes[j]);
            } else {
                util_dumper_printf(dumper, "  ");
            }
        }

        util_dumper_printf(dumper, "  ");

        for (uintptr_t j = 0U; j < HEX_LINE_BYTES; ++j) {
            int byte = ' ';

            if (j < n) {
                byte = (int) bytes[j];
            }

            util_dumper_printf(dumper, "%c", is_ascii_printable(byte) ? byte : '.');
        }

        util_dumper_newline(dumper);

        i += HEX_LINE_BYTES;
    }
}

void util_dumper_indent(const struct util_dumper *const dumper) {
    for (unsigned i = 0U; i < dumper->pad; ++i) {
        fputc(' ', dumper->dest);
    }
}

void util_dumper_pad(struct util_dumper *const util_dumper) {
    util_dumper->pad += DEFAULT_PAD;
}

void util_dumper_printf(struct util_dumper *const dumper, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    dumper_vprintf(dumper, fmt, args);

    va_end(args);
}

void util_dumper_printlnf(struct util_dumper *const dumper, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    dumper_vprintf(dumper, fmt, args);

    va_end(args);

    util_dumper_newline(dumper);
}

void util_dumper_newline(struct util_dumper *const dumper) {
    fputc('\n', dumper->dest);
    dumper->newline = true;
}

void util_dumper_reset_pad(struct util_dumper *const dumper) {
    dumper->pad = 0U;
}

void util_dumper_unpad(struct util_dumper *const dumper) {
    dumper->pad -= DEFAULT_PAD;
}

// unused in code, but useful when run from the GDB/LLDB shell
void quick_debug_dump(const void *const data, const size_t size) {
    struct util_dumper dumper = {
        .dest = stdout,
        .pad = 0U,
    };

    util_dumper_dump_hex(&dumper, data, size);
}
