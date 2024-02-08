// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(SHAKEUMHSP_VERSION_H)
#define SHAKEUMHSP_VERSION_H

#include <stdint.h>

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
#define DICEY_PROTO_MAJOR 0
#define DICEY_PROTO_REVISION 1
#define DICEY_PROTO_STRING #DICEY_PROTO_MAJOR "r" #DICEY_PROTO_REVISION

/**
 * @brief Structure representing the version information in a "hello" packet.
 */
struct dicey_version {
    uint16_t major;    /**< Major version number */
    uint16_t revision; /**< Revision number */
};

#define DICEY_VERSION_CURRENT ((struct dicey_version) { .major = DICEY_PROTO_MAJOR, .revision = DICEY_PROTO_REVISION })

#define DICEY_LIB_VERSION_MAJOR 0
#define DICEY_LIB_VERSION_MINOR 0
#define DICEY_LIB_VERSION_REVISION 1

#if defined(__cplusplus)
}
#endif

#endif // SHAKEUMHSP_VERSION_H
