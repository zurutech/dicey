from libc.stdint cimport uint16_t

cdef extern from "dicey/dicey.h":
    cdef struct dicey_version:
        uint16_t major
        uint16_t revision

    cdef enum:
        DICEY_PROTO_MAJOR
        DICEY_PROTO_REVISION
