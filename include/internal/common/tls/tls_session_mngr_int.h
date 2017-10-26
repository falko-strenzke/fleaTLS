#ifndef _flea_tls_session_mngr_int__H_
#define _flea_tls_session_mngr_int__H_

#include "flea/tls_session_mngr.h"
#include "internal/common/default.h"
#include "flea/types.h"
#include "internal/common/tls/tls_session_int_fwd.h"
#include "internal/common/tls/tls_const.h"
#include "flea/asn1_date.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLEA_CONST_TLS_SESSION_ID_MAX_LEN 32

struct struct_flea_tls_session_data_t
{
  flea_u8_t  master_secret__au8[FLEA_TLS_MASTER_SECRET_SIZE];
  flea_u16_t cipher_suite_id__u16;
  flea_u8_t  is_valid_session__u8;
};

struct struct_flea_tls_session_entry_t
{
  flea_tls_session_data_t session__t;
  flea_u16_t              use_cnt__u16;
  flea_gmt_time_t         valid_until__t;
  flea_u8_t               session_id__au8[FLEA_TLS_SESSION_ID_LEN];
};

struct struct_flea_tls_session_mngr_t
{
#ifdef FLEA_USE_STACK_BUF
  flea_tls_session_entry_t  sessions__bt[FLEA_TLS_MAX_NB_MNGD_SESSIONS ];
#else
  flea_tls_session_entry_t* sessions__bt;
#endif
  flea_u32_t                session_validity_period_seconds__u32;
  flea_dtl_t                nb_alloc_sessions__dtl;
  flea_u16_t                nb_used_sessions__u16;
};

flea_bool_t flea_tls_session_data_t__is_valid_session(const flea_tls_session_data_t* session__pt);


flea_err_t THR_flea_tls_session_mngr_t__get_free_session_slot(
  flea_tls_session_mngr_t*   session_mngr__pt,
  flea_tls_session_entry_t** result__ppt
);
flea_tls_session_entry_t* flea_tls_session_mngr_t__session_cache_lookup(
  flea_tls_session_mngr_t* session_mngr__pt,
  const flea_u8_t*         session_id__pcu8,
  flea_al_u8_t             session_id_len__alu8
);

void flea_tls_session_data_t__invalidate_session(flea_tls_session_data_t* session__pt);

void flea_tls_session_data_t__set_session_as_valid(flea_tls_session_data_t* session__pt);

#ifdef __cplusplus
}
#endif
#endif /* h-guard */