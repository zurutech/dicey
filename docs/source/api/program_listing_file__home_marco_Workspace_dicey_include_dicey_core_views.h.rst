
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_views.h:

Program Listing for File views.h
================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_views.h>` (``/home/marco/Workspace/dicey/include/dicey/core/views.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(KWHQWHOQKQ_TYPES_H)
   #define KWHQWHOQKQ_TYPES_H
   
   #include <stdbool.h>
   #include <stddef.h>
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_view {
       size_t len;       
       const void *data; 
   };
   
   struct dicey_view_mut {
       size_t len; 
       void *data; 
   };
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // KWHQWHOQKQ_TYPES_H
