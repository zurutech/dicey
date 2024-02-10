from libc.stddef cimport ptrdiff_t
from libc.stdint cimport int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, uint64_t

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
        DICEY_TYPE_INVALID = 0,
        DICEY_TYPE_UNIT = ord('$'),
        DICEY_TYPE_BOOL = ord('b'),
        DICEY_TYPE_BYTE = ord('c'),
        DICEY_TYPE_FLOAT = ord('f'),
        DICEY_TYPE_INT16 = ord('n'),
        DICEY_TYPE_INT32 = ord('i'),
        DICEY_TYPE_INT64 = ord('x'),
        DICEY_TYPE_UINT16 = ord('q'),
        DICEY_TYPE_UINT32 = ord('u'),
        DICEY_TYPE_UINT64 = ord('t'),
        DICEY_TYPE_ARRAY = ord('['),
        DICEY_TYPE_TUPLE = ord('('),
        DICEY_TYPE_PAIR = ord('{'),
        DICEY_TYPE_BYTES = ord('y'),
        DICEY_TYPE_STR = ord('s'),
        DICEY_TYPE_PATH = ord('@'),
        DICEY_TYPE_SELECTOR = ord('%'),
        DICEY_TYPE_ERROR = ord('e')

    cdef bint dicey_type_is_container(dicey_type type)
    cdef bint dicey_type_is_valid(dicey_type type)
    cdef const char *dicey_type_name(dicey_type type)
