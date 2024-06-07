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
