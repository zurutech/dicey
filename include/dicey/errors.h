#if !defined(HHQPUVHYDW_ERRORS_H)
#define HHQPUVHYDW_ERRORS_H

#include "dicey_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum dicey_error {
    DICEY_OK = 0,
    
    DICEY_EAGAIN    = -0x000B,
    DICEY_ENOMEM    = -0x000C,
    DICEY_EINVAL    = -0x0016,
    DICEY_ENODATA   = -0x003D,
    DICEY_EBADMSG   = -0x004A,
    DICEY_EOVERFLOW = -0x004B,

    DICEY_EPATH_TOO_LONG  = -0x1000,
    DICEY_ETUPLE_TOO_LONG = -0x1001,
    DICEY_EARRAY_TOO_LONG = -0x1002,

    DICEY_EBUILDER_TYPE_MISMATCH = -0x2000,
    DICEY_EVALUE_TYPE_MISMATCH   = -0x2001,

    DICEY_ENOT_SUPPORTED = -0x3000,
};

DICEY_EXPORT const char* dicey_strerror(int errnum);

#if defined(__cplusplus)
}
#endif


#endif // HHQPUVHYDW_ERRORS_H
