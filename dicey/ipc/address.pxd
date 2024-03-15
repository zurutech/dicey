from libc.stddef cimport size_t

from dicey.core cimport dicey_error

cdef extern from "dicey/dicey.h":
    cdef struct dicey_addr:
        const char *addr
        size_t len

    void dicey_addr_deinit(dicey_addr *addr)
    dicey_error dicey_addr_dup(dicey_addr *dest, dicey_addr src);
    const char *dicey_addr_from_str(dicey_addr *dest, const char *str)
    
cdef class Address:
    cdef dicey_addr _address

    cdef dicey_addr clone_raw(self)
    cdef dicey_addr leak(self)
