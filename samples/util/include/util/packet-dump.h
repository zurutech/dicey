// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(BSRUIZGUZI_PACKET_DUMP_H)
#define BSRUIZGUZI_PACKET_DUMP_H

#include <dicey/dicey.h>

#include "dumper.h"

void util_dumper_dump_packet(struct util_dumper *dumper, struct dicey_packet packet);

#endif // BSRUIZGUZI_PACKET_DUMP_H
