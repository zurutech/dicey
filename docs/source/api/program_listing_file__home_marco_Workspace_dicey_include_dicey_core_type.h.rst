
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_type.h:

Program Listing for File type.h
===============================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_type.h>` (``/home/marco/Workspace/dicey/include/dicey/core/type.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(PTDFNAAZWS_TYPE_H)
   #define PTDFNAAZWS_TYPE_H
   
   #include <stdbool.h>
   #include <stddef.h>
   #include <stdint.h>
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   typedef uint8_t dicey_bool;
   
   typedef uint8_t dicey_byte;
   
   typedef int16_t dicey_i16;
   
   typedef int32_t dicey_i32;
   
   typedef int64_t dicey_i64;
   
   typedef uint16_t dicey_u16;
   
   typedef uint32_t dicey_u32;
   
   typedef uint64_t dicey_u64;
   
   struct dicey_errmsg {
       uint16_t code;       
       const char *message; 
   };
   
   typedef double dicey_float;
   
   struct dicey_selector {
       const char *trait; 
       const char *elem;  
   };
   
   DICEY_EXPORT bool dicey_selector_is_valid(struct dicey_selector selector);
   
   DICEY_EXPORT ptrdiff_t dicey_selector_size(struct dicey_selector sel);
   
   enum dicey_type {
       DICEY_TYPE_INVALID = 0, 
       DICEY_TYPE_UNIT = '$', 
       DICEY_TYPE_BOOL = 'b', 
       DICEY_TYPE_BYTE = 'c', 
       DICEY_TYPE_FLOAT = 'f', 
       DICEY_TYPE_INT16 = 'n', 
       DICEY_TYPE_INT32 = 'i', 
       DICEY_TYPE_INT64 = 'x', 
       DICEY_TYPE_UINT16 = 'q', 
       DICEY_TYPE_UINT32 = 'u', 
       DICEY_TYPE_UINT64 = 't', 
       DICEY_TYPE_ARRAY = '[', 
       DICEY_TYPE_TUPLE = '(', 
       DICEY_TYPE_PAIR = '{',  
       DICEY_TYPE_BYTES = 'y', 
       DICEY_TYPE_STR = 's',   
       DICEY_TYPE_PATH = '@',     
       DICEY_TYPE_SELECTOR = '%', 
       DICEY_TYPE_ERROR = 'e', 
   };
   
   DICEY_EXPORT bool dicey_type_is_container(enum dicey_type type);
   
   DICEY_EXPORT bool dicey_type_is_valid(enum dicey_type type);
   
   DICEY_EXPORT const char *dicey_type_name(enum dicey_type type);
   
   #define DICEY_VARIANT_ID ((uint16_t) 'v')
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // PTDFNAAZWS_TYPE_H
