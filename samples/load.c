// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/packet-dump.h"

int main(const int argc, const char *const *const argv) {
    FILE *const in = argc > 1 ? fopen(argv[1], "rb") : stdin;

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
    const enum dicey_error err = dicey_packet_load(&pkt, &(const void *) { dumped_bytes }, &(size_t) { nbytes });
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
