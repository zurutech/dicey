# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

from collections.abc import Callable as _Callable
from dataclasses import dataclass as _dataclass

from libc.stddef cimport size_t
from libc.stdint cimport uint8_t

from libcpp cimport bool as c_bool

from .errors import InvalidDataError
from .types import Array, Byte, ErrorMessage, Int16, Int32, Int64, Pair, Path, Selector, UInt16, UInt32, UInt64

from .errors cimport _check
from .type   cimport dicey_type, dicey_selector, DICEY_VARIANT_ID
from .value  cimport dicey_value, dicey_errmsg, dicey_iterator, dicey_list, dicey_pair, \
                     dicey_iterator_has_next, dicey_iterator_next, dicey_list_iter, dicey_list_type, \
                     dicey_value_get_bool, dicey_value_get_byte, dicey_value_get_float, \
                     dicey_value_get_i16, dicey_value_get_i32, dicey_value_get_i64, \
                     dicey_value_get_u16, dicey_value_get_u32, dicey_value_get_u64, \
                     dicey_value_get_array, dicey_value_get_tuple, dicey_value_get_pair, \
                     dicey_value_get_bytes, dicey_value_get_str, dicey_value_get_path, \
                     dicey_value_get_selector, dicey_value_get_error

@_dataclass
class _AssocArgs:
    array_cls: type
    pair_lists_as_dict: bool
    value_hook: Callable[[object], object]

cdef struct _type_assoc:
    dicey_type type
    object (*_to_python)(const dicey_value *value, object args)

# note: cython is dumb and doesn't allow for unbound arrays, so I have to hardcode the length here
cdef _type_assoc[19] _assoc_list = [
    _type_assoc(dicey_type.DICEY_TYPE_INVALID, &_to_invalid),
    _type_assoc(dicey_type.DICEY_TYPE_UNIT, &_to_unit),
    _type_assoc(dicey_type.DICEY_TYPE_BOOL, &_to_bool),
    _type_assoc(dicey_type.DICEY_TYPE_BYTE, &_to_byte),
    _type_assoc(dicey_type.DICEY_TYPE_FLOAT, &_to_float),
    _type_assoc(dicey_type.DICEY_TYPE_INT16, &_to_int16),
    _type_assoc(dicey_type.DICEY_TYPE_INT32, &_to_int32),
    _type_assoc(dicey_type.DICEY_TYPE_INT64, &_to_int64),
    _type_assoc(dicey_type.DICEY_TYPE_UINT16, &_to_uint16),
    _type_assoc(dicey_type.DICEY_TYPE_UINT32, &_to_uint32),
    _type_assoc(dicey_type.DICEY_TYPE_UINT64, &_to_uint64),
    _type_assoc(dicey_type.DICEY_TYPE_ARRAY, &_to_array),
    _type_assoc(dicey_type.DICEY_TYPE_TUPLE, &_to_tuple),
    _type_assoc(dicey_type.DICEY_TYPE_PAIR, &_to_pair), 
    _type_assoc(dicey_type.DICEY_TYPE_BYTES, &_to_bytes),
    _type_assoc(dicey_type.DICEY_TYPE_STR, &_to_str),
    _type_assoc(dicey_type.DICEY_TYPE_PATH, &_to_path),
    _type_assoc(dicey_type.DICEY_TYPE_SELECTOR, &_to_selector),
    _type_assoc(dicey_type.DICEY_TYPE_ERROR, &_to_errmsg)
]

cdef const _type_assoc *_find_type_assoc(const dicey_type ty):
    cdef const _type_assoc *assoc = NULL
    cdef size_t assoc_len = sizeof(_assoc_list) // sizeof(_type_assoc)

    for i in range(assoc_len):
        assoc = &_assoc_list[i]

        if assoc.type == ty:
            return assoc

    return NULL

cdef _to_array(const dicey_value *const value, object args):
    cdef dicey_list lst

    _check(dicey_value_get_array(value, &lst))

    array_type = dicey_list_type(&lst)

    if array_type == dicey_type.DICEY_TYPE_PAIR and args.pair_lists_as_dict:
        return _to_dict(&lst, args)
    else:
        l = _to_list(&lst, args)

        return Array(type=l[0].__class__ if l else type(None), values=l)
    
cdef _to_bool(const dicey_value *const value, object args):
    cdef c_bool dest = 0

    _check(dicey_value_get_bool(value, &dest))

    return bool(dest)

cdef _to_byte(const dicey_value *const value, object args):
    cdef uint8_t dest = 0

    _check(dicey_value_get_byte(value, &dest))

    return Byte(dest)
    
cdef _to_bytes(const dicey_value *const value, object args):
    cdef const uint8_t *bys = NULL
    cdef size_t nbytes = 0

    _check(dicey_value_get_bytes(value, &bys, &nbytes))

    return bytes(bys[:nbytes])

