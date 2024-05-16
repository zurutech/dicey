
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_ipc_server.h:

Program Listing for File server.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_ipc_server.h>` (``/home/marco/Workspace/dicey/include/dicey/ipc/server.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
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
   
   DICEY_EXPORT enum dicey_error dicey_server_send(struct dicey_server *server, size_t id, struct dicey_packet packet);
   
   DICEY_EXPORT enum dicey_error dicey_server_send_and_wait(
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
