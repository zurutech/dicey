
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_hashset.h:

Program Listing for File hashset.h
==================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_hashset.h>` (``/home/marco/Workspace/dicey/include/dicey/core/hashset.h``)

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
   
   #if !defined(KVOFDWUXXQ_HASHSET_H)
   #define KVOFDWUXXQ_HASHSET_H
   
   #include "hashtable.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_hashset;
   
   struct dicey_hashset_iter {
       struct dicey_hashtable_iter _inner;
   };
   
   DICEY_EXPORT struct dicey_hashset *dicey_hashset_new(void);
   
   DICEY_EXPORT void dicey_hashset_delete(struct dicey_hashset *table);
   
   DICEY_EXPORT struct dicey_hashset_iter dicey_hashset_iter_start(const struct dicey_hashset *table);
   DICEY_EXPORT bool dicey_hashset_iter_next(struct dicey_hashset_iter *iter, const char **key);
   
   DICEY_EXPORT bool dicey_hashset_contains(const struct dicey_hashset *table, const char *key);
   DICEY_EXPORT bool dicey_hashset_remove(struct dicey_hashset *table, const char *key);
   
   DICEY_EXPORT enum dicey_hash_set_result dicey_hashset_add(struct dicey_hashset **set, const char *key);
   
   DICEY_EXPORT uint32_t dicey_hashset_size(const struct dicey_hashset *table);
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // KVOFDWUXXQ_HASHSET_H
