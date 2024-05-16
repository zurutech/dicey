
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_builders.h:

Program Listing for File builders.h
===================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_builders.h>` (``/home/marco/Workspace/dicey/include/dicey/core/builders.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(FJWTVTVLMM_BUILDERS_H)
   #define FJWTVTVLMM_BUILDERS_H
   
   #include <stdint.h>
   
   #include "dicey_export.h"
   #include "packet.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_message_builder {
       // these fields are internal and should not be accessed directly
   
       int _state;
   
       enum dicey_op _type;
       uint32_t _seq;
       const char *_path;
       struct dicey_selector _selector;
   
       struct dicey_arg *_root; // Root argument of the message. Requires freeing by either discard or build
   
       // While constructing the value, _root is the root of the value, and _borrowed_to is the value builder that is
       // holding the lock over this message builder.
       const struct dicey_value_builder *_borrowed_to;
   };
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_init(struct dicey_message_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_begin(struct dicey_message_builder *builder, enum dicey_op op);
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_build(
       struct dicey_message_builder *builder,
       struct dicey_packet *packet
   );
   
   DICEY_EXPORT void dicey_message_builder_discard(struct dicey_message_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_set_path(struct dicey_message_builder *builder, const char *path);
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_set_selector(
       struct dicey_message_builder *builder,
       struct dicey_selector selector
   );
   
   struct dicey_array_arg {
       enum dicey_type type;          
       uint16_t nitems;               
       const struct dicey_arg *elems; 
   };
   
   struct dicey_bytes_arg {
       uint32_t len;        
       const uint8_t *data; 
   };
   
   struct dicey_error_arg {
       uint16_t code;       
       const char *message; 
   };
   
   struct dicey_pair_arg {
       const struct dicey_arg *first;  
       const struct dicey_arg *second; 
   };
   
   struct dicey_tuple_arg {
       uint16_t nitems;               
       const struct dicey_arg *elems; 
   };
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *builder, uint32_t seq);
   
   struct dicey_arg {
       enum dicey_type type; 
       union {
           dicey_bool boolean;   
           dicey_byte byte;      
           dicey_float floating; 
           dicey_i16 i16;        
           dicey_i32 i32;        
           dicey_i64 i64;        
           dicey_u16 u16;        
           dicey_u32 u32;        
           dicey_u64 u64;        
           struct dicey_array_arg array; 
           struct dicey_tuple_arg tuple; 
           struct dicey_pair_arg pair; 
           struct dicey_bytes_arg bytes; 
           const char *str;
   
           struct dicey_selector selector;
   
           struct dicey_error_arg error; 
       };
   };
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_set_value(
       struct dicey_message_builder *builder,
       struct dicey_arg value
   );
   
   struct dicey_value_builder {
       // these fields are part of the internal interface of the value builder. Do not tamper with them
   
       int _state;
   
       // root of the built message, i.e. a leaf if the value is not a compound type, or a node otherwise.
       // this value is borrowed from the message builder, and must not be freed by the value builder
       struct dicey_arg *_root;
   
       // specialised builder structure for subvalues (used by arrays and tuples)
       struct _dicey_value_builder_list {
           enum dicey_type type; // type of the elements. Only valid if the value is an array
   
           // dynamic array of elements. Only valid if the value is an array or a tuple
           uint16_t nitems;
           size_t cap;
           struct dicey_arg *elems;
       } _list;
   };
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_value_start(
       struct dicey_message_builder *builder,
       struct dicey_value_builder *value
   );
   
   DICEY_EXPORT enum dicey_error dicey_message_builder_value_end(
       struct dicey_message_builder *builder,
       struct dicey_value_builder *value
   );
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_array_start(
       struct dicey_value_builder *builder,
       enum dicey_type type
   );
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *builder);
   
   DICEY_EXPORT bool dicey_value_builder_is_list(const struct dicey_value_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_next(
       struct dicey_value_builder *list,
       struct dicey_value_builder *elem
   );
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_pair_start(struct dicey_value_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_pair_end(struct dicey_value_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_set(struct dicey_value_builder *builder, struct dicey_arg value);
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *builder);
   
   DICEY_EXPORT enum dicey_error dicey_packet_message(
       struct dicey_packet *dest,
       uint32_t seq,
       enum dicey_op op,
       const char *path,
       struct dicey_selector selector,
       struct dicey_arg value
   );
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // FJWTVTVLMM_BUILDERS_H
