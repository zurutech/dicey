#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <util/dumper.h>

#define DEFAULT_PAD 4U

static bool is_ascii_printable(const char c) {
    return c >= ' ' && c <= '~';
}

void util_dumper_dump_hex(struct util_dumper *const dumper, const void *const data, const size_t size) {
    const uint8_t *const end = (uint8_t*) data + size;

    for (size_t i = 0U; i < size; ++i) {
        util_dumper_printf(dumper, "%02zu ", i);
    }

    puts("");

    for (const unsigned char *ptr = data; ptr < end; ++ptr) {
        util_dumper_printf(dumper, "%02X ", *ptr);
    }

    puts("");

    for (const unsigned char *ptr = data; ptr < end; ++ptr) {
        util_dumper_printf(dumper, "%c", is_ascii_printable(*ptr) ? *ptr : '.');

        // align the characters to the hex values above
        util_dumper_printf(dumper, "  ");
    }

    util_dumper_printf(dumper, "\n");
}

void util_dumper_indent(const struct util_dumper *const dumper) {
    for (unsigned i = 0U; i < dumper->pad; ++i) {
        fputc(' ', dumper->dest);
    }
}

void util_dumper_pad(struct util_dumper *const util_dumper) {
    util_dumper->pad += DEFAULT_PAD;
}

void util_dumper_printf(const struct util_dumper *const dumper, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(dumper->dest, fmt, args);

    va_end(args);
}

void util_dumper_printlnf(const struct util_dumper *const dumper, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    util_dumper_indent(dumper);
    vfprintf(dumper->dest, fmt, args);
    fputc('\n', dumper->dest);

    va_end(args);
}

void util_dumper_unpad(struct util_dumper *const dumper) {
    dumper->pad -= DEFAULT_PAD;
}
