
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_version.h:

Program Listing for File version.h
==================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_version.h>` (``/home/marco/Workspace/dicey/include/dicey/core/version.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(SHAKEUMHSP_VERSION_H)
   #define SHAKEUMHSP_VERSION_H
   
   #include <stdint.h>
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   #define DICEY_PROTO_MAJOR 0
   #define DICEY_PROTO_REVISION 1
   #define DICEY_PROTO_STRING #DICEY_PROTO_MAJOR "r" #DICEY_PROTO_REVISION
   
   struct dicey_version {
       uint16_t major;    
       uint16_t revision; 
   };
   
   #define DICEY_PROTO_VERSION_CURRENT                                                                                    \
       ((struct dicey_version) { .major = DICEY_PROTO_MAJOR, .revision = DICEY_PROTO_REVISION })
   
   DICEY_EXPORT int dicey_version_cmp(struct dicey_version a, struct dicey_version b);
   
   #define DICEY_LIB_VERSION_MAJOR 0
   #define DICEY_LIB_VERSION_MINOR 0
   #define DICEY_LIB_VERSION_PATCH 1
   #define DICEY_LIB_VERSION_STRING #DICEY_LIB_VERSION_MAJOR "." #DICEY_LIB_VERSION_MINOR "." #DICEY_LIB_VERSION_PATCH
   
   #define DICEY_LIB_VER_INT 0x00000001
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // SHAKEUMHSP_VERSION_H
