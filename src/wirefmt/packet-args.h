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

#if !defined(FWHTOKWVTU_PACKET_ARGS_H)
#define FWHTOKWVTU_PACKET_ARGS_H

#include <dicey/core/packet.h>

/**
 * @brief Duplicates a dicey_arg structure.
 *
 * This function creates a duplicate of the source dicey_arg structure and stores it in the destination dicey_arg
 * structure.
 *
 * @param dest Pointer to the destination dicey_arg structure. If NULL, a new dicey_arg structure will be allocated.
 * @param src Pointer to the source dicey_arg structure.
 * @return Pointer to the destination dicey_arg structure.
 */
struct dicey_arg *dicey_arg_dup(struct dicey_arg *dest, const struct dicey_arg *src);

void dicey_arg_free(const struct dicey_arg *arg);
void dicey_arg_free_contents(const struct dicey_arg *arg);

void dicey_arg_get_list(const struct dicey_arg *arg, const struct dicey_arg **list, const struct dicey_arg **end);

/**
 * @brief Moves the contents of one dicey_arg struct to another.
 * @note This function moves the contents of the source dicey_arg struct to the destination dicey_arg struct.
 *       The source dicey_arg struct will be zeroed out.
 *
 * @param dest Pointer to the destination dicey_arg struct. If NULL, a new dicey_arg struct will be allocated.
 * @param src Pointer to the source dicey_arg struct.
 * @return Pointer to the destination dicey_arg struct.
 */
struct dicey_arg *dicey_arg_move(struct dicey_arg *dest, struct dicey_arg *src);

#endif // FWHTOKWVTU_PACKET_ARGS_H
