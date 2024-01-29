#if !defined(HHQPUVHYDW_ERRORS_H)
#define HHQPUVHYDW_ERRORS_H

#include <stddef.h>

#include "dicey_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum dicey_error {
    DICEY_OK = 0x0000,
    
    DICEY_EAGAIN    = -0x0101,
    DICEY_ENOMEM    = -0x0102,
    DICEY_EINVAL    = -0x0103,
    DICEY_ENODATA   = -0x0104,
    DICEY_EBADMSG   = -0x0105,
    DICEY_EOVERFLOW = -0x0106,

    DICEY_EPATH_TOO_LONG  = -0x0207,
    DICEY_ETUPLE_TOO_LONG = -0x0208,
    DICEY_EARRAY_TOO_LONG = -0x0209,

    DICEY_EBUILDER_TYPE_MISMATCH = -0x030A,
    DICEY_EVALUE_TYPE_MISMATCH   = -0x030B,

    DICEY_ENOT_SUPPORTED = -0x040C,
};

struct dicey_error_def {
    enum dicey_error errnum;
    const char *name;
    const char *message;
};

DICEY_EXPORT const struct dicey_error_def* dicey_error_info(enum dicey_error errnum);
DICEY_EXPORT void dicey_error_infos(const struct dicey_error_def **defs, size_t *count);
DICEY_EXPORT const char* dicey_error_msg(enum dicey_error errnum);
DICEY_EXPORT const char* dicey_error_name(enum dicey_error errnum);

#if defined(__cplusplus)
}
#endif


#endif // HHQPUVHYDW_ERRORS_H
