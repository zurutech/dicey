from ..core import OutOfMemoryError

from cpython cimport Py_buffer

from dicey.core cimport _check

from .address cimport dicey_addr, dicey_addr_from_str, dicey_addr_deinit, dicey_addr_dup

cdef class Address:
    def __init__(self, addrstr: str):
        bstr = addrstr.encode('utf-8')

        if not dicey_addr_from_str(&self._address, bstr):
            raise OutOfMemoryError()

    def __dealloc__(self):
        dicey_addr_deinit(&self._address)

    def __getbuffer__(self, Py_buffer *const buffer, const int flags):
        cdef Py_ssize_t size = self._address.len

        buffer.buf = <char*> self._address.addr
        buffer.itemsize = 1  
        buffer.len = size
        buffer.ndim = 1
        buffer.obj = self
        buffer.readonly = 1

    @property
    def _is_valid(self) -> bool:
        return self._address.addr and self._address.len

    @property
    def is_abstract(self) -> bool:
        return self._is_valid and self._address.addr[0] == 0

    def __bytes__(self) -> bytes:
        if self.is_abstract:
            return b'@' + self._address.addr[1:self._address.len]
        
        return self._address.addr if self._is_valid else b''

    def __str__(self):
        return bytes(self).decode('utf-8')

    def __repr__(self):
        return f'Address({str(self)!r})'

    cdef dicey_addr clone_raw(self):
        cdef dicey_addr addr_copy

        _check(dicey_addr_dup(&addr_copy, self._address))

        return addr_copy

    cdef dicey_addr leak(self):
        cdef dicey_addr addr = self._address

        self._address.addr = NULL
        self._address.len = 0

        return addr
