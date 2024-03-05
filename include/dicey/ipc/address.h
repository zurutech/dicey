// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(BHSWUFULAM_ADDRESS_H)
#define BHSWUFULAM_ADDRESS_H

#include <stddef.h>

#include "dicey_export.h"

struct dicey_addr {
    const char *addr;
    size_t len;
};

DICEY_EXPORT struct dicey_addr dicey_addr_from_str(const char *str);

#endif // BHSWUFULAM_ADDRESS_H
