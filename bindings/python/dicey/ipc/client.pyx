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

from typing import Any as _Any, Callable as _Callable, Optional as _Optional

from inflection import underscore as _underscore

from dicey.core import Byte, DiceyError, ObjectExistsError, Operation, Path, Selector

from dicey.core cimport _PacketWrapper, _check, Message, dicey_packet, dicey_selector

from .sigparse import wrapper_for

from .address cimport Address
from .builtins cimport DICEY_INTROSPECTION_TRAIT_NAME
from .client cimport dicey_client, dicey_client_args, \
                     dicey_client_new, dicey_client_delete, \
                     dicey_client_set_context, \
                     dicey_client_connect, dicey_client_disconnect, dicey_client_request, \
                     dicey_client_is_running, \
                     dicey_client_subscribe_to, dicey_client_unsubscribe_from, \
                     dicey_client_inspect_path, dicey_client_inspect_path_as_xml
from .traits cimport dicey_element_type

DEFAULT_TIMEOUT_MS = 1000

EventCallback = _Callable[[object], None]
GlobalEventCallback = _Callable[[Message], None]

cdef void on_cevent(dicey_client *const cclient, void *const ctx, dicey_packet *const packet) noexcept with gil:
    cdef Message msg
    client = <Client> ctx

    msg = Message.from_cpacket(packet[0])

    # prevent the message from being deallocated twice
    packet[0] = dicey_packet(NULL, 0)

    if client.on_event:    
        client.on_event(msg)

    # this function is basically a private member of client, so it's not a problem to touch its private members
    for callback in client._event_map.get((msg.path, msg.selector), []):
        callback(msg.value)

