// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(BHSWUFULAM_ADDRESS_H)
#define BHSWUFULAM_ADDRESS_H

#include <stddef.h>

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dicey_addr {
    const char *addr;
    size_t len;
};

DICEY_EXPORT void dicey_addr_deinit(struct dicey_addr *addr);
DICEY_EXPORT const char *dicey_addr_from_str(struct dicey_addr *dest, const char *str);

#if defined(__cplusplus)
}
#endif

#endif // BHSWUFULAM_ADDRESS_H
