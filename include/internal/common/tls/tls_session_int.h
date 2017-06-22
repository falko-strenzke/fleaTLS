#ifndef _flea_tls_session_int__H_
#define _flea_tls_session_int__H_

#include "flea/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLEA_CONST_TLS_SESSION_ID_MAX_LEN 32

typedef struct
{
  flea_u32_t rd_sequence_number__au32[2];
  flea_u32_t wr_sequence_number__au32[2];
  flea_u8_t  master_secret__au8[48];
  flea_u16_t cipher_suite_id__u16;
} flea_tls_session_data_t;


#ifdef __cplusplus
}
#endif
#endif /* h-guard */
