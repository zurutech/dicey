
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_typedescr.h:

Program Listing for File typedescr.h
====================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_typedescr.h>` (``/home/marco/Workspace/dicey/include/dicey/core/typedescr.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(WQWGWWXACS_TYPEDESCR_H)
   #define WQWGWWXACS_TYPEDESCR_H
   
   #include <stdbool.h>
   
   #include "views.h"
   
   #include "dicey_export.h"
   
   enum dicey_typedescr_kind {
       DICEY_TYPEDESCR_INVALID,
   
       DICEY_TYPEDESCR_VALUE,      
       DICEY_TYPEDESCR_FUNCTIONAL, 
   };
   
   struct dicey_typedescr_op {
       struct dicey_view input;  
       struct dicey_view output; 
   };
   
   struct dicey_typedescr {
       enum dicey_typedescr_kind kind;
   
       union {
           const char *value;            
           struct dicey_typedescr_op op; 
       };
   };
   
   DICEY_EXPORT bool dicey_typedescr_is_valid(const char *typedescr);
   
   DICEY_EXPORT bool dicey_typedescr_parse(const char *typedescr, struct dicey_typedescr *descr);
   
   #endif // WQWGWWXACS_TYPEDESCR_H
