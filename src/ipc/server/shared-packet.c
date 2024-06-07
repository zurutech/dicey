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
#include <stdint.h>
#include <stdlib.h>

#include <dicey/core/packet.h>

#include "shared-packet.h"

struct dicey_shared_packet {
    size_t refc; // notice it's not atomic, so this is not thread safe
    struct dicey_packet packet;
};

struct dicey_shared_packet *dicey_shared_packet_from(const struct dicey_packet packet, const size_t starting_refcount) {
    assert(dicey_packet_is_valid(packet));

    struct dicey_shared_packet *const shared_packet = malloc(sizeof *shared_packet);
    if (shared_packet) {
        *shared_packet = (struct dicey_shared_packet) {
            .refc = starting_refcount,
            .packet = packet,
        };
    }

    return shared_packet;
}

/**
 * Borrows the shared packet as a packet. This DOES NOT increase the refcount.
 */
struct dicey_packet dicey_shared_packet_borrow(const struct dicey_shared_packet *const shared_packet) {
    assert(dicey_shared_packet_is_valid(shared_packet));

    return shared_packet->packet;
}

bool dicey_shared_packet_is_valid(const struct dicey_shared_packet *const shared_packet) {
    return shared_packet && shared_packet->refc > 0U && dicey_packet_is_valid(shared_packet->packet);
}

void dicey_shared_packet_ref(struct dicey_shared_packet *const shared_packet) {
    assert(dicey_shared_packet_is_valid(shared_packet));

    ++shared_packet->refc;
}

size_t dicey_shared_packet_size(const struct dicey_shared_packet *const shared_packet) {
    assert(dicey_shared_packet_is_valid(shared_packet));

    return shared_packet->packet.nbytes;
}

void dicey_shared_packet_unref(struct dicey_shared_packet *const shared_packet) {
    assert(dicey_shared_packet_is_valid(shared_packet));

    if (--shared_packet->refc == 0U) {
        dicey_packet_deinit(&shared_packet->packet);

        free(shared_packet);
    }
}
