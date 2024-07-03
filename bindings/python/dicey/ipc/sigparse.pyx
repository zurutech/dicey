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

from itertools import zip_longest as _zip_longest
from functools import cache as _cache
import re as _re
from typing import Callable as _Callable, Iterable as _Iterable

from dicey.core import Byte, ErrorMessage, Int16, Int32, Int64, UInt16, UInt32, UInt64, Array, Pair, UUID, Path, Selector, SignatureMismatchError

from dicey.core cimport dicey_type

# bad: there should be a way to import these as a list IMHO. Probably with cpdef, but it's clunky anyway

_known_types = [
    (dicey_type.DICEY_TYPE_UNIT, type(None)),
    (dicey_type.DICEY_TYPE_BOOL, bool),
    (dicey_type.DICEY_TYPE_BYTE, Byte),
    (dicey_type.DICEY_TYPE_FLOAT, float),
    (dicey_type.DICEY_TYPE_INT16, Int16),
    (dicey_type.DICEY_TYPE_INT32, Int32),
    (dicey_type.DICEY_TYPE_INT64, Int64),
    (dicey_type.DICEY_TYPE_UINT16, UInt16),
    (dicey_type.DICEY_TYPE_UINT32, UInt32),
    (dicey_type.DICEY_TYPE_UINT64, UInt64),
    (dicey_type.DICEY_TYPE_BYTES, bytes),
    (dicey_type.DICEY_TYPE_STR, str),
    (dicey_type.DICEY_TYPE_UUID, UUID),
    (dicey_type.DICEY_TYPE_PATH, Path),
    (dicey_type.DICEY_TYPE_SELECTOR, Selector),
    (dicey_type.DICEY_TYPE_ERROR, ErrorMessage),
]

_ARRAY_TYPE = chr(dicey_type.DICEY_TYPE_ARRAY)
_PAIR_TYPE = chr(dicey_type.DICEY_TYPE_PAIR)
_TUPLE_TYPE = chr(dicey_type.DICEY_TYPE_TUPLE)

_sig_divider = _re.compile(r'\s*->\s*')

_converters = {chr(dtype): converter for dtype, converter in _known_types}

cdef craft_array(object conv, type inner):
    def convert_array(value: _Iterable):
        return Array(inner, tuple(conv(v) for v in value))
    
    return (convert_array, Array)

cdef craft_pair(object key_conv, object value_conv):
    def convert_pair(value: _Iterable):
        elems = tuple(value)

        if len(elems) != 2:
            raise SignatureMismatchError("Wrong number of parameters provided to pair")

        return Pair(key_conv(elems[0]), value_conv(elems[1]))

    return (convert_pair, Pair)

cdef craft_tuple(list converters):
    def convert_tuple(value: _Iterable):
        l = []

        for conv, v in _zip_longest(converters, value):
            if conv is None or v is None:
                raise SignatureMismatchError("Wrong number of parameters provided to tuple")
            
            l.append(conv(v))

        return tuple(l)
    
    return (convert_tuple, tuple)

cdef class _Cursor:
    cdef str _cur
    cdef object _it

    def __init__(self, s: str):
        self._it = iter(s)

        try:
            self._cur = next(self._it)
        except StopIteration:
            self._cur = None

    def peek(self) -> str | None:
        return self._cur

    def next(self) -> str | None:
        if not self._cur:
            raise StopIteration

        value = self._cur

        try:
            self._cur = next(self._it)
        except StopIteration:
            self._cur = None

        return value
    
cdef tuple craft_wrapper(_Cursor cursor): 
    try:
        t = cursor.next() 
    except StopIteration:
        raise SignatureMismatchError("Missing type in signature")

    if t == _ARRAY_TYPE:
        wrapper, ty = craft_wrapper(cursor)
        ret = craft_array(wrapper, ty)

        if cursor.next() != ']':
            raise SignatureMismatchError("Missing closing bracket in array type")

    elif t == _PAIR_TYPE:
        key, _ = craft_wrapper(cursor)

        value, _ = craft_wrapper(cursor)

        if cursor.next() != '}':
            raise SignatureMismatchError("Missing closing bracket in pair type")

        ret = craft_pair(key, value)

    elif t == _TUPLE_TYPE:
        converters = []

        while cursor.peek() != ')':
            wrapper, _ = craft_wrapper(cursor)
            converters.append(wrapper)

        try:
            if cursor.next() != ')':
                raise SignatureMismatchError("Missing closing bracket in tuple type")
        except StopIteration:
            raise SignatureMismatchError("Missing closing bracket in tuple type")

        ret = craft_tuple(converters)

    else:
        try:
            target = _converters[t]
        except KeyError:
            raise SignatureMismatchError(f"Unknown type '{t}'")

        # hack: wrap the wrapper in a wrapper to unwrap any one-value tuples

        def wrapper(value: object):
            if isinstance(value, tuple) and len(value) == 1:
                value = value[0]
                
            return target(value)

        ret = (wrapper, target)

    return ret

@_cache
def wrapper_for(sig: str):
    parts = _sig_divider.split(sig)
    nparts = len(parts)

    if nparts in (1, 2):
        args = parts[0].strip()
    else:
        raise SignatureMismatchError()

    cursor = _Cursor(args)

    res, _ = craft_wrapper(cursor) if args else None

    if cursor.peek():
        raise SignatureMismatchError("Extra characters in signature")

    return res
