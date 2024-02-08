// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/base64.h"

static void print_help(const char *const progname) {
    fprintf(stderr, "usage: %s [-d] [FILE]\n", progname);
}

int main(const int argc, const char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    bool              decode = false;
    const char       *fin = NULL;

    while (*++argv) {
        if (!strcmp(*argv, "-h")) {
            print_help(progname);
            return EXIT_SUCCESS;
        }

        if (!strcmp(*argv, "-d")) {
            decode = true;

            continue;
        }

        if (**argv == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", *argv);
            return EXIT_FAILURE;
        }

        if (fin) {
            fprintf(stderr, "error: multiple output files specified\n");
            return EXIT_FAILURE;
        }

        fin = *argv;
    }

    FILE *in = fin ? fopen(fin, decode ? "rb" : "r") : stdin;
    if (!in) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    uint8_t *dumped_bytes = NULL;
    size_t   nbytes = 0, bcap = 0;

    while (!feof(in)) {
        uint8_t      buf[4096];
        const size_t n = fread(buf, 1, sizeof buf, in);
        if (!n) {
            break;
        }

        const size_t new_len = nbytes + n;
        if (new_len > bcap) {
            bcap += sizeof buf;
            dumped_bytes = realloc(dumped_bytes, bcap);
            if (!dumped_bytes) {
                abort(); // this silences cppcheck, we don't care about safety in this dummy program
            }
        }

        memcpy(dumped_bytes + nbytes, buf, n);
        nbytes = new_len;
    }

    fclose(in);

    if (!nbytes || !dumped_bytes) {
        free(dumped_bytes);

        fputs("error: no input\n", stderr);

        return EXIT_FAILURE;
    }

    if (decode) {
        size_t   out_len = 0;
        uint8_t *decoded = util_base64_decode((const char *) dumped_bytes, nbytes, &out_len);

        free(dumped_bytes);

        if (!decoded) {
            fputs("error: base64_decode failed\n", stderr);
            return EXIT_FAILURE;
        }

        fwrite(decoded, 1, out_len, stdout);
        free(decoded);
    } else {
        size_t out_len = 0;
        char  *encoded = util_base64_encode(dumped_bytes, nbytes, &out_len);

        free(dumped_bytes);

        if (!encoded) {
            fputs("error: base64_encode failed\n", stderr);
            return EXIT_FAILURE;
        }

        fputs(encoded, stdout);
        free(encoded);
    }

    return EXIT_SUCCESS;
}
