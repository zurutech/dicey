// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include "dicey/errors.h"
#include "dicey/packet.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <dicey/dicey.h>

#include <util/dumper.h>
#include <util/packet-dump.h>

int LLVMFuzzerTestOneInput(const uint8_t *const data, size_t size) {
    struct dicey_packet packet = { 0 };

    size_t left = size;

    const enum dicey_error err = dicey_packet_load(&packet, &(const void *) { data }, &left);
    switch (err) {
    default:
        {
            const struct dicey_error_def *const def = dicey_error_info(err);
            if (def) {
                printf("Unexpected error: %s: %s (%d)\n", def->name, def->message, def->errnum);
            } else {
                printf("Unexpected error: %d\n", err);
            }

            assert(false);
        }

    case DICEY_OK:
        // {
        //     struct util_dumper dumper = util_dumper_for(stdout);

        //     const enum dicey_packet_kind packet_kind = dicey_packet_get_kind(packet);

        //     // bye and hello are uninteresting
        //     if (packet_kind == DICEY_PACKET_KIND_MESSAGE) {
        //         util_dumper_printlnf(&dumper, "Unlikely random message found of size %zu", size);

        //         util_dumper_dump_hex(&dumper, data, size - left);

        //         util_dumper_dump_packet(&dumper, packet);
        //     }

        //     break;
        // }

    case DICEY_EAGAIN:
    case DICEY_EBADMSG:
    case DICEY_EINVAL:
    case DICEY_EOVERFLOW:
        break;
    }

    dicey_packet_deinit(&packet);

    return 0;
}
