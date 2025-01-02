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

from dataclasses import dataclass as _dataclass
from enum import Enum as _Enum
import re as _re
from typing import Any as _Any, ClassVar as _ClassVar

from libc.stddef cimport size_t
from libc.stdint cimport uint8_t, uint32_t

from .errors import BadVersionStrError
from .types import Path, Selector

from .builders cimport dicey_message_builder, dicey_message_builder_init, dicey_message_builder_begin, \
                       dicey_message_builder_discard, dicey_message_builder_build, dicey_message_builder_set_seq, \
                       dicey_message_builder_set_path, dicey_message_builder_set_selector, \
                       dicey_message_builder_set_value, dicey_message_builder_value_start, \
                       dicey_message_builder_value_end, dicey_value_builder, \
                       dump_value
                    
from .errors cimport _check

from .packet cimport dicey_packet, dicey_packet_deinit, dicey_op, \
                     dicey_bye, dicey_bye_reason, dicey_packet_as_bye, dicey_packet_bye, \
                     dicey_packet_as_hello, dicey_packet_hello, \
                     dicey_packet_as_message, dicey_packet_get_kind, dicey_packet_kind, \
                     dicey_packet_load

from .value cimport pythonize_value

cdef class _MessageBuilder:
    cdef dicey_message_builder builder

    cdef list obj_cache

    def __cinit__(self):
        dicey_message_builder_init(&self.builder)

    def __init__(self):
        self.obj_cache = []

    def __dealloc__(self):
        dicey_message_builder_discard(&self.builder)    

    cdef dicey_packet build(self):
        cdef dicey_packet packet
        _check(dicey_message_builder_build(&self.builder, &packet))

        return packet

    cdef start(self, int seq, dicey_op op):
        _check(dicey_message_builder_begin(&self.builder, op))
        _check(dicey_message_builder_set_seq(&self.builder, seq))

    cdef set_path(self, str path):
        pbytes = path.encode("ASCII")

        self.obj_cache.append(pbytes)

        _check(dicey_message_builder_set_path(&self.builder, pbytes))

    cdef set_selector(self, dicey_selector sel):
        _check(dicey_message_builder_set_selector(&self.builder, sel))

    cdef set_value(self, object obj):
        cdef dicey_value_builder value

        _check(dicey_message_builder_value_start(&self.builder, &value))

        dump_value(&value, obj, self.obj_cache)

        _check(dicey_message_builder_value_end(&self.builder, &value))

cdef class _PacketWrapper:
    @staticmethod
    cdef _PacketWrapper wrap(dicey_packet packet):
        cdef _PacketWrapper wrap = _PacketWrapper.__new__(_PacketWrapper)
        wrap.packet = packet

        return wrap

    def __dealloc__(self):
        dicey_packet_deinit(&self.packet)

class ByeReason(_Enum):
    SHUTDOWN = dicey_bye_reason.DICEY_BYE_REASON_SHUTDOWN
    ERROR = dicey_bye_reason.DICEY_BYE_REASON_ERROR

class Operation(_Enum):
    GET = dicey_op.DICEY_OP_GET
    SET = dicey_op.DICEY_OP_SET
    EXEC = dicey_op.DICEY_OP_EXEC
    SIGNAL = dicey_op.DICEY_OP_SIGNAL
    RESPONSE = dicey_op.DICEY_OP_RESPONSE

GET = Operation.GET
SET = Operation.SET
EXEC = Operation.EXEC
SIGNAL = Operation.SIGNAL
RESPONSE = Operation.RESPONSE

@_dataclass
class Version:
    major: int
    revision: int

    _matcher: _ClassVar = _re.compile(r"(\d+)r([1-9]\d*)")

    def __str__(self) -> str:
        return f"{self.major}r{self.revision}"

    @staticmethod
    def from_string(str version) -> Version:
        match = Version._matcher.match(version)
        if match is None:
            raise BadVersionStrError(version)

        return Version(int(match[1]), int(match[2]))

cdef class Packet:
    def __init__(self, seq: int = 0):
        self._seq = seq

    @property
    def seq(self) -> int:
        return self._seq

cdef class Bye(Packet):
    cdef object _reason

    def __init__(self, reason: ByeReason, seq: int = 0):
        super().__init__(seq)

        self._reason = reason

    @property
    def reason(self) -> ByeReason:
        return self._reason

    def __repr__(self) -> str:
        return str(self)

    def __str__(self) -> str:
        return f"Bye(reason={self.reason})"

    @staticmethod
    cdef Bye from_cpacket(dicey_packet packet):
        cdef dicey_bye bye
        _check(dicey_packet_as_bye(packet, &bye))

        assert bye.reason in (dicey_bye_reason.DICEY_BYE_REASON_SHUTDOWN, dicey_bye_reason.DICEY_BYE_REASON_ERROR)
        reason = ByeReason(bye.reason)

        return Bye(reason)

    cdef dicey_packet to_cpacket(self):
        cdef dicey_packet packet

        _check(dicey_packet_bye(&packet, 0, self.reason.value))

        return packet

