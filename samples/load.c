#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/packet-dump.h"

int main(void) {
    uint8_t *dumped_bytes = NULL;
    size_t nbytes = 0, bcap = 0;

    while (!feof(stdin)) {
        uint8_t buf[4096];
        const size_t n = fread(buf, 1, sizeof buf, stdin);
        if (!n) {
            break;
        }

        const size_t new_len = nbytes + n;
        if (new_len > bcap) {
            bcap += sizeof buf;
            dumped_bytes = realloc(dumped_bytes, bcap);
            assert(dumped_bytes);
        }

        memcpy(dumped_bytes + nbytes, buf, n);
        nbytes = new_len;
    }

    struct dicey_packet pkt = {0};
    const enum dicey_error err = dicey_packet_load(&pkt, &(const void*) { dumped_bytes }, &(size_t) { nbytes });
    if (err) {
        goto quit;
    }

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_dump_packet(&dumper, pkt);

quit:
    dicey_packet_deinit(&pkt);
    free(dumped_bytes);

    if (err) {
        fprintf(stderr, "error: %s\n", dicey_strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
