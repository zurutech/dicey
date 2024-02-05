// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(SHAKEUMHSP_VERSION_H)
#define SHAKEUMHSP_VERSION_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define DICEY_VERSION_MAJOR 0
#define DICEY_VERSION_REVISION 1
#define DICEY_VERSION_STRING #DICEY_VERSION_MAJOR "r" #DICEY_VERSION_REVISION

/**
 * @brief Structure representing the version information in a "hello" packet.
 */
struct dicey_version {
    uint16_t major;    /**< Major version number */
    uint16_t revision; /**< Revision number */
};

#define DICEY_VERSION_CURRENT                                                                                          \
    ((struct dicey_version) { .major = DICEY_VERSION_MAJOR, .revision = DICEY_VERSION_REVISION })

#if defined(__cplusplus)
}
#endif

#endif // SHAKEUMHSP_VERSION_H
