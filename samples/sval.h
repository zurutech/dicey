// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(OCVMKCPQZY_SVAL_H)
#define OCVMKCPQZY_SVAL_H

#include <dicey/dicey.h>

#define SVAL_PATH "/sval"
#define SVAL_TRAIT "sval.Sval"
#define SVAL_PROP "Value"
#define SVAL_SEL                                                                                                       \
    (struct dicey_selector) { .trait = SVAL_TRAIT, .elem = SVAL_PROP }
#define SVAL_SIG "s"

#endif // OCVMKCPQZY_SVAL_H
