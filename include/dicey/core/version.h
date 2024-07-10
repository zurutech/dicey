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

#if !defined(SHAKEUMHSP_VERSION_H)
#define SHAKEUMHSP_VERSION_H

#include <stdint.h>

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Major version number of the Dicey protocol implemented by this library
 * @note  The Dicey protocol is identified by a major version number and a revision number.
 *        The major version number is incremented every time the protocol is changed.
 *        The revision number is incremented every time the protocol is patched without breaking compatibility or
 *        adding new features.
 */
#define DICEY_PROTO_MAJOR 1
#define DICEY_PROTO_REVISION 0
#define DICEY_PROTO_STRING #DICEY_PROTO_MAJOR "r" #DICEY_PROTO_REVISION

/**
 * @brief Structure representing the protocol version information in a "hello" packet.
 */
struct dicey_version {
    uint16_t major;    /**< Major version number */
    uint16_t revision; /**< Revision number */
};

#define DICEY_PROTO_VERSION_CURRENT                                                                                    \
    ((struct dicey_version) { .major = DICEY_PROTO_MAJOR, .revision = DICEY_PROTO_REVISION })

DICEY_EXPORT int dicey_version_cmp(struct dicey_version a, struct dicey_version b);

/**
 * @brief Major version number of the Dicey library
 * @note  The Dicey library is identified by a major version number, a minor version number and a patch number.
 *        The major version number is incremented every time the library ABI compatibility is broken
 *        The minor version number is incremented every time new features are added to the library without breaking
 * compatibility The patch number is incremented every time the library is patched for bug fixes without breaking
 * compatibility
 */
#define DICEY_LIB_VERSION_MAJOR 0
#define DICEY_LIB_VERSION_MINOR 3
#define DICEY_LIB_VERSION_PATCH 2
#define DICEY_LIB_VERSION_STRING #DICEY_LIB_VERSION_MAJOR "." #DICEY_LIB_VERSION_MINOR "." #DICEY_LIB_VERSION_PATCH

#define DICEY_LIB_VER_INT 0x00000302

#if defined(__cplusplus)
}
#endif

#endif // SHAKEUMHSP_VERSION_H
