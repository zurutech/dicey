// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(BCSTIODIIH_ASSUME_H)
#define BCSTIODIIH_ASSUME_H

#include <assert.h>

#include <dicey/core/errors.h>

#define DICEY_ASSUME(EXPR)                                                                                             \
    do {                                                                                                               \
        const enum dicey_error err = (EXPR);                                                                           \
        assert(err == DICEY_OK);                                                                                       \
        (void) err;                                                                                                    \
    } while (0)

#endif // BCSTIODIIH_ASSUME_H
