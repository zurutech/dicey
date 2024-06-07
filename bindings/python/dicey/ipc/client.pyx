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

from dicey.core import DiceyError, Operation, Path, Selector

from dicey.core cimport _PacketWrapper, _check, Message, dicey_packet, dicey_selector

from .address cimport Address
from .client cimport dicey_client, dicey_client_args, \
                     dicey_client_new, dicey_client_delete, \
                     dicey_client_set_context, \
                     dicey_client_connect, dicey_client_disconnect, dicey_client_request, \
                     dicey_client_is_running, \
                     dicey_client_subscribe_to, dicey_client_unsubscribe_from

DEFAULT_TIMEOUT_MS = 1000

EventCallback = _Callable[[Message], None]

cdef void on_cevent(dicey_client *const cclient, void *const ctx, dicey_packet packet) noexcept with gil:
    cdef Message msg
    client = <Client> ctx

    if client.on_event:
        msg = Message.from_cpacket(packet)
        
        client.on_event(msg)

cdef class Client:
    cdef dicey_client *client

    _on_event: EventCallback

    def __cinit__(self):
        cdef dicey_client_args args

        args.on_event = &on_cevent
        args.inspect_func = NULL

        _check(dicey_client_new(&self.client, &args))

        dicey_client_set_context(self.client, <void *> self)

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

    def exec(self, path: Path | str, selector: Selector | (str, str), arg: _Any = None, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.EXEC, path, selector, arg), timeout_ms)

    def get(self, path: Path | str, selector: Selector | (str, str), timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.GET, path, selector), timeout_ms)

    @property
    def on_event(self) -> EventCallback:
        return self._on_event

    @on_event.setter
    def on_event(self, value: EventCallback):
        self._on_event = value

    def request(self, message: Message, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        cdef dicey_packet response
        _check(dicey_client_request(self.client, message.to_cpacket(), &response, timeout_ms))

        cdef _PacketWrapper wrapper = _PacketWrapper.wrap(response)

        return Message.from_cpacket(response).value

    @property
    def running(self) -> bool:
        return dicey_client_is_running(self.client)

    def set(self, path: Path | str, selector: Selector | (str, str), value: _Any, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        self.request(Message(Operation.SET, path, selector, value), timeout_ms)

    def subscribe(self, path: Path | str, selector: Selector | (str, str), timeout_ms: int = DEFAULT_TIMEOUT_MS):
        path = str(path).encode('ASCII')

        selector = Selector(*selector) if isinstance(selector, tuple) else selector

        trait = selector.trait.encode('ASCII')
        elem = selector.elem.encode('ASCII')

        _check(dicey_client_subscribe_to(self.client, path, dicey_selector(trait, elem), timeout_ms))

    def unsubscribe(self, path: Path | str, selector: Selector | (str, str), timeout_ms: int = DEFAULT_TIMEOUT_MS):
        path = str(path).encode('ASCII')

        selector = Selector(*selector) if isinstance(selector, tuple) else selector

        trait = selector.trait.encode('ASCII')
        elem = selector.elem.encode('ASCII')

        _check(dicey_client_unsubscribe_from(self.client, path, dicey_selector(trait, elem), timeout_ms))

def connect(addr: Address | str, on_event: _Optional[EventCallback] = None) -> Client:
    cl = Client()

    cl.on_event = on_event

    cl.connect(addr)

    return cl
