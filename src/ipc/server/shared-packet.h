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

#if !defined(AJGCFSQAPA_SHARED_PACKET_H)
#define AJGCFSQAPA_SHARED_PACKET_H

#include <stdbool.h>
#include <stddef.h>

#include <dicey/core/packet.h>

/**
 * shared_packet is a structure that represents a refcounted packet. This is useful when doing multisends (like in
 * signals) This is in general a suboptimal solution, but it's probably enough to solve most of the issues we have.
 * shared_packet is not thread safe, and it's only meant to be used in dicey_server (which is single threaded)
 */

struct dicey_shared_packet;

struct dicey_shared_packet *dicey_shared_packet_from(struct dicey_packet packet, size_t starting_refcount);

/**
 * Borrows the shared packet as a packet. This DOES NOT increase the refcount.
 * Do not free the packet, as it's owned by the shared_packet.
 */
struct dicey_packet dicey_shared_packet_borrow(const struct dicey_shared_packet *shared_packet);

bool dicey_shared_packet_is_valid(const struct dicey_shared_packet *shared_packet);

void dicey_shared_packet_ref(struct dicey_shared_packet *shared_packet);

size_t dicey_shared_packet_size(const struct dicey_shared_packet *shared_packet);

void dicey_shared_packet_unref(struct dicey_shared_packet *shared_packet);

#endif // AJGCFSQAPA_SHARED_PACKET_H
