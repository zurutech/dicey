#if !defined(HHQPUVHYDW_ERRORS_H)
#define HHQPUVHYDW_ERRORS_H

#include <errno.h>

#if defined (__cplusplus)
extern "C" {
#endif

// this enum uses same values as errno.h for comparable value. While the errno values are not defined by the C standard,
// they are defined by POSIX, Win32 and C++, and widely used. We use them here to make it easier to integrate with
// existing code.
enum dicey_error {
    DICEY_OK = 0,
    DICEY_EAGAIN    = -EAGAIN,
    DICEY_ENOMEM    = -ENOMEM,
    DICEY_EINVAL    = -EINVAL,
    DICEY_EBADMSG   = -EBADMSG,
    DICEY_EOVERFLOW = -EOVERFLOW,

    DICEY_EPATH_TOO_LONG = -0x1000,
    DICEY_ETUPLE_TOO_LONG = -0x1001,
    DICEY_EARRAY_TOO_LONG = -0x1002,
};

const char* dicey_strerror(int errnum);

#if defined(__cplusplus)
}
#endif


#endif // HHQPUVHYDW_ERRORS_H
