/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "internal/common/default.h"
#include "flea/types.h"
#include "flea/alloc.h"
#include "flea/cert_verify.h"
#include "flea/error_handling.h"
#include "internal/common/ber_dec.h"
#include "flea/x509.h"
#include "flea/tls.h"
#include "flea/rw_stream.h"
#include "flea/cert_store.h"
#include "internal/common/cert_path_int.h"
#include "internal/common/tls/handsh_reader.h"
#include "internal/pltf_if/time.h"

#define FLEA_TLS_CERT_BUF_SIZE     1536
#define FLEA_TLS_CERT_PATH_MAX_LEN 20

flea_err_t THR_flea_tls__cert_path_validation(
  flea_tls_ctx_t*          tls_ctx__pt,
  // flea_tls_handsh_reader_t* hs_rdr__pt,
  flea_rw_stream_t*        rd_strm__pt,
  const flea_cert_store_t* trust_store__pt,
  flea_public_key_t*       pubkey_to_construct__pt
)
{
  flea_u8_t enc_len__au8[3];
  // flea_rw_stream_t* rd_strm__pt;
  flea_bool_t finished__b = FLEA_FALSE;
  flea_bool_t even__b     = FLEA_TRUE;
  flea_bool_t first__b    = FLEA_TRUE;
  flea_gmt_time_t compare_time__t;
  flea_al_u16_t cert_count__alu16 = 0;

  FLEA_DECL_BUF(cert_buf_1__bu8, flea_u8_t, FLEA_TLS_CERT_BUF_SIZE);
  flea_al_u16_t buf_1_len__alu16     = 0;
  flea_x509_cert_ref_t cert_ref_1__t = flea_x509_cert_ref_t__INIT_VALUE;
  FLEA_DECL_BUF(cert_buf_2__bu8, flea_u8_t, FLEA_TLS_CERT_BUF_SIZE);
  flea_al_u16_t buf_2_len__alu16     = 0;
  flea_x509_cert_ref_t cert_ref_2__t = flea_x509_cert_ref_t__INIT_VALUE;
  flea_u32_t prev_cert_len__u32      = 0;

  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(THR_flea_pltfif_time__get_current_time(&compare_time__t));

  // rd_strm__pt = flea_tls_handsh_reader_t__get_read_stream(hs_rdr__pt);
  do
  {
    flea_u32_t new_cert_len__u32;
    flea_u8_t* new_cert__pu8;
    flea_x509_cert_ref_t* new_cert_ref__pt;
    flea_u8_t* prev_cert__pu8;
    // flea_x509_cert_ref_t* prev_cert_ref__pt;
    flea_bool_t is_new_cert_trusted__b;
    flea_u16_t path_len__u16;
    flea_basic_constraints_t* basic_constraints__pt;

    if(++cert_count__alu16 > FLEA_TLS_CERT_PATH_MAX_LEN)
    {
      FLEA_THROW("maximal cert path size for TLS exceeded", FLEA_ERR_INV_ARG);
    }

    FLEA_CCALL(THR_flea_rw_stream_t__force_read(rd_strm__pt, enc_len__au8, sizeof(enc_len__au8)));
    new_cert_len__u32 = ((flea_u32_t) enc_len__au8[0] << 16) | (enc_len__au8[1] << 8) | (enc_len__au8[2]);
    if(even__b)
    {
      FLEA_FREE_BUF(cert_buf_1__bu8);
      if(buf_1_len__alu16 < new_cert_len__u32)
      {
        FLEA_FREE_BUF(cert_buf_1__bu8);
        FLEA_ALLOC_BUF(cert_buf_1__bu8, new_cert_len__u32);
        buf_1_len__alu16 = new_cert_len__u32;
      }
      // TODO: STACK BUF LIMIT CHECK
      new_cert__pu8    = cert_buf_1__bu8;
      new_cert_ref__pt = &cert_ref_1__t;
      even__b = FLEA_FALSE;

      prev_cert__pu8 = cert_buf_2__bu8;
      // prev_cert_ref__pt = &cert_ref_2__t;
    }
    else
    {
      /*FLEA_FREE_BUF(cert_buf_2__bu8);
       * FLEA_ALLOC_BUF(cert_buf_2__bu8, new_cert_len__u32);*/

      if(buf_2_len__alu16 < new_cert_len__u32)
      {
        // TODO: STACK BUF LIMIT CHECK
        FLEA_FREE_BUF(cert_buf_2__bu8);
        FLEA_ALLOC_BUF(cert_buf_2__bu8, new_cert_len__u32);
        buf_2_len__alu16 = new_cert_len__u32;
      }
      new_cert__pu8    = cert_buf_2__bu8;
      new_cert_ref__pt = &cert_ref_2__t;
      even__b = FLEA_TRUE;

      prev_cert__pu8 = cert_buf_1__bu8;
      // prev_cert_ref__pt = &cert_ref_1__t;
    }
    FLEA_CCALL(THR_flea_rw_stream_t__force_read(rd_strm__pt, new_cert__pu8, new_cert_len__u32));

    FLEA_CCALL(THR_flea_x509_cert_ref_t__ctor(new_cert_ref__pt, new_cert__pu8, new_cert_len__u32));

    basic_constraints__pt = &new_cert_ref__pt->extensions__t.basic_constraints__t;
    FLEA_CCALL(
      THR_flea_cert_store_t__is_cert_trusted(
        trust_store__pt,
        new_cert__pu8,
        new_cert_len__u32,
        &is_new_cert_trusted__b
      )
    );

    if(!flea_x509_is_cert_self_issued(new_cert_ref__pt) && !first__b)
    {
      if(basic_constraints__pt->is_present__u8)
      {
        if(basic_constraints__pt->has_path_len__b)
        {
          if(path_len__u16 > basic_constraints__pt->path_len__u16)
          {
            FLEA_THROW("path len constraint exceeded", FLEA_ERR_CERT_PATH_LEN_CONSTR_EXCEEDED);
          }
        }
      }
      path_len__u16++;
    }


    FLEA_CCALL(
      THR_flea_cert_path__validate_single_cert(
        new_cert_ref__pt,
        is_new_cert_trusted__b,
        first__b,
        &compare_time__t
      )
    );

    if(!first__b)
    {
      FLEA_CCALL(
        THR_flea_x509_verify_cert_signature(
          prev_cert__pu8,
          prev_cert_len__u32,
          new_cert__pu8,
          new_cert_len__u32
        )
      );
    }
    else
    {
      // TODO: VALIDATED CERT KEY USAGE FOR TLS
      FLEA_CCALL(THR_flea_public_key_t__ctor_cert(pubkey_to_construct__pt, new_cert_ref__pt));
    }
    if(is_new_cert_trusted__b)
    {
      break;
    }
    first__b = FLEA_FALSE;
    prev_cert_len__u32 = new_cert_len__u32;
  } while(!finished__b);
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(cert_buf_1__bu8);
    FLEA_FREE_BUF_FINAL(cert_buf_2__bu8);
  );
} /* THR_flea_tls__cert_path_validation */
