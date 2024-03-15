from dataclasses import dataclass as _dataclass

from libc.stdint cimport INT16_MIN, INT16_MAX, INT32_MIN, INT32_MAX, INT64_MIN, INT64_MAX, \
                         UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX

from .errors import DiceyError

@_dataclass(frozen=True)
class Array:
    type: type
    values: _Iterable

@_dataclass(frozen=True)
class Byte:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not 0 <= self.value <= UINT8_MAX:
            raise ValueError("byte value out of range")

@_dataclass(frozen=True)
class ErrorMessage(DiceyError):
    """An error message is an error that can be sent or received via the Dicey protocol"""
    # 16-bit error code
    code: int
    message: str

    def __post_init__(self):
        if not 0 <= self.code <= UINT16_MAX:
            raise ValueError("code must be a 16-bit unsigned integer")

@_dataclass(frozen=True)
class Int16:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not INT16_MIN <= self.value <= INT16_MAX:
            raise ValueError("int16 value out of range")

@_dataclass(frozen=True)
class Int32:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not INT32_MIN <= self.value <= INT32_MAX:
            raise ValueError("int32 value out of range")

@_dataclass(frozen=True)
class Int64:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not INT64_MIN <= self.value <= INT64_MAX:
            raise ValueError("int64 value out of range")

@_dataclass(frozen=True)
class Pair:
    first: object
    second: object

@_dataclass(frozen=True)
class Path:
    """A path is an ASCII string that identifies an object residing on the server"""
    value: str

    def __post_init__(self):
        if not self.value:
            raise ValueError("paths can't be empty")

        if not self.value.isascii():
            raise ValueError("paths must be ASCII strings")

        # TODO: validate path format further

    def __str__(self):
        return self.value

@_dataclass(frozen=True)
class Selector:
    """A selector is a pair of two ASCII trings identifying a specific element in a given trait"""
    trait: str
    elem: str

    def __str__(self):
        return f"{self.trait}:{self.elem}"

@_dataclass(frozen=True)
class UInt16:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not 0 <= self.value <= UINT16_MAX:
            raise ValueError("uint16 value out of range")

@_dataclass(frozen=True)
class UInt32:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not 0 <= self.value <= UINT32_MAX:
            raise ValueError("uint32 value out of range")

@_dataclass(frozen=True)
class UInt64:
    value: int

    def __int__(self):
        return self.value

    def __post_init__(self):
        if not 0 <= self.value <= UINT64_MAX:
            raise ValueError("uint64 value out of range")