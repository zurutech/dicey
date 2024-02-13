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
 * @brief Structure representing the protocol version information in a "hello" packet.
 */
struct dicey_version {
    uint16_t major;    /**< Major version number */
    uint16_t revision; /**< Revision number */
};

#define DICEY_PROTO_VERSION_CURRENT                                                                                    \
    ((struct dicey_version) { .major = DICEY_PROTO_MAJOR, .revision = DICEY_PROTO_REVISION })

/**
 * @brief Major version number of the Dicey library
 * @note  The Dicey library is identified by a major version number, a minor version number and a patch number.
 *        The major version number is incremented every time the library ABI compatibility is broken
 *        The minor version number is incremented every time new features are added to the library without breaking
 * compatibility The patch number is incremented every time the library is patched for bug fixes without breaking
 * compatibility
 */
#define DICEY_LIB_VERSION_MAJOR 0
#define DICEY_LIB_VERSION_MINOR 0
#define DICEY_LIB_VERSION_PATCH 1
#define DICEY_LIB_VERSION_STRING #DICEY_LIB_VERSION_MAJOR "." #DICEY_LIB_VERSION_MINOR "." #DICEY_LIB_VERSION_PATCH

#define DICEY_LIB_VER_INT 0x00000001

#if defined(__cplusplus)
}
#endif

#endif // SHAKEUMHSP_VERSION_H
