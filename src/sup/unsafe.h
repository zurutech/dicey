// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GQWOPCFNUH_UNSAFE_H)
#define GQWOPCFNUH_UNSAFE_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/core/views.h>

#if defined(__cplusplus)
extern "C" {
#endif

// We assume that the current platform doesn't have a pathological implementation of pointers, aka zeroing their memory
// actually sets them to null. If this is not the case, somewhere else there are macros to stop the build
#define ZERO_ARRAY(BASE, LEN) memset((BASE), 0, sizeof *(BASE) * (LEN))

void dunsafe_read_bytes(const struct dicey_view_mut dest, const void **src);
void dunsafe_write_bytes(void **dest, struct dicey_view view);

#if defined(__cplusplus)
}
#endif

#endif // GQWOPCFNUH_UNSAFE_H
