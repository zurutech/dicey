/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
