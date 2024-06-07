# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from dataclasses import dataclass as _dataclass
from typing import Any as _Any, Iterable as _Iterable, Optional as _Optional

from libc.stdint cimport INT16_MIN, INT16_MAX, INT32_MIN, INT32_MAX, INT64_MIN, INT64_MAX, \
                         UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX

from .errors import DiceyError

@_dataclass(frozen=True)
class Array:
    type: type
    values: _Iterable

    def __iter__(self) -> _Iterable:
        return iter(self.values)

    def __len__(self) -> int:
        return len(self.values)

    def __getitem__(self, index: int) -> _Any:
        return self.values[index]

@_dataclass(frozen=True, init=False, repr=False)
class Byte:
    value: int 

    def __init__(self, value: int | str):
        if isinstance(value, str):
            if len(value) != 1 or not value.isascii():
                raise ValueError("byte only accepts uint8 or ASCII characters")

            value = ord(value)

        elif not 0 <= value <= UINT8_MAX:
            raise ValueError("byte value out of range")

        object.__setattr__(self, 'value', int(value))

    def __int__(self):
        return self.value

    def __str__(self):
        return chr(self.value)

    def __repr__(self):
        pv = str(self)

        if pv.isprintable():
            pv = f"'{pv}'"
        else:
            pv = int(self)

        return f"Byte({pv})"        

@_dataclass(frozen=True)
class ErrorMessage(DiceyError):
    """An error message is an error that can be sent or received via the Dicey protocol"""
    # 16-bit error code
    code: int
    message: _Optional[str] = None

    def __post_init__(self):
        if not INT16_MIN <= self.code <= INT16_MAX:
            raise ValueError("code must be a 16-bit signed integer")

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