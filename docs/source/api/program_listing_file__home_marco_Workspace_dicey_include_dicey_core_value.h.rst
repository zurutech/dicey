
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_value.h:

Program Listing for File value.h
================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_value.h>` (``/home/marco/Workspace/dicey/include/dicey/core/value.h``)

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
   
   #if !defined(TOJAFCVDUG_VALUE_H)
   #define TOJAFCVDUG_VALUE_H
   
   #include <stdbool.h>
   #include <stddef.h>
   #include <stdint.h>
   
   #include "dicey_export.h"
   #include "errors.h"
   #include "type.h"
   
   #include "data-info.h"
   #include "views.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_value {
       // internal data - do not touch
       enum dicey_type _type;
       union _dicey_data_info _data;
   };
   
   struct dicey_iterator {
       uint16_t _type;
       struct dicey_view _data; // Borrowed view over a sequence of dicey_value
   };
   
   DICEY_EXPORT bool dicey_iterator_has_next(struct dicey_iterator iter);
   
   DICEY_EXPORT enum dicey_error dicey_iterator_next(struct dicey_iterator *iter, struct dicey_value *dest);
   
   struct dicey_list {
       // internal data
       uint16_t _type;          // The array type or VARIANT_ID (for a tuple)
       struct dicey_view _data; // Borrowed data associated with this list
   };
   
   DICEY_EXPORT struct dicey_iterator dicey_list_iter(const struct dicey_list *list);
   
   DICEY_EXPORT int dicey_list_type(const struct dicey_list *list);
   
   struct dicey_pair {
       struct dicey_value first;
       struct dicey_value second;
   };
   
   DICEY_EXPORT bool dicey_value_can_be_returned_from(const struct dicey_value *value, const char *sigstr);
   
   DICEY_EXPORT enum dicey_type dicey_value_get_type(const struct dicey_value *value);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_array(const struct dicey_value *value, struct dicey_list *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_bool(const struct dicey_value *value, bool *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_byte(const struct dicey_value *value, uint8_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_bytes(
       const struct dicey_value *value,
       const uint8_t **dest,
       size_t *nbytes
   );
   
   DICEY_EXPORT enum dicey_error dicey_value_get_error(const struct dicey_value *value, struct dicey_errmsg *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_float(const struct dicey_value *value, double *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_i16(const struct dicey_value *value, int16_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_i32(const struct dicey_value *value, int32_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_i64(const struct dicey_value *value, int64_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_pair(const struct dicey_value *value, struct dicey_pair *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_path(const struct dicey_value *value, const char **dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_selector(const struct dicey_value *value, struct dicey_selector *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_str(const struct dicey_value *value, const char **dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_tuple(const struct dicey_value *value, struct dicey_list *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_uuid(const struct dicey_value *value, struct dicey_uuid *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_u16(const struct dicey_value *value, uint16_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_u32(const struct dicey_value *value, uint32_t *dest);
   
   DICEY_EXPORT enum dicey_error dicey_value_get_u64(const struct dicey_value *value, uint64_t *dest);
   
   DICEY_EXPORT bool dicey_value_is(const struct dicey_value *value, enum dicey_type type);
   
   DICEY_EXPORT bool dicey_value_is_compatible_with(const struct dicey_value *value, const char *sigstr);
   
   DICEY_EXPORT bool dicey_value_is_unit(const struct dicey_value *value);
   
   DICEY_EXPORT bool dicey_value_is_valid(const struct dicey_value *value);
   
   #ifdef __cplusplus
   }
   #endif
   
   #endif // TOJAFCVDUG_VALUE_H
