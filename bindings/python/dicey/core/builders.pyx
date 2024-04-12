# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

from collections.abc import Iterable as _Iterable
from dataclasses import dataclass as _dataclass
from typing import Callable as _Callable

from libc.stdint cimport int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t

# this only works as long as something includes <stdbool.h> before this file - which is the case, given that Dicey does
from libcpp cimport bool as c_bool

from .types import Array, Byte, ErrorMessage, Int16, Int32, Int64, Pair, Path, Selector, UInt16, UInt32, UInt64

from .builders cimport dicey_value_builder, dicey_value_builder_set, \
                       dicey_value_builder_array_start, dicey_value_builder_array_end, \
                       dicey_value_builder_pair_start, dicey_value_builder_pair_end, \
                       dicey_value_builder_tuple_start, dicey_value_builder_tuple_end, \
                       dicey_value_builder_next, \
                       dicey_arg, dicey_bytes_arg, dicey_error_arg 

from .errors   cimport _check
from .type     cimport dicey_type

cdef class _BuilderHandle:
    cdef dicey_value_builder *value

    # we need to keep track of objects that we've created, so that they don't get garbage collected while being held
    # by C code
    cdef list obj_cache

    cdef void set_array(self, a: Array):
        cdef dicey_value_builder elem
        cdef dicey_type inner_type = _converter_for(a.type).dtype

        _check(dicey_value_builder_array_start(self.value, inner_type))

        for item in a.values:
            if not isinstance(item, a.type):
                raise TypeError(f"list of {a.type} contains an element of type {type(item)}") 

            _check(dicey_value_builder_next(self.value, &elem))
            dump_value(&elem, item, self.obj_cache, a.type)

        _check(dicey_value_builder_array_end(self.value))

    cdef void set_bool(self, c_bool b):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_BOOL
        arg.boolean = b

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_byte(self, uint8_t by):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_BYTE
        arg.byte = by

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_bytes(self, bytes b):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_BYTES
        arg.bytes = dicey_bytes_arg(len(b), <const uint8_t*> b)

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_dict(self, dict d):
        arr = Array(
            type=Pair,
            values=list(map(lambda entry: Pair(*entry), d.items())),
        )

        self.obj_cache.append(arr)

        self.set_array(arr)

    cdef void set_err(self, e: ErrorMessage):
        cdef const char *cmsg = NULL;

        if e.message:
            ebytes = e.message.encode("UTF-8")

            self.obj_cache.append(ebytes)

            cmsg = ebytes            

        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_ERROR
        arg.error = dicey_error_arg(e.code, cmsg)

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_float(self, float f):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_FLOAT
        arg.floating = f

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_i16(self, int16_t i):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_INT16
        arg.i16 = i

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_i32(self, int32_t i):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_INT32
        arg.i32 = i

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_i64(self, int64_t i):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_INT64
        arg.i64 = i

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_iterable(self, t: Iterable[object]):
        cdef dicey_value_builder elem

        _check(dicey_value_builder_tuple_start(self.value))

        for item in t:
            _check(dicey_value_builder_next(self.value, &elem))
            dump_value(&elem, item, self.obj_cache)

        _check(dicey_value_builder_tuple_end(self.value))

    cdef void set_pair(self, p: Pair):
        cdef dicey_value_builder elem

        _check(dicey_value_builder_pair_start(self.value))

        for item in (p.first, p.second):
            _check(dicey_value_builder_next(self.value, &elem))
            dump_value(&elem, item, self.obj_cache)

        _check(dicey_value_builder_pair_end(self.value))

    cdef void set_path(self, p: Path):
        sbytes = p.value.encode("UTF-8")

        self.obj_cache.append(sbytes)

        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_PATH
        
        arg.str = sbytes

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_selector(self, s: Selector):
        cdef bytes trait = s.trait.encode("ASCII")
        cdef bytes elem  = s.elem.encode("ASCII")

        self.obj_cache += [trait, elem]

        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_SELECTOR
        arg.selector = dicey_selector(trait, elem)

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_str(self, str s):
        sbytes = s.encode("UTF-8")

        self.obj_cache.append(sbytes)

        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_STR
        arg.str = sbytes

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_u16(self, uint16_t u):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_UINT16
        arg.u16 = u

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_u32(self, uint32_t u):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_UINT32
        arg.u32 = u

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_u64(self, uint64_t u):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_UINT64
        arg.u64 = u

        _check(dicey_value_builder_set(self.value, arg))

    cdef void set_unit(self):
        cdef dicey_arg arg
        arg.type = dicey_type.DICEY_TYPE_UNIT

        _check(dicey_value_builder_set(self.value, arg))

    @staticmethod
    cdef _BuilderHandle new(dicey_value_builder *const value, list obj_cache):
        cdef _BuilderHandle self = _BuilderHandle()
        self.value = value
        self.obj_cache = obj_cache

        return self

