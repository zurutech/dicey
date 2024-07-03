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

from libc.stdint cimport uint32_t

from libcpp cimport bool as c_bool

from dicey.core cimport dicey_error, dicey_packet, dicey_selector, dicey_version

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

    ctypedef void dicey_client_on_sub_unsub_done_fn(
        dicey_client *client,
        void *ctx,
        dicey_error status
    )

    ctypedef void dicey_client_event_fn(dicey_client *client, void *ctx, dicey_packet *packet)
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

    dicey_error dicey_client_inspect_path(
        dicey_client *client,
        const char *path,
        dicey_packet *response,
        uint32_t timeout
    )

    dicey_error dicey_client_inspect_path_async(
        dicey_client *client,
        const char *path,
        dicey_client_on_reply_fn *cb,
        void *data,
        uint32_t timeout
    )

    dicey_error dicey_client_inspect_path_as_xml(
        dicey_client *client,
        const char *path,
        dicey_packet *response,
        uint32_t timeout
    )
    
    dicey_error dicey_client_inspect_path_as_xml_async(
        dicey_client *client,
        const char *path,
        dicey_client_on_reply_fn *cb,
        void *data,
        uint32_t timeout
    )

    c_bool dicey_client_is_running(const dicey_client *client)

    dicey_error dicey_client_list_objects(
        dicey_client *client,
        dicey_packet *response,
        uint32_t timeout
    )

    dicey_error dicey_client_list_objects_async(
        dicey_client *client,
        dicey_client_on_reply_fn *cb,
        void *data,
        uint32_t timeout
    )

    dicey_error dicey_client_list_traits(
        dicey_client *client,
        dicey_packet *response,
        uint32_t timeout
    )

    dicey_error dicey_client_list_traits_async(
        dicey_client *client,
        dicey_client_on_reply_fn *cb,
        void *data,
        uint32_t timeout
    )

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

    dicey_error dicey_client_subscribe_to(
        dicey_client *client,
        const char *path,
        dicey_selector sel,
        uint32_t timeout
    )

    dicey_error dicey_client_subscribe_to_async(
        dicey_client *client,
        const char *path,
        dicey_selector sel,
        dicey_client_on_sub_unsub_done_fn *cb,
        void *data,
        uint32_t timeout
    )

    dicey_error dicey_client_unsubscribe_from(
        dicey_client *client,
        const char *path,
        dicey_selector sel,
        uint32_t timeout
    )

    dicey_error dicey_client_unsubscribe_from_async(
        dicey_client *client,
        const char *path,
        dicey_selector sel,
        dicey_client_on_sub_unsub_done_fn *cb,
        void *data,
        uint32_t timeout
    )
