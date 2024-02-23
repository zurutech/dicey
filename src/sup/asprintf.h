// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(HFKNEIUTND_ASPRINTF_H)
#define HFKNEIUTND_ASPRINTF_H

#if defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#define _GNU_SOURCE
#include <stdio.h>
#else
#include <stdarg.h>

int asprintf(char **const s, const char *const fmt, ...);
int vasprintf(char **const s, const char *const fmt, va_list ap);
#endif

#endif // HFKNEIUTND_ASPRINTF_H
