// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <uv.h>

#include <dicey/core/errors.h>

#include "sup/trace.h"

enum dicey_error dicey_error_from_uv(const int uv_error) {
    switch (uv_error) {
    case 0:
        return DICEY_OK;

    case UV_EAGAIN:
        return TRACE(DICEY_EAGAIN);

    case UV_ENOENT:
        return TRACE(DICEY_ENOENT);

    case UV_ENOMEM:
        return TRACE(DICEY_ENOMEM);

    case UV_EINVAL:
        return TRACE(DICEY_EINVAL);

    case UV_EPIPE:
        return TRACE(DICEY_EPIPE);

    case UV_ENODATA:
        return TRACE(DICEY_ENODATA);

    case UV_EOVERFLOW:
        return TRACE(DICEY_EOVERFLOW);

    case UV_ECONNRESET:
        return TRACE(DICEY_ECONNRESET);

    case UV_ETIMEDOUT:
        return TRACE(DICEY_ETIMEDOUT);

    case UV_ECONNREFUSED:
        return TRACE(DICEY_ECONNREFUSED);

    case UV_EADDRINUSE:
        return TRACE(DICEY_EADDRINUSE);

    default:
        return TRACE(DICEY_EUV_UNKNOWN);
    }
}
