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

from libc.stddef cimport ptrdiff_t
from libc.stdint cimport int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t

from libcpp cimport bool as c_bool

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
        int16_t code
        const char *message

    ctypedef double dicey_float

    cdef struct dicey_selector:
        const char *trait
        const char *elem

    cdef c_bool dicey_selector_is_valid(dicey_selector selector)
    cdef ptrdiff_t dicey_selector_size(dicey_selector sel)

    cdef enum dicey_type:
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

    cdef int16_t DICEY_VARIANT_ID

    cdef c_bool dicey_type_is_container(dicey_type type)
    cdef c_bool dicey_type_is_valid(dicey_type type)
    cdef const char *dicey_type_name(dicey_type type)
