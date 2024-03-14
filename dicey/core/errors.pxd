from libc.stddef cimport size_t

cdef extern from "dicey/dicey.h":
    cdef enum dicey_error:
        pass

    cdef struct dicey_error_def:
        dicey_error errnum
        const char *name
        const char *message

    void dicey_error_infos(const dicey_error_def **defs, size_t *count)

cdef void _check(const dicey_error error)