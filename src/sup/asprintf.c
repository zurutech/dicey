// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **const s, const char *const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    const int ret = vasprintf(s, fmt, ap);

    va_end(ap);

    return ret;
}

int vasprintf(char **const s, const char *const fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    const int l = vsnprintf(NULL, 0, fmt, ap2);

    va_end(ap2);

    if (l < 0 || !(*s = malloc(l + 1U))) {
        return -1;
    }

    return vsnprintf(*s, l + 1U, fmt, ap);
}
