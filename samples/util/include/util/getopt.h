// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(MIFVOFWBUL_GETOPT_WIN_H)
#define MIFVOFWBUL_GETOPT_WIN_H

#if defined(_WIN32)

int getopt(const int nargc, char *const nargv[], const char *const ostr);

extern char *optarg;
extern int optind, opterr, optopt;
extern int optreset;

#else

#include <unistd.h>

#endif

#endif // MIFVOFWBUL_GETOPT_WIN_H
