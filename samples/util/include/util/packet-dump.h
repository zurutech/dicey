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

#if !defined(BSRUIZGUZI_PACKET_DUMP_H)
#define BSRUIZGUZI_PACKET_DUMP_H

#include <dicey/dicey.h>

#include "dumper.h"

void util_dumper_dump_packet(struct util_dumper *dumper, struct dicey_packet packet);
void util_dumper_dump_value(struct util_dumper *dumper, const struct dicey_value *value);

#endif // BSRUIZGUZI_PACKET_DUMP_H
