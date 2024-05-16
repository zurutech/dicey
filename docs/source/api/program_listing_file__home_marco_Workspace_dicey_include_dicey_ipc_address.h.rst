
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_ipc_address.h:

Program Listing for File address.h
==================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_ipc_address.h>` (``/home/marco/Workspace/dicey/include/dicey/ipc/address.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(BHSWUFULAM_ADDRESS_H)
   #define BHSWUFULAM_ADDRESS_H
   
   #include <stddef.h>
   
   #include "../core/errors.h"
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_addr {
       const char *addr; 
       size_t len;       
   };
   
   DICEY_EXPORT void dicey_addr_deinit(struct dicey_addr *addr);
   
   DICEY_EXPORT enum dicey_error dicey_addr_dup(struct dicey_addr *dest, struct dicey_addr src);
   
   DICEY_EXPORT const char *dicey_addr_from_str(struct dicey_addr *dest, const char *str);
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // BHSWUFULAM_ADDRESS_H
