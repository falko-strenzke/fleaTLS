/* fleaTLS cryptographic library
Copyright (C) 2015-2019 cryptosource GmbH

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */


#ifndef _flea_alloc_dbg_int__H_
#define _flea_alloc_dbg_int__H_

#include "internal/common/default.h"
#include "flea/types.h"

#ifdef FLEA_USE_BUF_DBG_CANARIES

void flea_dbg_canaries__signal_canary_error();
void flea_dbg_canaries__clear_canary_error();
int flea_dbg_canaries__is_canary_error_set();

# define __FLEA_SIGNAL_DBG_CANARY_ERROR()     flea_dbg_canaries__signal_canary_error()

# define FLEA_CLEAR_DBG_CANARY_ERROR()        flea_dbg_canaries__clear_canary_error()

# define FLEA_IS_DBG_CANARY_ERROR_SIGNALLED() flea_dbg_canaries__is_canary_error_set()
#endif // ifdef FLEA_USE_BUF_DBG_CANARIES

#ifdef FLEA_USE_BUF_DBG_CANARIES

# define FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name) \
  (((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[0] != 0xDE || \
  ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[1] != 0xAD || \
  ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[2] != 0xBE || \
  ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[3] != 0xEF || \
  ((flea_u8_t*) (&__name[__name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE]))[ 0] != 0xA5 || \
  ((flea_u8_t*) (&__name[__name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE]))[ 1] != 0xAF || \
  ((flea_u8_t*) (&__name[__name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE]))[ 2] != 0x49 || \
  ((flea_u8_t*) (&__name[__name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE]))[ 3] != 0x73)

# define FLEA_NB_STACK_BUF_ENTRIES(__name)     ((sizeof(__name ## _FLEA_DBG_CANARIES__RAW) - 8) / sizeof(__name[0]))
# define __FLEA_GET_ALLOCATED_BUF_NAME(__name) __name ## _FLEA_DBG_CANARIES__RAW
# define FLEA_BUF_SET_CANANRIES(__name, __size) \
  do { \
    ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[0] = 0xDE; \
    ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[1] = 0xAD; \
    ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[2] = 0xBE; \
    ((flea_u8_t*) (__name ## _FLEA_DBG_CANARIES__RAW))[3] = 0xEF; \
    ((flea_u8_t*) (&__name[__size]))[ 0] = 0xA5; \
    ((flea_u8_t*) (&__name[__size]))[ 1] = 0xAF; \
    ((flea_u8_t*) (&__name[__size]))[ 2] = 0x49; \
    ((flea_u8_t*) (&__name[__size]))[ 3] = 0x73; \
  } while(0)

# define FLEA_BUF_CHK_DBG_CANARIES(__name) \
  do { \
    if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
    {__FLEA_FREE_BUF_SET_NULL(__name ## _FLEA_DBG_CANARIES__RAW); /*s.th. tests don't show leak */ \
     __FLEA_SIGNAL_DBG_CANARY_ERROR();}                           /* we are in the cleanup section and cannot use THROW*/ \
  } while(0)

# ifdef FLEA_HEAP_MODE
#  define FLEA_DECL_BUF(__name, __type, __static_size) \
  __type * __name ## _FLEA_DBG_CANARIES__RAW = NULL; \
  __type* __name = NULL; \
  typedef __type __name ## _DBG_CANARIES_HELP_TYPE; \
  flea_u32_t __name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE = 0

#  define FLEA_ALLOC_BUF(__name, __dynamic_size) \
  do { \
    __name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE = __dynamic_size; \
    FLEA_ALLOC_MEM(__name ## _FLEA_DBG_CANARIES__RAW, sizeof(__name[0]) * (__dynamic_size) + 8); \
    __name = (__name ## _DBG_CANARIES_HELP_TYPE*) & (((flea_u8_t*) __name ## _FLEA_DBG_CANARIES__RAW)[4]); \
    FLEA_BUF_SET_CANANRIES(__name, __dynamic_size); \
  } while(0)

#  define FLEA_FREE_BUF_FINAL(__name) \
  do { \
    if(__name) { \
      if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
      {__FLEA_SIGNAL_DBG_CANARY_ERROR();} /* we are in the cleanup section and cannot use THROW*/ \
      FLEA_FREE_MEM(__name ## _FLEA_DBG_CANARIES__RAW); \
    } \
  } while(0)

#  define FLEA_FREE_BUF_FINAL_SECRET_ARR(__name, __type_len) \
  do { \
    if(__name) { \
      flea_memzero_secure((flea_u8_t*) __name, (__type_len) * sizeof(__name[0])); \
      if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
      {__FLEA_SIGNAL_DBG_CANARY_ERROR();} /* we are in the cleanup section and cannot use THROW*/ \
      FLEA_FREE_MEM(__name ## _FLEA_DBG_CANARIES__RAW); \
    } \
  } while(0)

#  define FLEA_FREE_BUF(__name) \
  do { \
    if(__name) { \
      if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
      {__FLEA_SIGNAL_DBG_CANARY_ERROR();} /* we are in the cleanup section and cannot use THROW*/ \
      FLEA_FREE_MEM_SET_NULL(__name ## _FLEA_DBG_CANARIES__RAW); \
      __name = NULL; /*s. th. user buffer is also NULL */ \
    } \
  } while(0)

# elif defined FLEA_STACK_MODE // #ifdef FLEA_HEAP_MODE

#  define FLEA_DECL_BUF(__name, __type, __static_size) \
  flea_u32_t __name ## _FLEA_DBG_CANARIES_DYNAMIC_SIZE = __static_size; \
  flea_u8_t __name ## _FLEA_DBG_CANARIES__RAW[(__static_size) * sizeof(__type) + 8]; \
  __type* __name = (__type*) &(((flea_u8_t*) __name ## _FLEA_DBG_CANARIES__RAW)[4]); \
  FLEA_BUF_SET_CANANRIES(__name, __static_size)

#  define FLEA_STACK_BUF_NB_ENTRIES(__name) ((sizeof(__name ## _FLEA_DBG_CANARIES__RAW) - 8) / sizeof(__name[0]))

#  define FLEA_ALLOC_BUF(__name, __dynamic_size) \

#  define FLEA_FREE_BUF_FINAL(__name) \
  do { \
    if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
    {__FLEA_SIGNAL_DBG_CANARY_ERROR();} \
  } while(0)


#  define FLEA_FREE_BUF(__name) FLEA_FREE_BUF_FINAL(__name)

#  define FLEA_FREE_BUF_FINAL_SECRET_ARR(__name, __type_len) \
  do { \
    if(__name) { \
      flea_memzero_secure((flea_u8_t*) __name, (__type_len) * sizeof(__name[0])); \
      if(FLEA_BUF_DBG_CANARIES_ARE_NOT_OK(__name)) \
      {__FLEA_SIGNAL_DBG_CANARY_ERROR();} \
    } \
  } while(0)
# else // ifdef FLEA_HEAP_MODE
#  error neither heap nor stack buf defined
# endif //  #ifdef FLEA_HEAP_MODE

#endif // #ifdef FLEA_USE_BUF_DBG_CANARIES

#endif /* h-guard */
