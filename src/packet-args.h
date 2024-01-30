// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(FWHTOKWVTU_PACKET_ARGS_H)
#define FWHTOKWVTU_PACKET_ARGS_H

#include <dicey/packet.h>

struct dicey_arg *dicey_arg_dup(struct dicey_arg *dest, const struct dicey_arg *src);

void dicey_arg_free(const struct dicey_arg *arg);
void dicey_arg_free_contents(const struct dicey_arg *arg);

void dicey_arg_get_list(const struct dicey_arg *arg, const struct dicey_arg **list, const struct dicey_arg **end);

#endif // FWHTOKWVTU_PACKET_ARGS_H
