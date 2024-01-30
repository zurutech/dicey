// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GQWOPCFNUH_UNSAFE_H)
#define GQWOPCFNUH_UNSAFE_H

#include <stddef.h>
#include <stdint.h>

#include <dicey/views.h>

#if defined(__cplusplus)
extern "C" {
#endif

void dunsafe_read_bytes(const struct dicey_view_mut dest, const void **src);
void dunsafe_write_bytes(void **dest, struct dicey_view view);

#if defined(__cplusplus)
}
#endif

#endif // GQWOPCFNUH_UNSAFE_H
