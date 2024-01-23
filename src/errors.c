#include <dicey/errors.h>

const char* dicey_strerror(const int errnum) {
    switch (errnum) {
    default:
        return "unknown error";

    case DICEY_OK:
        return "success";

    case DICEY_EAGAIN:
        return "not enough data";

    case DICEY_ENOMEM:
        return "out of memory";

    case DICEY_EINVAL:
        return "invalid argument";

    case DICEY_EBADMSG:
        return "bad message";

    case DICEY_EOVERFLOW:
        return "overflow";

    case DICEY_EPATH_TOO_LONG:
        return "path too long";

    case DICEY_ETUPLE_TOO_LONG:
        return "tuple too long";

    case DICEY_EARRAY_TOO_LONG:
        return "array too long";

    case DICEY_EBUILDER_TYPE_MISMATCH:
        return "builder type mismatch";

    case DICEY_EVALUE_TYPE_MISMATCH:
        return "value type mismatch";

    case DICEY_ENOT_SUPPORTED:
        return "unsupported operation";
    }
}