def _dicey_matcher(dtype: int) -> _Callable:
    def _matcher(fn: _Callable) -> _Callable:
        fn.dtype = dtype
        return fn

    return _matcher

@_dicey_matcher(dicey_type.DICEY_TYPE_ARRAY)
def _add_array(_BuilderHandle value, a: Array):
    value.set_array(a)

@_dicey_matcher(dicey_type.DICEY_TYPE_BOOL)
def _add_bool(_BuilderHandle value, b: bool):
    value.set_bool(b)

@_dicey_matcher(dicey_type.DICEY_TYPE_BYTE)
def _add_byte(_BuilderHandle value, by: Byte):
    value.set_byte(by)

@_dicey_matcher(dicey_type.DICEY_TYPE_BYTES)
def _add_bytes(_BuilderHandle value, b: bytes):
    value.set_bytes(b)

@_dicey_matcher(dicey_type.DICEY_TYPE_ARRAY)
def _add_dict(_BuilderHandle value, d: dict):
    value.set_dict(d)

@_dicey_matcher(dicey_type.DICEY_TYPE_ERROR)
def _add_err(_BuilderHandle value, e: ErrorMessage):
    value.set_err(e)

@_dicey_matcher(dicey_type.DICEY_TYPE_FLOAT)
def _add_float(_BuilderHandle value, f: float):
    value.set_float(f)

@_dicey_matcher(dicey_type.DICEY_TYPE_INT16)
def _add_i16(_BuilderHandle value, i: Int16):
    value.set_i16(i.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_INT32)
def _add_i32(_BuilderHandle value, i: Int32):
    value.set_i32(i.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_INT64)
def _add_i64(_BuilderHandle value, i: Int64):
    value.set_i64(i.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_TUPLE)
def _add_iterable(_BuilderHandle value, t: Iterable[object]):
    value.set_iterable(t)

@_dicey_matcher(dicey_type.DICEY_TYPE_PAIR)
def _add_pair(_BuilderHandle value, p: Pair):
    value.set_pair(p)

@_dicey_matcher(dicey_type.DICEY_TYPE_PATH)
def _add_path(_BuilderHandle value, p: Path):
    value.set_path(p)

@_dicey_matcher(dicey_type.DICEY_TYPE_SELECTOR)
def _add_selector(_BuilderHandle value, s: Selector):
    value.set_selector(s)

@_dicey_matcher(dicey_type.DICEY_TYPE_STR)
def _add_str(_BuilderHandle value, s: str):
    value.set_str(s)

@_dicey_matcher(dicey_type.DICEY_TYPE_UINT16)
def _add_u16(_BuilderHandle value, u: UInt16):
    value.set_u16(u.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_UINT32)
def _add_u32(_BuilderHandle value, u: UInt32):
    value.set_u32(u.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_UINT64)
def _add_u64(_BuilderHandle value, u: UInt64):
    value.set_u64(u.value)

@_dicey_matcher(dicey_type.DICEY_TYPE_UNIT)
def _add_unit(_BuilderHandle value, ignored):
    value.set_unit()

def _converter_for(type t) -> _Callable:
    return _assoc_list.get(t)

cdef void dump_value(dicey_value_builder *const value, object obj, list obj_cache, type as_type = None):
    assert isinstance(obj_cache, list)

    assoc_conv = _converter_for(as_type if as_type else type(obj))
    
    if assoc_conv:
        assoc_conv(_BuilderHandle.new(value, obj_cache), obj)
    else:
        raise TypeError(f"unsupported type: {type(obj)}")

_assoc_list = {
    bool:         _add_bool,
    type(None):   _add_unit,
    Byte:         _add_byte,
    float:        _add_float,
    Int16:        _add_i16,
    Int32:        _add_i32,
    Int64:        _add_i64,
    UInt16:       _add_u16,
    UInt32:       _add_u32,
    UInt64:       _add_u64,

    Array:        _add_array,
    dict:         _add_dict,
    list:         _add_iterable,
    set:          _add_iterable,
    tuple:        _add_iterable,
    Pair:         _add_pair,

    bytes:        _add_bytes,
    str:          _add_str,

    Path:         _add_path,
    Selector:     _add_selector,
    ErrorMessage: _add_err,
}
