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

#include "internal/common/default.h"
#include "internal/common/tls/tls_rec_prot_rdr.h"
#include "internal/common/tls/tls_rec_prot.h"
#include "flea/types.h"
#include "flea/error.h"
#include "flea/error_handling.h"
#include "flea/rw_stream.h"

#ifdef FLEA_HAVE_TLS
static flea_err_e THR_flea_rec_prot_rdr_t__read(
  void*                   custom_obj__pv,
  flea_u8_t*              target_buffer__pu8,
  flea_dtl_t*             nb_bytes_to_read__pdtl,
  flea_stream_read_mode_e rd_mode__e
)
{
  flea_tls_rec_prot_rdr_hlp_t* hlp__pt = (flea_tls_rec_prot_rdr_hlp_t*) custom_obj__pv;
  flea_dtl_t nb_bytes_to_read__dtl     = *nb_bytes_to_read__pdtl;

  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(
    THR_flea_recprot_t__read_data(
      hlp__pt->rec_prot__pt,
      (flea_tls_rec_cont_type_e) hlp__pt->record_type__u8,
      target_buffer__pu8,
      &nb_bytes_to_read__dtl,
      rd_mode__e
    )
  );
  *nb_bytes_to_read__pdtl = nb_bytes_to_read__dtl;
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_rw_stream_t__ctor_rec_prot(
  flea_rw_stream_t*            rec_prot_read_str__pt,
  flea_tls_rec_prot_rdr_hlp_t* hlp__pt,
  flea_recprot_t*              rec_prot__pt,
  flea_al_u8_t                 record_type__alu8
)
{
  FLEA_THR_BEG_FUNC();
  hlp__pt->record_type__u8 = record_type__alu8;
  hlp__pt->rec_prot__pt    = rec_prot__pt;
  FLEA_CCALL(
    THR_flea_rw_stream_t__ctor(
      rec_prot_read_str__pt,
      (void*) hlp__pt,
      NULL,
      NULL,
      THR_flea_rec_prot_rdr_t__read,
      NULL,
      NULL,
      0
    )
  );
  FLEA_THR_FIN_SEC_empty();
}

#endif /* ifdef FLEA_HAVE_TLS */
