
.. _program_listing_file__home_marco_Workspace_dicey_include_dicey_core_errors.h:

Program Listing for File errors.h
=================================

|exhale_lsh| :ref:`Return to documentation for file <file__home_marco_Workspace_dicey_include_dicey_core_errors.h>` (``/home/marco/Workspace/dicey/include/dicey/core/errors.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   // Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
   
   #if !defined(HHQPUVHYDW_ERRORS_H)
   #define HHQPUVHYDW_ERRORS_H
   
   #include <stdbool.h>
   #include <stddef.h>
   
   #include "dicey_export.h"
   
   #if defined(__cplusplus)
   extern "C" {
   #endif
   enum dicey_error {
       DICEY_OK = 0x0000, 
       DICEY_EAGAIN = -0x0101,
   
       DICEY_ENOENT = -0x0102,       
       DICEY_ENOMEM = -0x0103,       
       DICEY_EINVAL = -0x0104,       
       DICEY_ENODATA = -0x0105,      
       DICEY_EBADMSG = -0x0106,      
       DICEY_EOVERFLOW = -0x0107,    
       DICEY_ECONNREFUSED = -0x0108, 
       DICEY_ETIMEDOUT = -0x0109,    
       DICEY_ECANCELLED = -0x010A,   
       DICEY_EALREADY = -0x010B,     
       DICEY_EPIPE = -0x010C,        
       DICEY_ECONNRESET = -0x010D,   
       DICEY_EEXIST = -0x010E,       
       DICEY_EPATH_TOO_LONG = -0x020F,  
       DICEY_ETUPLE_TOO_LONG = -0x0210, 
       DICEY_EARRAY_TOO_LONG = -0x0211, 
       DICEY_EVALUE_TYPE_MISMATCH = -0x0312, 
       DICEY_ENOT_SUPPORTED = -0x0413,       
       DICEY_ECLIENT_TOO_OLD = -0x0414,      
       DICEY_ESERVER_TOO_OLD = -0x0415,      
       DICEY_EPATH_DELETED = -0x0416,        
       DICEY_EPATH_NOT_FOUND = -0x0417,      
       DICEY_EPATH_MALFORMED = -0x0418,      
       DICEY_ETRAIT_NOT_FOUND = -0x0419,     
       DICEY_EELEMENT_NOT_FOUND = -0x041A,   
       DICEY_ESIGNATURE_MALFORMED = -0x041B, 
       DICEY_ESIGNATURE_MISMATCH = -0x041C,  
       DICEY_EPROPERTY_READ_ONLY = -0x041D,  
       DICEY_EPEER_NOT_FOUND = -0x041E,      
       DICEY_ESEQNUM_MISMATCH = -0x041F,     
       DICEY_EUV_UNKNOWN = -0x0520 
   };
   
   struct dicey_error_def {
       enum dicey_error errnum; 
       const char *name;        
       const char *message;     
   };
   
   DICEY_EXPORT const struct dicey_error_def *dicey_error_info(enum dicey_error errnum);
   
   DICEY_EXPORT void dicey_error_infos(const struct dicey_error_def **defs, size_t *count);
   
   DICEY_EXPORT bool dicey_error_is_valid(enum dicey_error errnum);
   
   DICEY_EXPORT const char *dicey_error_msg(enum dicey_error errnum);
   
   DICEY_EXPORT const char *dicey_error_name(enum dicey_error errnum);
   
   #if defined(__cplusplus)
   }
   #endif
   
   #endif // HHQPUVHYDW_ERRORS_H
