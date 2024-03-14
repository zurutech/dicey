// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <dicey/core/version.h>

#define CMP(A, B) ((int) (((A) < (B)) - ((A) > (B))))

int dicey_version_cmp(const struct dicey_version a, const struct dicey_version b) {
    const int res = CMP(a.major, b.major);

    return res ? res : CMP(a.revision, b.revision);
}
