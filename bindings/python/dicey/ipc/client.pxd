# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

from libc.stdint cimport uint32_t

from libcpp cimport bool as c_bool

from dicey.core cimport dicey_error, dicey_packet, dicey_version

from .address cimport dicey_addr

cdef extern from "dicey/dicey.h":
    cdef enum dicey_client_event_type:
        DICEY_CLIENT_EVENT_CONNECT
        DICEY_CLIENT_EVENT_ERROR
        DICEY_CLIENT_EVENT_HANDSHAKE_START
        DICEY_CLIENT_EVENT_INIT
        DICEY_CLIENT_EVENT_MESSAGE_RECEIVING
        DICEY_CLIENT_EVENT_MESSAGE_SENDING
        DICEY_CLIENT_EVENT_SERVER_BYE
        DICEY_CLIENT_EVENT_QUITTING
        DICEY_CLIENT_EVENT_QUIT

    cdef struct dicey_client_event_error:
        dicey_error err
        char *msg

    cdef struct dicey_client_event:
        dicey_client_event_type type

        dicey_client_event_error error
        dicey_packet packet
        dicey_version version

    cdef struct dicey_client:
        pass

    ctypedef void dicey_client_on_connect_fn(
        dicey_client *client,
        void *ctx,
        dicey_error status,
        const char *msg
    )

    ctypedef void dicey_client_on_disconnect_fn(dicey_client *client, void *ctx, dicey_error status)

    ctypedef void dicey_client_on_reply_fn(
        dicey_client *client,
        void *ctx,
        dicey_error status,
        dicey_packet *packet
    )

    ctypedef void dicey_client_event_fn(dicey_client *client, void *ctx, dicey_packet packet)
    ctypedef void dicey_client_inspect_fn(dicey_client *client, void *ctx, dicey_client_event event)

    cdef struct dicey_client_args:
        dicey_client_inspect_fn *inspect_func
        dicey_client_event_fn *on_event

    dicey_error dicey_client_new(dicey_client **dest, const dicey_client_args *args)
    void dicey_client_delete(dicey_client *client)

    dicey_error dicey_client_connect(dicey_client *client, dicey_addr addr);
    dicey_error dicey_client_connect_async(
        dicey_client *client,
        dicey_addr addr,
        dicey_client_on_connect_fn *cb,
        void *data
    )

    dicey_error dicey_client_disconnect(dicey_client *client)
    dicey_error dicey_client_disconnect_async(
        dicey_client *client,
        dicey_client_on_disconnect_fn *cb,
        void *data
    )

    void *dicey_client_get_context(const dicey_client *client)

    c_bool dicey_client_is_running(const dicey_client *client)

    dicey_error dicey_client_request(
        dicey_client *client,
        dicey_packet packet,
        dicey_packet *response,
        uint32_t timeout
    )

    dicey_error dicey_client_request_async(
        dicey_client *client,
        dicey_packet packet,
        dicey_client_on_reply_fn *cb,
        void *data,
        uint32_t timeout
    )

    void *dicey_client_set_context(dicey_client *client, void *data)
