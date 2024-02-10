from libc.stddef cimport ptrdiff_t
from libc.stdint cimport int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t

cdef extern from "dicey/dicey.h":
    ctypedef uint8_t dicey_bool

    ctypedef uint8_t dicey_byte
    ctypedef int16_t dicey_i16
    ctypedef int32_t dicey_i32
    ctypedef int64_t dicey_i64
    ctypedef uint16_t dicey_u16
    ctypedef uint32_t dicey_u32
    ctypedef uint64_t dicey_u64

    cdef struct dicey_errmsg:
        uint16_t code
        const char *message

    ctypedef double dicey_float

    cdef struct dicey_selector:
        const char *trait
        const char *elem

    cdef bint dicey_selector_is_valid(dicey_selector selector)
    cdef ptrdiff_t dicey_selector_size(dicey_selector sel)

    ctypedef enum dicey_type:
        DICEY_TYPE_INVALID
        DICEY_TYPE_UNIT
        DICEY_TYPE_BOOL
        DICEY_TYPE_BYTE
        DICEY_TYPE_FLOAT
        DICEY_TYPE_INT16
        DICEY_TYPE_INT32
        DICEY_TYPE_INT64
        DICEY_TYPE_UINT16
        DICEY_TYPE_UINT32
        DICEY_TYPE_UINT64
        DICEY_TYPE_ARRAY
        DICEY_TYPE_TUPLE
        DICEY_TYPE_PAIR
        DICEY_TYPE_BYTES
        DICEY_TYPE_STR
        DICEY_TYPE_PATH
        DICEY_TYPE_SELECTOR
        DICEY_TYPE_ERROR

    cdef bint dicey_type_is_container(dicey_type type)
    cdef bint dicey_type_is_valid(dicey_type type)
    cdef const char *dicey_type_name(dicey_type type)
