
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_ipc_registry.h:

Program Listing for File registry.h
===================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_ipc_registry.h>` (``/home/marco/Workspace/dicey/include/dicey/ipc/registry.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(PIIPYFREDI_REGISTRY_H)
   #define PIIPYFREDI_REGISTRY_H
   
   #include <stdbool.h>
   
   #include "../core/errors.h"
   #include "../core/hashset.h"
   #include "../core/hashtable.h"
   #include "../core/type.h"
   #include "../core/views.h"
   
   #include "traits.h"
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   
   struct dicey_object {
       struct dicey_hashset *traits; 
       void *_cached_xml; 
   };
   
   struct dicey_object_entry {
       const char *path; 
       const struct dicey_object *object; 
   };
   
   DICEY_EXPORT bool dicey_object_implements(const struct dicey_object *object, const char *trait);
   
   struct dicey_element_new_entry {
       enum dicey_element_type
           type; 
       const char *name;      
       const char *signature; 
   };
   
   struct dicey_registry {
       // note: While the paths are technically hierarchical, this has zero to no effect on the actual implementation.
       //       The paths are simply used as a way to identify objects and traits, and "directory-style" access is not
       //       of much use ATM. If this ever becomes useful, it's simple to implement - just swap the hashtable for a
       //       sorted tree or something similar.
       struct dicey_hashtable *_paths;
   
       struct dicey_hashtable *_traits;
   
       // scratchpad buffer used when crafting strings. Non thread-safe like all the rest of the registry.
       struct dicey_view_mut _buffer;
   };
   
   DICEY_EXPORT void dicey_registry_deinit(struct dicey_registry *registry);
   
   DICEY_EXPORT enum dicey_error dicey_registry_init(struct dicey_registry *registry);
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_object_with(struct dicey_registry *registry, const char *path, ...);
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_object_with_trait_list(
       struct dicey_registry *registry,
       const char *path,
       const char *const *trait
   );
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_object_with_trait_set(
       struct dicey_registry *registry,
       const char *path,
       struct dicey_hashset *set
   );
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_trait(struct dicey_registry *registry, struct dicey_trait *trait);
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_trait_with(struct dicey_registry *registry, const char *name, ...);
   
   DICEY_EXPORT enum dicey_error dicey_registry_add_trait_with_element_list(
       struct dicey_registry *registry,
       const char *name,
       const struct dicey_element_new_entry *elems,
       size_t count
   );
   
   DICEY_EXPORT bool dicey_registry_contains_element(
       const struct dicey_registry *registry,
       const char *path,
       const char *trait_name,
       const char *elem
   );
   
   DICEY_EXPORT bool dicey_registry_contains_object(const struct dicey_registry *registry, const char *path);
   
   DICEY_EXPORT bool dicey_registry_contains_trait(const struct dicey_registry *registry, const char *name);
   
   DICEY_EXPORT enum dicey_error dicey_registry_delete_object(struct dicey_registry *registry, const char *const name);
   
   DICEY_EXPORT const struct dicey_element *dicey_registry_get_element(
       const struct dicey_registry *registry,
       const char *path,
       const char *trait_name,
       const char *elem
   );
   
   DICEY_EXPORT bool dicey_registry_get_element_entry(
       const struct dicey_registry *registry,
       const char *path,
       const char *trait_name,
       const char *elem,
       struct dicey_element_entry *entry
   );
   
   DICEY_EXPORT const struct dicey_element *dicey_registry_get_element_from_sel(
       const struct dicey_registry *registry,
       const char *path,
       struct dicey_selector sel
   );
   
   DICEY_EXPORT bool dicey_registry_get_element_entry_from_sel(
       const struct dicey_registry *registry,
       const char *path,
       struct dicey_selector sel,
       struct dicey_element_entry *entry
   );
   
   DICEY_EXPORT const struct dicey_object *dicey_registry_get_object(
       const struct dicey_registry *registry,
       const char *path
   );
   
   DICEY_EXPORT bool dicey_registry_get_object_entry(
       const struct dicey_registry *registry,
       const char *path,
       struct dicey_object_entry *entry
   );
   
   DICEY_EXPORT struct dicey_trait *dicey_registry_get_trait(const struct dicey_registry *registry, const char *name);
   
   DICEY_EXPORT enum dicey_error dicey_registry_remove_object(struct dicey_registry *registry, const char *path);
   
   enum dicey_registry_walk_event {
       DICEY_REGISTRY_WALK_EVENT_OBJECT_END,   
       DICEY_REGISTRY_WALK_EVENT_OBJECT_START, 
       DICEY_REGISTRY_WALK_EVENT_TRAIT_END,    
       DICEY_REGISTRY_WALK_EVENT_TRAIT_START,  
       DICEY_REGISTRY_WALK_EVENT_ELEMENT,      
   };
   
   typedef bool dicey_registry_walk_fn(
       const struct dicey_registry *registry,
       enum dicey_registry_walk_event event,
       const char *path,
       const struct dicey_selector sel,
       const struct dicey_trait *trait,
       const struct dicey_element *element,
       void *user_data
   );
   
   DICEY_EXPORT enum dicey_error dicey_registry_walk_object_elements(
       const struct dicey_registry *registry,
       const char *path,
       dicey_registry_walk_fn *callback,
       void *user_data
   );
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // PIIPYFREDI_REGISTRY_H
