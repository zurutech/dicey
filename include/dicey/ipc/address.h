// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(BHSWUFULAM_ADDRESS_H)
#define BHSWUFULAM_ADDRESS_H

#include <stddef.h>

#include "../core/errors.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents the address of either a Unix domain socket or a Windows named pipe.
 * @note  This structure is owning and must be deinitialised with dicey_addr_deinit().
 */
struct dicey_addr {
    const char *addr; /**< The address, which may or may not be null-terminated (i.e. in the case of abstract UDS). */
    size_t len;       /**< The length of the address, excluding any null terminator. */
};

/**
 * @brief Deinitialises a `dicey_addr`, releasing any resources it may own and resetting it to an empty state.
 */
DICEY_EXPORT void dicey_addr_deinit(struct dicey_addr *addr);

/**
 * @brief Duplicates an address.
 * @param dest The destination address.
 * @param src  The source address.
 * @return     Error code. Possible values are:
 *             - OK: the address was successfully duplicated to `dest`
 *             - ENOMEM: memory allocation failed
 */
DICEY_EXPORT enum dicey_error dicey_addr_dup(struct dicey_addr *dest, struct dicey_addr src);

/**
 * @brief Parses a string into an address.
 * @param dest The destination address. In case this function succeeds, the `addr` field will point to a newly allocated
 *             buffer containing the address, and the `len` field will be set to the length of the address. The buffer
 *             must be deallocated with dicey_addr_deinit().
 * @param str  The string to parse.
 * @return     The address, or NULL if the string could not be processed or in case of memory exhaustion. This is the
 *             same as the `addr` field of `dest`. As such, it may be null-terminated or not, so it's not advisable to
 *             use it as a C string.
 */
DICEY_EXPORT const char *dicey_addr_from_str(struct dicey_addr *dest, const char *str);

#if defined(__cplusplus)
}
#endif

#endif // BHSWUFULAM_ADDRESS_H
