// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <uv.h>

#include <dicey/core/errors.h>

enum dicey_error dicey_error_from_uv(const int uv_error) {
    switch (uv_error) {
    case 0:
        return DICEY_OK;

    case UV_EAGAIN:
        return DICEY_EAGAIN;

    case UV_ENOMEM:
        return DICEY_ENOMEM;

    case UV_EINVAL:
        return DICEY_EINVAL;

    case UV_ENODATA:
        return DICEY_ENODATA;

    case UV_EOVERFLOW:
        return DICEY_EOVERFLOW;

    case UV_ECONNREFUSED:
        return DICEY_ECONNREFUSED;

    default:
        return DICEY_EUV_UNKNOWN;
    }
}
