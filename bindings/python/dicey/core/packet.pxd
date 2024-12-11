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
from libc.stdint cimport uint32_t

from libcpp cimport bool as c_bool

from .errors cimport dicey_error
from .value cimport dicey_value
from .version cimport dicey_version
from .type cimport dicey_selector

cdef extern from "dicey/dicey.h":
    cdef enum dicey_bye_reason:
        DICEY_BYE_REASON_INVALID
        DICEY_BYE_REASON_SHUTDOWN
        DICEY_BYE_REASON_ERROR

    cdef enum dicey_op:
        DICEY_OP_INVALID
        DICEY_OP_GET
        DICEY_OP_SET
        DICEY_OP_EXEC
        DICEY_OP_SIGNAL
        DICEY_OP_RESPONSE

    cdef enum dicey_packet_kind:
        DICEY_PACKET_KIND_INVALID
        DICEY_PACKET_KIND_HELLO
        DICEY_PACKET_KIND_BYE
        DICEY_PACKET_KIND_MESSAGE

    cdef struct dicey_bye:
        dicey_bye_reason reason

    cdef struct dicey_hello:
        dicey_version version

    cdef struct dicey_message:
        dicey_op type
        const char* path
        dicey_selector selector
        dicey_value value

    cdef struct dicey_packet:
        void* payload
        size_t nbytes

    c_bool dicey_bye_reason_is_valid(dicey_bye_reason reason)
    const char* dicey_bye_reason_to_string(dicey_bye_reason reason)

    c_bool dicey_op_is_valid(dicey_op type)
    c_bool dicey_op_requires_payload(dicey_op kind)
    const char* dicey_op_to_string(dicey_op type)

    c_bool dicey_packet_kind_is_valid(dicey_packet_kind kind)
    const char* dicey_packet_kind_to_string(dicey_packet_kind kind)

    dicey_error dicey_packet_load(dicey_packet* packet, const void** data, size_t* nbytes)
    dicey_error dicey_packet_as_bye(dicey_packet packet, dicey_bye* bye)
    dicey_error dicey_packet_as_hello(dicey_packet packet, dicey_hello* hello)
    dicey_error dicey_packet_as_message(dicey_packet packet, dicey_message* message)
    void dicey_packet_deinit(dicey_packet* packet)
    dicey_error dicey_packet_dump(dicey_packet packet, void** data, size_t* nbytes)
    dicey_packet_kind dicey_packet_get_kind(dicey_packet packet)
    dicey_error dicey_packet_get_seq(dicey_packet packet, uint32_t* seq)
    c_bool dicey_packet_is_valid(dicey_packet packet)
    dicey_error dicey_packet_bye(dicey_packet* dest, uint32_t seq, dicey_bye_reason reason)
    dicey_error dicey_packet_hello(dicey_packet* dest, uint32_t seq, dicey_version version)

cdef class Packet:
    cdef uint32_t _seq

cdef class Message(Packet):
    cdef object _op
    cdef object _path
    cdef object _selector
    cdef object _value
    
    @staticmethod
    cdef Message from_cpacket(dicey_packet packet)
    
    cdef dicey_packet to_cpacket(self)

cdef class _PacketWrapper:
    cdef dicey_packet packet

    @staticmethod
    cdef _PacketWrapper wrap(dicey_packet packet)
    