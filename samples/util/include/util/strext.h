// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(XVAUPCELRC_STREXT_H)
#define XVAUPCELRC_STREXT_H

#include <string.h>

#if defined(_MSC_VER) && __STDC_VERSION__ < 202311L

static inline char *strndup(const char *const str, const size_t maxlen) {
    const size_t len = strnlen(str, maxlen);

    char *const ret = malloc(len + 1);
    if (ret) {
        memcpy(ret, str, len);
        ret[len] = '\0';
    }

    return ret;
}

#endif // _MSC_VER && __STDC_VERSION__ < 202311L

#endif // XVAUPCELRC_STREXT_H
