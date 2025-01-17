
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_packet.h:

Program Listing for File packet.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_packet.h>` (``/home/marco/Workspace/dicey/include/dicey/core/packet.h``)

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
   
   #if !defined(XYDQQUJZAI_PACKET_H)
   #define XYDQQUJZAI_PACKET_H
   
   #include <stddef.h>
   #include <stdint.h>
   
   #include "dicey_export.h"
   #include "errors.h"
   #include "value.h"
   #include "version.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   enum dicey_bye_reason {
       DICEY_BYE_REASON_INVALID = 0, 
       DICEY_BYE_REASON_SHUTDOWN = 1, 
       DICEY_BYE_REASON_ERROR = 2,    
   };
   
   DICEY_EXPORT bool dicey_bye_reason_is_valid(enum dicey_bye_reason reason);
   
   DICEY_EXPORT const char *dicey_bye_reason_to_string(enum dicey_bye_reason reason);
   
   enum dicey_op {
       DICEY_OP_INVALID = 0, 
       DICEY_OP_GET = '<',
   
       DICEY_OP_SET = '>',
   
       DICEY_OP_EXEC = '?',
   
       DICEY_OP_EVENT = '!',
   
       DICEY_OP_RESPONSE = ':',
   };
   
   DICEY_EXPORT bool dicey_op_is_valid(enum dicey_op type);
   
   DICEY_EXPORT bool dicey_op_requires_payload(enum dicey_op kind);
   
   DICEY_EXPORT const char *dicey_op_to_string(enum dicey_op type);
   
   enum dicey_packet_kind {
       DICEY_PACKET_KIND_INVALID = 0, 
       DICEY_PACKET_KIND_HELLO,
   
       DICEY_PACKET_KIND_BYE,
   
       DICEY_PACKET_KIND_MESSAGE
   };
   
   DICEY_EXPORT bool dicey_packet_kind_is_valid(enum dicey_packet_kind kind);
   
   DICEY_EXPORT const char *dicey_packet_kind_to_string(enum dicey_packet_kind kind);
   
   struct dicey_bye {
       enum dicey_bye_reason reason; 
   };
   
   struct dicey_hello {
       struct dicey_version version; 
   };
   
   struct dicey_message {
       enum dicey_op type;             
       const char *path;               
       struct dicey_selector selector; 
       struct dicey_value value;       
   };
   
   struct dicey_packet {
       void *payload; 
       size_t nbytes; 
   };
   
   DICEY_EXPORT enum dicey_error dicey_packet_load(struct dicey_packet *packet, const void **data, size_t *nbytes);
   
   DICEY_EXPORT enum dicey_error dicey_packet_as_bye(struct dicey_packet packet, struct dicey_bye *bye);
   
   DICEY_EXPORT enum dicey_error dicey_packet_as_hello(struct dicey_packet packet, struct dicey_hello *hello);
   
   DICEY_EXPORT enum dicey_error dicey_packet_as_message(struct dicey_packet packet, struct dicey_message *message);
   
   DICEY_EXPORT void dicey_packet_deinit(struct dicey_packet *packet);
   
   DICEY_EXPORT enum dicey_error dicey_packet_dump(struct dicey_packet packet, void **data, size_t *nbytes);
   
   DICEY_EXPORT enum dicey_error dicey_packet_forward_message(
       struct dicey_packet *dest,
       struct dicey_packet old,
       uint32_t seq,
       enum dicey_op type,
       const char *path,
       struct dicey_selector selector
   );
   
   DICEY_EXPORT enum dicey_packet_kind dicey_packet_get_kind(struct dicey_packet packet);
   
   DICEY_EXPORT enum dicey_error dicey_packet_get_seq(struct dicey_packet packet, uint32_t *seq);
   
   DICEY_EXPORT enum dicey_error dicey_packet_set_seq(struct dicey_packet packet, uint32_t seq);
   
   DICEY_EXPORT bool dicey_packet_is_valid(struct dicey_packet packet);
   
   DICEY_EXPORT enum dicey_error dicey_packet_bye(struct dicey_packet *dest, uint32_t seq, enum dicey_bye_reason reason);
   
   DICEY_EXPORT enum dicey_error dicey_packet_hello(struct dicey_packet *dest, uint32_t seq, struct dicey_version version);
   
   #ifdef __cplusplus
   }
   #endif
   
   #endif // XYDQQUJZAI_PACKET_H
