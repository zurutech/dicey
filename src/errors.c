// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <assert.h>
#include <stddef.h>

#include <dicey/errors.h>

// the errors conventionally use the first byte as an incremental index
#define INDEX_OF(E) ((ptrdiff_t) (-(E) &0xFF))
#define ERROR_INFO_FOR(E, NAME, MSG) [INDEX_OF(E)] = { .errnum = (E), .name = NAME, .message = MSG }

static const struct dicey_error_def error_info[] = {
    ERROR_INFO_FOR(DICEY_OK, "OK", "success"),
    ERROR_INFO_FOR(DICEY_EAGAIN, "TryAgain", "not enough data"),
    ERROR_INFO_FOR(DICEY_ENOMEM, "OutOfMemory", "out of memory"),
    ERROR_INFO_FOR(DICEY_EINVAL, "InvalidData", "invalid argument"),
    ERROR_INFO_FOR(DICEY_ENODATA, "NoDataAvailable", "no data available"),
    ERROR_INFO_FOR(DICEY_EBADMSG, "BadMessage", "bad message"),
    ERROR_INFO_FOR(DICEY_EOVERFLOW, "Overflow", "overflow"),
    ERROR_INFO_FOR(DICEY_EPATH_TOO_LONG, "PathTooLong", "path too long"),
    ERROR_INFO_FOR(DICEY_ETUPLE_TOO_LONG, "TupleTooLong", "tuple too long"),
    ERROR_INFO_FOR(DICEY_EARRAY_TOO_LONG, "ArrayTooLong", "array too long"),
    ERROR_INFO_FOR(DICEY_EVALUE_TYPE_MISMATCH, "ValueTypeMismatch", "value type mismatch"),
    ERROR_INFO_FOR(DICEY_ENOT_SUPPORTED, "NotSupported", "unsupported operation"),
};

const struct dicey_error_def *dicey_error_info(const enum dicey_error errnum) {
    return dicey_error_is_valid(errnum) ? &error_info[INDEX_OF(errnum)] : NULL;
}

void dicey_error_infos(const struct dicey_error_def **const defs, size_t *const count) {
    assert(defs && count);

    *defs = error_info;
    *count = sizeof error_info / sizeof *error_info;
}

bool dicey_error_is_valid(enum dicey_error errnum) {
    switch (errnum) {
    case DICEY_OK:
    case DICEY_EAGAIN:
    case DICEY_ENOMEM:
    case DICEY_EINVAL:
    case DICEY_ENODATA:
    case DICEY_EBADMSG:
    case DICEY_EOVERFLOW:
    case DICEY_EPATH_TOO_LONG:
    case DICEY_ETUPLE_TOO_LONG:
    case DICEY_EARRAY_TOO_LONG:
    case DICEY_EVALUE_TYPE_MISMATCH:
    case DICEY_ENOT_SUPPORTED:
        return true;

    default:
        return false;
    }
}

const char *dicey_error_name(const enum dicey_error errnum) {
    const struct dicey_error_def *const def = dicey_error_info(errnum);

    return def ? def->name : "UnknownError";
}

const char *dicey_error_msg(const enum dicey_error errnum) {
    const struct dicey_error_def *const def = dicey_error_info(errnum);

    return def ? def->message : "unknown error";
}
