// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/packet-dump.h"
#include "util/packet-json.h"

enum file_probe_result {
    FILE_PROBE_FAIL,

    FILE_PROBE_BINARY,
    FILE_PROBE_PROBABLY_TEXT,
};

static enum file_probe_result file_probe(const char *const path) {
    char buf[4096];

    FILE *const file = fopen(path, "rb");
    if (!file) {
        return FILE_PROBE_FAIL;
    }

    const size_t n = fread(buf, 1, sizeof buf, file);
    fclose(file);

    if (!n) {
        return FILE_PROBE_FAIL;
    }

    const char *const end = buf + n;
    for (const char *p = buf; p < end; ++p) {
        if (!isprint(*p) && !isspace(*p)) {
            return FILE_PROBE_BINARY;
        }
    }

    return FILE_PROBE_PROBABLY_TEXT;
}

static void print_help(const char *const progname) {
    fprintf(stderr, "usage: %s [-bj] [FILE]\n", progname);
}

int main(const int argc, const char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    bool              text_mode = false;
    const char       *fin = NULL;

    while (*++argv) {
        if (!strcmp(*argv, "-h")) {
            print_help(progname);
            return EXIT_SUCCESS;
        }

        if (!strcmp(*argv, "-b") || !strcmp(*argv, "-j")) {
            text_mode = (*argv)[1] == 'j';
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

    FILE *in = stdin;

    if (fin) {
        const enum file_probe_result mode = file_probe(fin);
        if (mode == FILE_PROBE_FAIL) {
            perror("fopen");
            return EXIT_FAILURE;
        }

        in = fopen(fin, mode == FILE_PROBE_BINARY ? "rb" : "r");
        if (!in) {
            perror("fopen");
            return EXIT_FAILURE;
        }

        text_mode = mode == FILE_PROBE_PROBABLY_TEXT;
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

    if (!nbytes || !dumped_bytes) {
        fputs("error: no input\n", stderr);
        return EXIT_FAILURE;
    }

    struct dicey_packet    pkt = { 0 };
    const enum dicey_error err = text_mode
                                     ? util_json_to_dicey(&pkt, dumped_bytes, nbytes)
                                     : dicey_packet_load(&pkt, &(const void *) { dumped_bytes }, &(size_t) { nbytes });
    if (err) {
        goto quit;
    }

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_dump_packet(&dumper, pkt);

quit:
    dicey_packet_deinit(&pkt);
    free(dumped_bytes);
    fclose(in);

    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
