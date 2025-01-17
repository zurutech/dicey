/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
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

#if !defined(KTMJWIYSDS_VALUE_INTERNAL_H)
#define KTMJWIYSDS_VALUE_INTERNAL_H

#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/value.h>

/**
 * @brief Creates an owning value pointing at a given value nested inside the message contained in the packet.
 * @note  This function performs no checks to validate that wanted_bit and owner are related; this makes it pretty
 *        unsafe for obvious reasons. `wanted_bit` must come from a value inside `owner` packet, like the root value, a
 *        tuple element, ...
 */
void dicey_owning_value_from_parts(
    struct dicey_owning_value *dest,
    struct dicey_packet owner,
    struct dicey_value *wanted_bit
);

#endif // KTMJWIYSDS_VALUE_INTERNAL_H