cdef _to_dict(const dicey_list *const list, object args):
    d = {}

    cdef dicey_iterator it = dicey_list_iter(list)

    cdef dicey_pair pair
    cdef dicey_value pair_val

    while dicey_iterator_has_next(it):
        _check(dicey_iterator_next(&it, &pair_val))

        _check(dicey_value_get_pair(&pair_val, &pair))

        key = _to_value(&pair.first, args)

        if key in d:
            raise ValueError("Duplicate key in pair list")

        d[key] = _to_value(&pair.second, args)

    return d

cdef _to_errmsg(const dicey_value *const value, object args):
    cdef dicey_errmsg err

    _check(dicey_value_get_error(value, &err))

    return ErrorMessage(err.code, err.message.decode('utf-8') if err.message else "unspecified")

cdef _to_float(const dicey_value *const value, object args):
    cdef double dest = .0

    _check(dicey_value_get_float(value, &dest))

    return dest

cdef _to_int16(const dicey_value *const value, object args):
    cdef int16_t dest = 0

    _check(dicey_value_get_i16(value, &dest))

    return Int16(dest)

cdef _to_int32(const dicey_value *const value, object args):
    cdef int32_t dest = 0

    _check(dicey_value_get_i32(value, &dest))

    return Int32(dest)

cdef _to_int64(const dicey_value *const value, object args):
    cdef int64_t dest = 0

    _check(dicey_value_get_i64(value, &dest))

    return Int64(dest)

cdef _to_invalid(const dicey_value *value, object args):
    raise InvalidDataError()

cdef _to_list(const dicey_list *const list, object args):
    l = []

    cdef dicey_iterator it = dicey_list_iter(list)

    cdef dicey_value item

    while dicey_iterator_has_next(it):
        _check(dicey_iterator_next(&it, &item))

        l.append(_to_value(&item, args))

    is_array = dicey_list_type(list) != DICEY_VARIANT_ID

    if is_array and args.array_cls is not tuple:
        return args.array_cls(l)
    else:
        return tuple(l)

cdef _to_pair(const dicey_value *const value, object args):
    cdef dicey_pair pair

    _check(dicey_value_get_pair(value, &pair))

    return (_to_value(&pair.first, args), _to_value(&pair.second, args))

cdef _to_path(const dicey_value *const value, object args):
    cdef const char *s = NULL

    _check(dicey_value_get_path(value, &s))
    
    return Path(s.decode('ASCII'))

cdef _to_selector(const dicey_value *const value, object args):
    cdef dicey_selector sel

    _check(dicey_value_get_selector(value, &sel))

    return Selector(sel.trait.decode('ASCII'), sel.elem.decode('ASCII'))

cdef _to_str(const dicey_value *const value, object args):
    cdef const char *s = NULL

    _check(dicey_value_get_str(value, &s))

    return s.decode('utf-8')

cdef _to_tuple(const dicey_value *const value, object args):
    cdef dicey_list lst

    _check(dicey_value_get_tuple(value, &lst))

    return _to_list(&lst, args)

cdef _to_uint16(const dicey_value *const value, object args):
    cdef uint16_t dest = 0

    _check(dicey_value_get_u16(value, &dest))

    return UInt16(dest)

cdef _to_uint32(const dicey_value *const value, object args):
    cdef uint32_t dest = 0

    _check(dicey_value_get_u32(value, &dest))

    return UInt32(dest)

cdef _to_uint64(const dicey_value *const value, object args):
    cdef uint64_t dest = 0

    _check(dicey_value_get_u64(value, &dest))

    return UInt64(dest)

cdef _to_unit(const dicey_value *const value, object args):
    return None

cdef _to_value(const dicey_value *const value, object args):
    if not value or not args:
        raise ValueError("Invalid arguments")

    cdef dicey_type ty = dicey_value_get_type(value)
    if ty == dicey_type.DICEY_TYPE_INVALID:
        raise InvalidDataError()

    cdef const _type_assoc *assoc

    assoc = _find_type_assoc(ty)
    if not assoc:
        raise InvalidDataError()

    py_val = assoc._to_python(value, args)

    return args.value_hook(py_val) if args.value_hook else py_val

# note: the default array class here is tuple due to the fact it is immutable, and thus can be used as a dict key
cdef pythonize_value(const dicey_value *const value, object value_hook=None, type array_cls=tuple, bint pair_lists_as_dict=True):
    args = _AssocArgs(
        array_cls = array_cls,
        pair_lists_as_dict = pair_lists_as_dict,
        value_hook = value_hook,
    )

    ret = _to_value(value, args)

    # if the error is the root value, raise it as an error for convenience
    if isinstance(ret, ErrorMessage):
        raise ret

    return ret    
