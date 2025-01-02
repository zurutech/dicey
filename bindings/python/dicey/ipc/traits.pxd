# Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
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

cdef extern from "dicey/dicey.h":
    cdef enum dicey_element_type:
        DICEY_ELEMENT_TYPE_INVALID
        DICEY_ELEMENT_TYPE_OPERATION
        DICEY_ELEMENT_TYPE_PROPERTY
        DICEY_ELEMENT_TYPE_SIGNAL
