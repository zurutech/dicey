# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

from typing import Any as _Any, Callable as _Callable, Optional as _Optional

from dicey.core import DiceyError, Operation, Path, Selector

from dicey.core cimport _PacketWrapper, _check, Message, dicey_packet

from .address cimport Address
from .client cimport dicey_client, dicey_client_args, \
                     dicey_client_new, dicey_client_delete, \
                     dicey_client_set_context, \
                     dicey_client_connect, dicey_client_disconnect, dicey_client_request, \
                     dicey_client_is_running

DEFAULT_TIMEOUT_MS = 1000

EventCallback = _Callable[[Operation, Path, Selector, _Optional[_Any]], None]

cdef void on_cevent(dicey_client *const cclient, void *const ctx, dicey_packet packet) noexcept:
    cdef Message msg
    client = <Client> ctx

    if client.on_event:
        msg = Message.from_cpacket(packet)
        
        client.on_event(msg.operation, msg.path, msg.selector, msg.value)

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

    def set(self, path: Path | str, selector: Selector | (str, str), value: _Any, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        return self.request(Message(Operation.SET, path, selector, value), timeout_ms)

    def request(self, message: Message, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> _Any:
        cdef dicey_packet response
        _check(dicey_client_request(self.client, message.to_cpacket(), &response, timeout_ms))

        cdef _PacketWrapper wrapper = _PacketWrapper.wrap(response)

        return Message.from_cpacket(response).value

    @property
    def running(self) -> bool:
        return dicey_client_is_running(self.client)

    @property
    def on_event(self) -> EventCallback:
        return self._on_event

def connect(addr: Address | str) -> Client:
    cl = Client()

    cl.connect(addr)

    return cl
