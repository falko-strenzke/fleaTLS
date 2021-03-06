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

#include "internal/common/tls/tls_client_int_ecc.h"
#include "internal/common/default.h"
#include "flea/error_handling.h"
#include "flea/tls_session_mngr.h"
#include "flea/alloc.h"

#ifdef FLEA_HAVE_TLS_CLIENT

flea_err_e THR_flea_tls__snd_clt_kex_ecdhe(
  flea_tls_ctx_t*           tls_ctx__pt,
  flea_tls_handshake_ctx_t* hs_ctx__pt,
  flea_tls_prl_hash_ctx_t*  p_hash_ctx__pt
)
{
  flea_ref_cu8_t pub_point__rcu8;
  flea_u8_t pub_point_len__u8;
  flea_u32_t hdr_len__u32;

  FLEA_THR_BEG_FUNC();


  flea_pubkey_t__get_encoded_plain_ref(hs_ctx__pt->ecdhe_pub_key__pt, &pub_point__rcu8);
  hdr_len__u32 = pub_point__rcu8.len__dtl + 1;


  FLEA_CCALL(
    THR_flea_tls__snd_hands_msg_hdr(
      &tls_ctx__pt->rec_prot__t,
      p_hash_ctx__pt,
      HANDSHAKE_TYPE_CLIENT_KEY_EXCHANGE,
      hdr_len__u32
    )
  );

  pub_point_len__u8 = (flea_u8_t) pub_point__rcu8.len__dtl;
  FLEA_CCALL(
    THR_flea_tls__snd_hands_msg_content(
      &tls_ctx__pt->rec_prot__t,
      p_hash_ctx__pt,
      &pub_point_len__u8,
      1
    )
  );

  FLEA_CCALL(
    THR_flea_tls__snd_hands_msg_content(
      &tls_ctx__pt->rec_prot__t,
      p_hash_ctx__pt,
      pub_point__rcu8.data__pcu8,
      pub_point__rcu8.len__dtl
    )
  );


  FLEA_THR_FIN_SEC(
    flea_pubkey_t__dtor(hs_ctx__pt->ecdhe_pub_key__pt);
  );
} /* THR_flea_tls__snd_clt_kex_ecdhe */

#endif /* ifdef FLEA_HAVE_TLS_CLIENT */
