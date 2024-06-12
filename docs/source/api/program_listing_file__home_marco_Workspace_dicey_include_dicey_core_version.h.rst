
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_version.h:

Program Listing for File version.h
==================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_version.h>` (``/home/marco/Workspace/dicey/include/dicey/core/version.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   /*
    * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
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
   
   #if !defined(SHAKEUMHSP_VERSION_H)
   #define SHAKEUMHSP_VERSION_H
   
   #include <stdint.h>
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   #define DICEY_PROTO_MAJOR 1
   #define DICEY_PROTO_REVISION 0
   #define DICEY_PROTO_STRING #DICEY_PROTO_MAJOR "r" #DICEY_PROTO_REVISION
   
   struct dicey_version {
       uint16_t major;    
       uint16_t revision; 
   };
   
   #define DICEY_PROTO_VERSION_CURRENT                                                                                    \
       ((struct dicey_version) { .major = DICEY_PROTO_MAJOR, .revision = DICEY_PROTO_REVISION })
   
   DICEY_EXPORT int dicey_version_cmp(struct dicey_version a, struct dicey_version b);
   
   #define DICEY_LIB_VERSION_MAJOR 0
   #define DICEY_LIB_VERSION_MINOR 2
   #define DICEY_LIB_VERSION_PATCH 0
   #define DICEY_LIB_VERSION_STRING #DICEY_LIB_VERSION_MAJOR "." #DICEY_LIB_VERSION_MINOR "." #DICEY_LIB_VERSION_PATCH
   
   #define DICEY_LIB_VER_INT 0x00000001
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // SHAKEUMHSP_VERSION_H
