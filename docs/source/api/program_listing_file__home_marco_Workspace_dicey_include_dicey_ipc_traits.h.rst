
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_ipc_traits.h:

Program Listing for File traits.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_ipc_traits.h>` (``/home/marco/Workspace/dicey/include/dicey/ipc/traits.h``)

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
   
   #if !defined(GFJABYMEEM_TRAITS_H)
   #define GFJABYMEEM_TRAITS_H
   
   #include <stdbool.h>
   #include <stdint.h>
   
   #include "../core/errors.h"
   #include "../core/hashtable.h"
   #include "../core/type.h"
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   enum dicey_element_type {
       DICEY_ELEMENT_TYPE_INVALID,
   
       DICEY_ELEMENT_TYPE_OPERATION, 
       DICEY_ELEMENT_TYPE_PROPERTY,  
       DICEY_ELEMENT_TYPE_SIGNAL,    
   };
   
   struct dicey_element {
       enum dicey_element_type type; 
       const char *signature; 
       bool readonly; 
       uintptr_t _tag; 
   };
   
   struct dicey_element_entry {
       struct dicey_selector sel; 
       const struct dicey_element *element; 
   };
   
   struct dicey_trait_iter {
       struct dicey_hashtable_iter _inner;
   };
   
   struct dicey_trait {
       const char *name; 
       struct dicey_hashtable *elems; 
   };
   
   DICEY_EXPORT struct dicey_trait_iter dicey_trait_iter_start(const struct dicey_trait *trait);
   
   DICEY_EXPORT bool dicey_trait_iter_next(
       struct dicey_trait_iter *iter,
       const char **elem_name,
       struct dicey_element *elem
   );
   
   DICEY_EXPORT void dicey_trait_delete(struct dicey_trait *trait);
   
   DICEY_EXPORT struct dicey_trait *dicey_trait_new(const char *name);
   
   DICEY_EXPORT enum dicey_error dicey_trait_add_element(
       struct dicey_trait *trait,
       const char *name,
       struct dicey_element elem
   );
   
   DICEY_EXPORT bool dicey_trait_contains_element(const struct dicey_trait *trait, const char *name);
   
   DICEY_EXPORT const struct dicey_element *dicey_trait_get_element(const struct dicey_trait *trait, const char *name);
   
   DICEY_EXPORT bool dicey_trait_get_element_entry(
       const struct dicey_trait *trait,
       const char *name,
       struct dicey_element_entry *entry
   );
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // GFJABYMEEM_TRAITS_H