cdef class Hello(Packet):
    cdef object _version

    def __init__(self, version: Version, seq: int = 0):
        super().__init__(seq)

        self._version = version

    @property
    def version(self) -> Version:
        return self._version

    def __repr__(self) -> str:
        return f'Hello(version={self.version!r})'

    def __str__(self) -> str:
        return f"Hello(version={self.version})"

    @staticmethod
    cdef Hello from_cpacket(dicey_packet packet):
        cdef dicey_hello hello
        _check(dicey_packet_as_hello(packet, &hello))

        version = Version(hello.version.major, hello.version.revision)

        return Hello(version)

    cdef dicey_packet to_cpacket(self):
        cdef dicey_packet packet

        _check(dicey_packet_hello(&packet, 0, dicey_version(self.version.major, self.version.revision)))

        return packet

cdef class Message(Packet):
    def __init__(self, op: Operation | str, path: str | Path, selector: Selector | (str, str), value: _Any = None, seq: int = 0):
        super().__init__(seq)

        self._op = Operation[op] if isinstance(op, str) else op
        self._path = Path(path) if isinstance(path, str) else path
        self._selector = Selector(*selector) if isinstance(selector, tuple) else selector
        self._value = value

    @property
    def op(self) -> Operation:
        return self._op

    @property
    def path(self) -> Path:
        return self._path

    @property
    def selector(self) -> Selector:
        return self._selector

    @property
    def value(self) -> _Any:
        return self._value

    def __repr__(self) -> str:
        return f'Message(path={self.path!r}, selector={self.selector!r}, value={self.value!r})'

    def __str__(self) -> str:
        return f"Message(path={self.path}, selector={self.selector}, value={self.value})"

    @staticmethod
    cdef Message from_cpacket(dicey_packet packet):
        cdef dicey_message message
        _check(dicey_packet_as_message(packet, &message))

        op = Operation(message.type)
        path = message.path.decode("ASCII")
        selector = Selector(message.selector.trait.decode("ASCII"), message.selector.elem.decode("ASCII"))
        value = pythonize_value(&message.value)

        return Message(op, path, selector, value)

    cdef dicey_packet to_cpacket(self):
        builder = _MessageBuilder()

        trait = self.selector.trait.encode("ASCII")
        elem = self.selector.elem.encode("ASCII")

        builder.start(self.seq, self.op.value)
        builder.set_path(self.path.value)
        builder.set_selector(dicey_selector(trait, elem))

        if self.op != Operation.GET:
            builder.set_value(self.value)

        return builder.build()

cdef dicey_packet _dump_packet(object packet):
    if isinstance(packet, Bye):
        return Bye.to_cpacket(packet)
    elif isinstance(packet, Hello):
        return Hello.to_cpacket(packet)
    elif isinstance(packet, Message):
        return Message.to_cpacket(packet)
    else:
        assert False, "Unknown packet kind"

cdef object _load_packet(dicey_packet packet):
    cdef dicey_packet_kind kind = dicey_packet_get_kind(packet)

    if kind == dicey_packet_kind.DICEY_PACKET_KIND_BYE:
        return Bye.from_cpacket(packet)
    elif kind == dicey_packet_kind.DICEY_PACKET_KIND_HELLO:
        return Hello.from_cpacket(packet)
    elif kind == dicey_packet_kind.DICEY_PACKET_KIND_MESSAGE:
        return Message.from_cpacket(packet)
    else:
        assert False, "Unknown packet kind"

def dump(object packet not None: Packet, fp):
    data = dumps(packet)

    fp.write(data)

def dumps(object packet not None: Packet) -> bytes:
    cdef dicey_packet cpacket = _dump_packet(packet)
    
    return bytes((<uint8_t*> cpacket.payload)[:cpacket.nbytes])

def load(object fp not None: _Any) -> Packet:
    data = fp.read() # dumbest implementation ever

    return loads(data)

def loads(bytes data not None: bytes) -> Packet:
    cdef const void *data_ptr = <uint8_t*> data
    cdef size_t data_len = len(data)
    cdef dicey_packet packet

    _check(dicey_packet_load(&packet, &data_ptr, &data_len))

    # RAII wrapper
    cdef _PacketWrapper wrapper = _PacketWrapper.wrap(packet)

    return _load_packet(packet)
