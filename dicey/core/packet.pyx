from dataclasses import dataclass as _dataclass
from enum import Enum as _Enum
import re as _re
from typing import Any as _Any, ClassVar as _ClassVar

from libc.stddef cimport size_t
from libc.stdint cimport uint8_t

from .errors import BadVersionStrError
from .types import Selector

from .errors cimport _check

from .packet cimport dicey_bye, dicey_bye_reason, dicey_packet, dicey_packet_as_bye, dicey_packet_as_hello,  \
                     dicey_packet_as_message, dicey_packet_deinit, dicey_packet_get_kind, dicey_packet_kind, \
                     dicey_packet_load

from .value cimport pythonize_value

cdef class _PacketWrapper:
    cdef dicey_packet packet

    @staticmethod
    cdef _PacketWrapper wrap(dicey_packet packet):
        cdef _PacketWrapper wrap = _PacketWrapper.__new__(_PacketWrapper)
        wrap.packet = packet

        return wrap

    def __deinit__(self):
        dicey_packet_deinit(&self.packet)

class ByeReason(_Enum):
    SHUTDOWN = dicey_bye_reason.DICEY_BYE_REASON_SHUTDOWN
    ERROR = dicey_bye_reason.DICEY_BYE_REASON_ERROR

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

cdef class Bye:
    cdef object _reason

    def __init__(self, reason: ByeReason):
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

cdef class Hello:
    cdef object _version

    def __init__(self, version: Version):
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

cdef class Message:
    cdef str _path
    cdef object _selector
    cdef object _value

    def __init__(self, path: str, selector: Selector, value: _Any):
        self._path = path
        self._selector = selector
        self._value = value

    @property
    def path(self) -> str:
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

        path = message.path.decode("ASCII")
        selector = Selector(message.selector.trait.decode("ASCII"), message.selector.elem.decode("ASCII"))
        value = pythonize_value(&message.value)

        return Message(path, selector, value)

Packet = Bye | Hello | Message

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

def loads(bytes data not None: bytes) -> Packet:
    cdef const void *data_ptr = <uint8_t*> data
    cdef size_t data_len = len(data)
    cdef dicey_packet packet

    _check(dicey_packet_load(&packet, &data_ptr, &data_len))

    # RAII wrapper
    cdef _PacketWrapper wrapper = _PacketWrapper.wrap(packet)

    return _load_packet(packet)
