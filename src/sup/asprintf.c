// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "asprintf.h"

int asprintf(char **const dest, const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);

    const int ret = vasprintf(dest, fmt, args);

    va_end(args);

    return ret;
}

int vasprintf(char **const dest, const char *const fmt, va_list args) {
    va_list args2;
    va_copy(args2, args);

    const int len = vsnprintf(NULL, 0, fmt, args2);

    va_end(args2);

    // ugly nonsense due to MSVC disliking assignments in conditionals
    if (len >= 0) {
        *dest = malloc(len + 1U);

        if (dest) {
            return vsnprintf(*dest, len + 1U, fmt, args);
        }
    }

    return -1;
}
