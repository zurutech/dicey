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

cdef extern from "dicey/dicey.h":
    cdef const char *DICEY_REGISTRY_PATH
    cdef const char *DICEY_REGISTRY_TRAITS_PATH

    cdef const char *DICEY_INTROSPECTION_TRAIT_NAME

    cdef const char *DICEY_INTROSPECTION_DATA_PROP_NAME
    cdef const char *DICEY_INTROSPECTION_DATA_PROP_SIG

    cdef const char *DICEY_INTROSPECTION_XML_PROP_NAME
    cdef const char *DICEY_INTROSPECTION_XML_PROP_SIG

    cdef const char *DICEY_REGISTRY_TRAIT_NAME

    cdef const char *DICEY_REGISTRY_OBJECTS_PROP_NAME
    cdef const char *DICEY_REGISTRY_OBJECTS_PROP_SIG

    cdef const char *DICEY_REGISTRY_TRAITS_PROP_NAME
    cdef const char *DICEY_REGISTRY_TRAITS_PROP_SIG

    cdef const char *DICEY_REGISTRY_ELEMENT_EXISTS_OP_NAME
    cdef const char *DICEY_REGISTRY_ELEMENT_EXISTS_OP_SIG

    cdef const char *DICEY_REGISTRY_PATH_EXISTS_OP_NAME
    cdef const char *DICEY_REGISTRY_PATH_EXISTS_OP_SIG

    cdef const char *DICEY_REGISTRY_TRAIT_EXISTS_OP_NAME
    cdef const char *DICEY_REGISTRY_TRAIT_EXISTS_OP_SIG

    cdef const char *DICEY_TRAIT_TRAIT_NAME

    cdef const char *DICEY_TRAIT_PROPERTIES_PROP_NAME
    cdef const char *DICEY_TRAIT_PROPERTIES_PROP_SIG

    cdef const char *DICEY_TRAIT_SIGNALS_PROP_NAME
    cdef const char *DICEY_TRAIT_SIGNALS_PROP_SIG

    cdef const char *DICEY_TRAIT_OPERATIONS_PROP_NAME
    cdef const char *DICEY_TRAIT_OPERATIONS_PROP_SIG