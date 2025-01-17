
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_ipc_server.h:

Program Listing for File server.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_ipc_server.h>` (``/home/marco/Workspace/dicey/include/dicey/ipc/server.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   /*
    * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
    *
    * Licensed under the Apache License, Version 2.0 (the "License");
    * you may not use this file except in compliance with the License.
    * You may obtain a copy of the License at
    *
    *     http://www.apache.org/licenses/LICENSE-2.0
    *
    * Unless required by applicable law or agreed to in writing, software
    * distributed under the License is distributed on an "AS IS" BASIS,
    * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    * See the License for the specific language governing permissions and
    * limitations under the License.
    */
   
   #if !defined(JJYWCOYURK_SERVER_H)
   #define JJYWCOYURK_SERVER_H
   
   #include <stdbool.h>
   #include <stddef.h>
   
   #include "../core/errors.h"
   #include "../core/packet.h"
   #include "../core/type.h"
   
   #include "address.h"
   #include "registry.h"
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_client_info {
       size_t id;       
       void *user_data; 
   };
   
   struct dicey_server;
   
   typedef bool dicey_server_on_connect_fn(struct dicey_server *server, size_t id, void **user_data);
   
   typedef void dicey_server_on_disconnect_fn(struct dicey_server *server, const struct dicey_client_info *cln);
   
   typedef void dicey_server_on_error_fn(
       struct dicey_server *server,
       enum dicey_error err,
       const struct dicey_client_info *cln,
       const char *msg,
       ...
   );
   
   typedef void dicey_server_on_request_fn(
       struct dicey_server *server,
       const struct dicey_client_info *cln,
       uint32_t seq,
       struct dicey_packet packet
   );
   
   struct dicey_server_args {
       dicey_server_on_connect_fn *on_connect;       
       dicey_server_on_disconnect_fn *on_disconnect; 
       dicey_server_on_error_fn *on_error;           
       dicey_server_on_request_fn *on_request; 
   };
   
   DICEY_EXPORT void dicey_server_delete(struct dicey_server *state);
   
   DICEY_EXPORT enum dicey_error dicey_server_new(struct dicey_server **dest, const struct dicey_server_args *args);
   
   DICEY_EXPORT enum dicey_error dicey_server_add_object(
       struct dicey_server *server,
       const char *path,
       struct dicey_hashset *trait_names
   );
   
   DICEY_EXPORT enum dicey_error dicey_server_add_object_with(struct dicey_server *server, const char *path, ...);
   
   DICEY_EXPORT enum dicey_error dicey_server_add_trait(struct dicey_server *server, struct dicey_trait *trait);
   
   DICEY_EXPORT enum dicey_error dicey_server_delete_object(struct dicey_server *server, const char *path);
   
   DICEY_EXPORT void *dicey_server_get_context(struct dicey_server *server);
   
   DICEY_EXPORT struct dicey_registry *dicey_server_get_registry(struct dicey_server *server);
   
   DICEY_EXPORT enum dicey_error dicey_server_kick(struct dicey_server *server, size_t id);
   
   DICEY_EXPORT enum dicey_error dicey_server_raise(struct dicey_server *server, struct dicey_packet packet);
   
   /*
    * @brief Raises an event, notifying all clients subscribed to it. This function is synchronous and will block until the
    *        event is actually sent.
    * @note  Even if this function returns, there is no guarantee that the clients actually received anything. This
    * function only guarantees that the `write()` syscall is actually performed and that it succeeded.
    * @param server The server to raise the event from.
    * @param event  The event to raise. The ownership of the packet is transferred to the server, which will free it when
    *               done. This packet must be an event packet.
    * @return       Error code. The possible values are several and include:
    *               - OK: the event was successfully raised
    *               - ENOMEM: memory allocation failed
    *               - EINVAL: the packet is invalid (e.g. it is not an event)
    *               - EELEMENT_NOT_FOUND: the event's element is not found
    */
   DICEY_EXPORT enum dicey_error dicey_server_raise_and_wait(struct dicey_server *server, struct dicey_packet packet);
   
   DICEY_EXPORT enum dicey_error dicey_server_send_response(
       struct dicey_server *server,
       size_t id,
       struct dicey_packet packet
   );
   
   DICEY_EXPORT enum dicey_error dicey_server_send_response_and_wait(
       struct dicey_server *server,
       size_t id,
       struct dicey_packet packet
   );
   
   DICEY_EXPORT void *dicey_server_set_context(struct dicey_server *server, void *new_context);
   
   DICEY_EXPORT enum dicey_error dicey_server_start(struct dicey_server *server, struct dicey_addr addr);
   
   DICEY_EXPORT enum dicey_error dicey_server_stop(struct dicey_server *server);
   
   DICEY_EXPORT enum dicey_error dicey_server_stop_and_wait(struct dicey_server *server);
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // JJYWCOYURK_SERVER_H
