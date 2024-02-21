// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(RSPVKYOAEK_TRACE_H)
#define RSPVKYOAEK_TRACE_H

#if defined(NDEBUG)
#define TRACE(X) X
#else
#include <dicey/core/errors.h>

enum dicey_error _trace_err(enum dicey_error errnum);

#define TRACE(X) _trace_err(X)
#endif

#endif // RSPVKYOAEK_TRACE_H