cdef class Client:
    cdef dicey_client *client

    _on_event: GlobalEventCallback

    _event_map: dict[(Pair, Selector), set[EventCallback]]

    def __cinit__(self):
        cdef dicey_client_args args

        args.on_event = &on_cevent
        args.inspect_func = NULL

        _check(dicey_client_new(&self.client, &args))

        dicey_client_set_context(self.client, <void *> self)

        self._on_event = None
        self._event_map = {}

    def __enter__(self) -> Client:
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.running:
            self.disconnect()

    def __dealloc__(self):
        try:
            # either disconnects or throws an exception, it's the same
            self.disconnect()
        except:
            # ignore errors if any, this is just for safety
            pass

        dicey_client_delete(self.client)

    def connect(self, addr: Address | str):
        cdef dicey_addr caddr

        if isinstance(addr, str):
            caddr = Address(addr).leak()
        else:
            # the address must be cloned: the client wants an owned copy, and I don't like stealing stuff
            # from Python objects
            caddr = addr.clone_raw()
        
        _check(dicey_client_connect(self.client, caddr))

    def disconnect(self):
        _check(dicey_client_disconnect(self.client))

    def exec(self, path: Path | str, selector: Selector | (str, str), arg: _Any = None, *, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.EXEC, path, selector, arg), timeout_ms=timeout_ms)

    def get(self, path: Path | str, selector: Selector | (str, str), *, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.GET, path, selector), timeout_ms=timeout_ms)

    def inspect(self, path: Path | str, *, timeout_ms: int = DEFAULT_TIMEOUT_MS, xml = False) -> _Any:
        cdef dicey_packet response

        if xml:
            _check(dicey_client_inspect_path_as_xml(self.client, str(path).encode('ASCII'), &response, timeout_ms))
        else:
            _check(dicey_client_inspect_path(self.client, str(path).encode('ASCII'), &response, timeout_ms))

        return Message.from_cpacket(response).value

    def object(self, path: Path | str, *, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        return _craft_object_for(self, path, timeout_ms=timeout_ms)

    def objects(self, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        cdef dicey_packet response

        _check(dicey_client_list_objects(self.client, &response, timeout_ms))

        return Message.from_cpacket(response).value

    @property
    def on_event(self) -> GlobalEventCallback:
        return self._on_event

    @on_event.setter
    def on_event(self, value: GlobalEventCallback):
        self._on_event = value

    def register_for(self, path: Path | str, selector: Selector | (str, str), callback: EventCallback):
        self._event_map.setdefault((path, selector), set()).add(callback)

        # as a good measure, subscribe to the event, but ignore the error the server might throw
        # if we're already subscribed
        try:
            self.subscribe(path, selector)
        except ObjectExistsError:
            pass

    def request(self, message: Message, *, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        cdef dicey_packet response
        _check(dicey_client_request(self.client, message.to_cpacket(), &response, timeout_ms))

        cdef _PacketWrapper wrapper = _PacketWrapper.wrap(response)

        return Message.from_cpacket(response).value

    @property
    def running(self) -> bool:
        return dicey_client_is_running(self.client)

    def set(self, path: Path | str, selector: Selector | (str, str), value: _Any, *, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        self.request(Message(Operation.SET, path, selector, value), timeout_ms=timeout_ms)

    def subscribe(self, path: Path | str, selector: Selector | (str, str), *, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        path = str(path).encode('ASCII')

        selector = Selector(*selector) if isinstance(selector, tuple) else selector

        trait = selector.trait.encode('ASCII')
        elem = selector.elem.encode('ASCII')

        _check(dicey_client_subscribe_to(self.client, path, dicey_selector(trait, elem), timeout_ms))

    def traits(self, path: Path | str, *, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.LIST_TRAITS, path), timeout_ms)

    def unregister_from(self, path: Path | str, selector: Selector | (str, str), callback: EventCallback, *, unsubscribe: bool = True):
        if callbacks := self._event_map.get((path, selector)):
            callbacks.discard(callback)

            if not callbacks and unsubscribe:
                self.unsubscribe(path, selector)

    def unsubscribe(self, path: Path | str, selector: Selector | (str, str), *, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        path = str(path).encode('ASCII')

        selector = Selector(*selector) if isinstance(selector, tuple) else selector

        trait = selector.trait.encode('ASCII')
        elem = selector.elem.encode('ASCII')

        _check(dicey_client_unsubscribe_from(self.client, path, dicey_selector(trait, elem), timeout_ms))

class Event:
    def __init__(self, client: Client, path: Path | str, selector: Selector | (str, str)):
        self._client = client
        self._path = Path(path)
        self._selector = Selector(*selector) if isinstance(selector, tuple) else selector

    def register(self, callback: EventCallback):
        self._client.register_for(self._path, self._selector, callback)

def connect(addr: Address | str, *, on_event: _Optional[GlobalEventCallback] = None) -> Client:
    cl = Client()

    cl.on_event = on_event

    cl.connect(addr)

    return cl

class Object:
    def __init__(self, client: Client, path: Path | str, data: _Any, timeout_ms: int):
        self._client = client
        self._path = Path(path)
        self._data = data
        self._timeout_ms = timeout_ms        

    @staticmethod
    def metadata(obj: Object) -> dict[str, dict[str, tuple[Byte, str, _Optional[bool]]]]:
        return obj._data

    @staticmethod
    def path(obj: Object) -> Path:
        return obj._path

    @staticmethod
    def traits(obj: Object) -> tuple[str]:
        return tuple(obj._data.keys())

_PROPERTY = chr(dicey_element_type.DICEY_ELEMENT_TYPE_PROPERTY)
_OPERATION = chr(dicey_element_type.DICEY_ELEMENT_TYPE_OPERATION)
_SIGNAL = chr(dicey_element_type.DICEY_ELEMENT_TYPE_SIGNAL)

def _craft_object_for(client: Client, path: Path | str, timeout_ms: int) -> Object:
    class ObjectImpl(Object):
        pass

    data = client.inspect(path)

    fields = {}

    for trait, elems in data.items():
        for elem, info in elems.items():
            if len(info) == 3:
                dtype, sig, ro = info
            else:
                dtype, sig = info
                ro = False

            if dtype == _PROPERTY:
                prop = property(lambda self, *, trait=trait, elem=elem: self._client.get(self._path, (trait, elem), timeout_ms=self._timeout_ms))
                
                if not ro:
                    wrapper = wrapper_for(sig)
                    prop = prop.setter(lambda self, *value, trait=trait, elem=elem: self._client.set(self._path, (trait, elem),
                        wrapper(value), timeout_ms=self._timeout_ms))

                synthesised = prop

            elif dtype == _OPERATION:
                wrapper = wrapper_for(sig)
                synthesised = lambda self, *args, trait=trait, elem=elem: self._client.exec(self._path, (trait, elem), 
                    wrapper(args), timeout_ms=self._timeout_ms)

            elif dtype == _SIGNAL:
                synthesised = Event(client, path, (trait, elem))
            else:
                raise DiceyError(f"Unknown element type: {dtype}")

            fields.setdefault(elem, {})[trait] = synthesised

    for elem, entries in fields.items():
        elem = _underscore(elem)

        if len(entries) == 1:
            setattr(ObjectImpl, elem, next(iter(entries.values())))
        else:
            for trait, entry in entries.items():
                trait = _underscore(trait).replace(".", "_")
                unique_name = f'{trait}_{elem}'
                setattr(ObjectImpl, unique_name, entry)

    return ObjectImpl(client, path, data, timeout_ms)
    