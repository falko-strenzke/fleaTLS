/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "internal/common/default.h"
#include "flea/error_handling.h"
#include "flea/alloc.h"
#include "flea/array_util.h"
#include "flea/bin_utils.h"
#include "flea/tls.h"
#include "flea/cbc_filter.h"
#include "flea/hash_stream.h"
#include "internal/common/tls/handsh_reader.h"
#include "internal/common/tls/tls_rec_prot_rdr.h"
#include "internal/common/tls/tls_common.h"
#include "flea/rng.h"
#include <stdio.h>
#include "flea/pkcs8.h"
#include "flea/rsa.h"
#include "flea/pk_api.h"
#include "internal/common/tls/parallel_hash.h"
#include "flea/ec_key_gen.h"
#include "flea/byte_vec.h"

#ifdef FLEA_HAVE_TLS

flea_err_t THR_flea_tls__read_client_hello(
  flea_tls_ctx_t*           tls_ctx,
  flea_tls_handsh_reader_t* hs_rdr__pt
)
{
  flea_rw_stream_t* hs_rd_stream__pt;
  flea_u8_t client_version_major_minor__au8[2];
  flea_u8_t session_id_len__u8;

  // TODO: free buf fehlt:
  FLEA_DECL_BUF(session_id__bu8, flea_u8_t, 32);
  const flea_al_u8_t max_session_id_len__alu8 = 32;
  flea_u8_t client_compression_methods_len__u8;
  flea_u16_t cipher_suites_len_from_peer__u16;
  flea_u8_t cipher_suites_len_to_dec__au8[2];
  flea_bool_t found_compression_method;
  flea_bool_t client_presented_sec_reneg_fallback_ciph_suite__b = FLEA_FALSE;

  // flea_u8_t extension_type__au8[2]; // TODO: meaningful representation of extension type
  FLEA_THR_BEG_FUNC();


  if(flea_tls_handsh_reader_t__get_msg_rem_len(hs_rdr__pt) < 34)
  {
    FLEA_THROW("message too short", FLEA_ERR_TLS_GENERIC);
  }

  hs_rd_stream__pt = flea_tls_handsh_reader_t__get_read_stream(hs_rdr__pt);

  // read version
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      client_version_major_minor__au8,
      sizeof(client_version_major_minor__au8)
    )
  );
  // TODO: negotiate version properly
  if(client_version_major_minor__au8[0] != tls_ctx->version.major ||
    client_version_major_minor__au8[1] != tls_ctx->version.minor)
  {
    FLEA_THROW("Version mismatch!", FLEA_ERR_TLS_GENERIC);
  }

  // read random
  // TODO: CHECK HOW TIME IS TO BE USED AND THEN ENCODE IT CORRECTLY
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      tls_ctx->security_parameters.client_random.gmt_unix_time,
      4
    )
  );
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      tls_ctx->security_parameters.client_random.random_bytes,
      28
    )
  );


  // read session id length
  FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &session_id_len__u8));
  if(session_id_len__u8 > max_session_id_len__alu8)
  {
    FLEA_THROW("invalid session id length", FLEA_ERR_TLS_GENERIC);
  }

  // read session id
  FLEA_ALLOC_BUF(session_id__bu8, session_id_len__u8);
  FLEA_CCALL(THR_flea_rw_stream_t__read_full(hs_rd_stream__pt, session_id__bu8, session_id_len__u8));
  // TODO: if != 0: resumption !

  // TODO: stream function to read in the length

  FLEA_CCALL(THR_flea_rw_stream_t__read_full(hs_rd_stream__pt, cipher_suites_len_to_dec__au8, 2));
  cipher_suites_len_from_peer__u16 = flea__decode_U16_BE(cipher_suites_len_to_dec__au8);


  if(cipher_suites_len_from_peer__u16 % 2 != 0)
  {
    FLEA_THROW("incorrect cipher suites length", FLEA_ERR_TLS_GENERIC);
  }

  flea_bool_t found = FLEA_FALSE;
  flea_u16_t supported_cs_len__u16 = tls_ctx->allowed_cipher_suites__prcu16->len__dtl;
  flea_u16_t supported_cs_index__u16;
  flea_u16_t chosen_cs_index__u16 = supported_cs_len__u16;
  while(cipher_suites_len_from_peer__u16)
  {
    flea_u8_t curr_cs__au8[2];
    flea_u16_t curr_cs_from_peer__alu16;
    FLEA_CCALL(THR_flea_rw_stream_t__read_full(hs_rd_stream__pt, curr_cs__au8, 2));
    curr_cs_from_peer__alu16 = curr_cs__au8[0] << 8 | curr_cs__au8[1];
    if(curr_cs_from_peer__alu16 == FLEA_TLS_EMPTY_RENEGOTIATION_INFO_SCSV)
    {
      client_presented_sec_reneg_fallback_ciph_suite__b = FLEA_TRUE;
    }
    // iterate over all supported cipher suites
    supported_cs_index__u16 = 0;
    while(supported_cs_index__u16 < supported_cs_len__u16)
    {
      if(curr_cs_from_peer__alu16 == tls_ctx->allowed_cipher_suites__prcu16->data__pcu16[ supported_cs_index__u16 ])
      {
        if(supported_cs_index__u16 < chosen_cs_index__u16)
        {
          chosen_cs_index__u16 = supported_cs_index__u16;
          tls_ctx->selected_cipher_suite__u16 = curr_cs_from_peer__alu16;
          found = FLEA_TRUE;
          break;
        }
      }
      supported_cs_index__u16 += 1;
    }
    cipher_suites_len_from_peer__u16 -= 2;
  }
  if(found == FLEA_FALSE)
  {
    FLEA_THROW("Could not agree on cipher", FLEA_ERR_TLS_COULD_NOT_AGREE_ON_CIPHERSUITE);
  }

  FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &client_compression_methods_len__u8));

  flea_u8_t curr_cm;
  while(client_compression_methods_len__u8)
  {
    FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &curr_cm));
    if(curr_cm == NO_COMPRESSION)
    {
      found_compression_method = FLEA_TRUE;
      break;
    }
    client_compression_methods_len__u8--;
  }
  if(found_compression_method == FLEA_FALSE)
  {
    FLEA_THROW("Could not agree on compression method", FLEA_ERR_TLS_COULD_NOT_AGREE_ON_CMPR_METH);
  }

  // if there are still bytes left to read, they must be from extensions
  if(flea_tls_handsh_reader_t__get_msg_rem_len(hs_rdr__pt) != 0)
  {
    FLEA_CCALL(THR_flea_tls_ctx_t__client_parse_extensions(tls_ctx, hs_rdr__pt));
# if 0
    flea_al_u16_t all_extensions_len__alu16;
    flea_u8_t byte;
    // read extension length
    // TODO: stream function to read in the length

    FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &byte));
    all_extensions_len__alu16 = byte << 8;
    FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &byte));
    all_extensions_len__alu16 |= byte;

    while(flea_tls_handsh_reader_t__get_msg_rem_len(hs_rdr__pt) > 0)
    {
      // flea_u32_t extension_len__u32;
      FLEA_CCALL(
        THR_flea_rw_stream_t__read_full(
          hs_rd_stream__pt,
          extension_type__au8,
          2
        )
      );
      if(flea_tls_handsh_reader_t__get_msg_rem_len(hs_rdr__pt))
      {
        FLEA_CCALL(THR_flea_tls_ctx_t__client_parse_extensions(tls_ctx, hs_rdr__pt));
        // FLEA_CCALL(THR_flea_rw_stream_t__read_int_be(hs_rd_stream__pt, &extension_len__u32, 2));

        /* FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &byte));
         * extension_len__alu16 = byte << 8;
         * FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, &byte));
         * extension_len__alu16 |= byte;*/

        // TODO: implement handle_extension function that processes the extensions
        //
        // TODO: HANDLING SI-EXT: if client features SR, then set ctx->client_has_sec_reneg__u8 = TRUE
        //                                                  NO, just set_sec_reneg_flag to TRUE
        //                                                  (together with
        //                                                  security tests)
        // FLEA_CCALL(THR_flea_rw_stream_t__skip_read(hs_rd_stream__pt, extension_len__u32));
      }
    }
# endif /* if 0 */
  }
  // check length in the header field for integrity
  if(flea_tls_handsh_reader_t__get_msg_rem_len(hs_rdr__pt) != 0)
  {
    FLEA_THROW("Header length field mismatch", FLEA_ERR_TLS_GENERIC);
  }

  if(client_presented_sec_reneg_fallback_ciph_suite__b)
  {
    tls_ctx->sec_reneg_flag__u8 = FLEA_TRUE;
  }

  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(session_id__bu8);
  );
} /* THR_flea_tls__read_client_hello */

static flea_err_t THR_flea_tls__send_server_hello(
  flea_tls_ctx_t*               tls_ctx,
  flea_tls_parallel_hash_ctx_t* p_hash_ctx
)
{
  flea_u8_t suite__au8[2];

  flea_al_u16_t ext_len__alu16;
  flea_u32_t len = 2 + 32 + 1 + 32 + 2 + 1;

  FLEA_THR_BEG_FUNC();
  ext_len__alu16 = flea_tls_ctx_t__compute_extensions_length(tls_ctx);
  len += ext_len__alu16;
  if(ext_len__alu16)
  {
    /* for encoding of extensions length */
    len += 2;
  }
  // calculate length for the header
  // TODO: include cipher suites length instead of hard coded 2 (5+6th place)
  // TODO: include extensions length (last place)
  // TODO: change 4th element (32) to the real SessionID length

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_hdr(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      HANDSHAKE_TYPE_SERVER_HELLO,
      len
    )
  );

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      &tls_ctx->version.major,
      1
    )
  );
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      &tls_ctx->version.minor,
      1
    )
  );

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      tls_ctx->security_parameters.server_random.gmt_unix_time,
      sizeof(tls_ctx->security_parameters.server_random.gmt_unix_time)
    )
  );
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      tls_ctx->security_parameters.server_random.random_bytes,
      sizeof(tls_ctx->security_parameters.server_random.random_bytes)
    )
  );

  // TODO: actual implementation, e.g. support renegotiation
  flea_u8_t dummy_session_id_len = 32;
  flea_u8_t dummy_session_id[32];
  flea_rng__randomize(dummy_session_id, dummy_session_id_len);


  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      &dummy_session_id_len,
      1
    )
  );
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      dummy_session_id,
      dummy_session_id_len
    )
  );

  suite__au8[0] = tls_ctx->selected_cipher_suite__u16 >> 8;
  suite__au8[1] = tls_ctx->selected_cipher_suite__u16;
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      suite__au8,
      sizeof(suite__au8)
    )
  );

  // We don't support compression
  flea_u8_t null_byte = 0;
  FLEA_CCALL(THR_flea_tls__send_handshake_message_content(&tls_ctx->rec_prot__t, p_hash_ctx, &null_byte, 1));

  FLEA_CCALL(THR_flea_tls_ctx_t__send_extensions_length(tls_ctx, p_hash_ctx));
  if(flea_tls_ctx_t__do_send_sec_reneg_ext(tls_ctx))
  {
    FLEA_CCALL(THR_flea_tls_ctx_t__send_reneg_ext(tls_ctx, p_hash_ctx));
  }

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_tls__send_server_hello */

static flea_err_t THR_flea_tls__send_server_kex(
  flea_tls_ctx_t*               tls_ctx__pt,
  flea_tls_parallel_hash_ctx_t* p_hash_ctx__pt
)
{
  flea_tls__kex_method_t kex_method__t;

  FLEA_DECL_BUF(pub_key__b_u8, flea_u8_t, FLEA_PK_MAX_INTERNAL_FORMAT_PUBKEY_LEN);
  FLEA_DECL_BUF(priv_key__b_u8, flea_u8_t, FLEA_ECC_MAX_ENCODED_POINT_LEN); // only used ECD
  flea_u32_t hdr_len__u32;
  flea_ref_cu8_t pub_point__rcu8;
  flea_pub_key_param_u param__u;
  flea_u8_t ec_curve_type__u8[] = {3}; // named_curve has value 3

  FLEA_DECL_BUF(msg__bu8, flea_u8_t, 256 + 3);                                  // max 256B pub point + 3B for named curve
  FLEA_DECL_BUF(msg_hash__bu8, flea_u8_t, 20);                                  // SHA1 used
  FLEA_DECL_flea_byte_vec_t__CONSTR_HEAP_ALLOCATABLE_OR_STACK(sig_vec__t, 256); // TODO MAX_SIG_SIZE

  FLEA_DECL_OBJ(key__t, flea_private_key_t);

  FLEA_THR_BEG_FUNC();

  kex_method__t = flea_tls_get_kex_method_by_cipher_suite_id(tls_ctx__pt->selected_cipher_suite__u16);
  if(kex_method__t == FLEA_TLS_KEX_ECDHE)
  {
    // set domain parameters
    FLEA_CCALL(THR_flea_ec_gfp_dom_par_ref_t__set_by_builtin_id(&param__u.ecc_dom_par__t, flea_secp256r1));
    // TODO (JR): not fixed!


    flea_al_u8_t priv_key_len__al_u8;
    flea_byte_vec_t scalar_vec__t   = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;
    flea_byte_vec_t pubpoint_vec__t = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;
    flea_al_u8_t pub_key_len__al_u8 = FLEA_ECC_MAX_ENCODED_POINT_LEN;
    priv_key_len__al_u8 = FLEA_ECC_MAX_ORDER_BYTE_SIZE;
    FLEA_ALLOC_BUF(pub_key__b_u8, pub_key_len__al_u8);
    FLEA_ALLOC_BUF(priv_key__b_u8, priv_key_len__al_u8);
    FLEA_CCALL(
      THR_flea_generate_ecc_key(
        pub_key__b_u8,
        &pub_key_len__al_u8,
        priv_key__b_u8,
        &priv_key_len__al_u8,
        &param__u.ecc_dom_par__t
      )
    );

    flea_byte_vec_t__set_ref(&pubpoint_vec__t, pub_key__b_u8, pub_key_len__al_u8);
    flea_byte_vec_t__set_ref(&scalar_vec__t, priv_key__b_u8, priv_key_len__al_u8);

    // generate keys
    FLEA_CCALL(
      THR_flea_private_key_t__ctor_ecc(
        &tls_ctx__pt->ecdhe_priv_key,
        &scalar_vec__t,
        &param__u.ecc_dom_par__t
      )
    );
    FLEA_CCALL(
      THR_flea_public_key_t__ctor_ecc(
        &tls_ctx__pt->ecdhe_pub_key,
        &pubpoint_vec__t,
        &param__u.ecc_dom_par__t
      )
    );

    // send parameters (curve) and public point
    pub_point__rcu8 = flea_public_key__get_encoded_public_component(&tls_ctx__pt->ecdhe_pub_key);

    // TODO: derive signature length from cert
    hdr_len__u32 = 3 + 1 + pub_point__rcu8.len__dtl + 2 + 2 + 256; // 3 for named curve + 1 for pub point length + 2 for sig/hash alg + 2 sig length + 256 for sha256 sig

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_hdr(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        HANDSHAKE_TYPE_SERVER_KEY_EXCHANGE,
        hdr_len__u32
      )
    );

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        ec_curve_type__u8,
        1
      )
    );

    flea_u8_t secp256r1[] = {0, 23}; // TODO: not hardcoded, choose curve
    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        secp256r1,
        2
      )
    );

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        secp256r1,
        2
      )
    );

    flea_u8_t pub_point_len__u8 = pub_point__rcu8.len__dtl;
    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        &pub_point_len__u8,
        1
      )
    );

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        pub_point__rcu8.data__pcu8,
        pub_point__rcu8.len__dtl
      )
    );

    // TODO: not hardcoded
    flea_u8_t sig_and_hash_alg[] = {0x04, 0x01};

    // write server ec params into msg__bu8
    FLEA_ALLOC_BUF(msg__bu8, 3 + pub_point__rcu8.len__dtl);
    memcpy(msg__bu8, ec_curve_type__u8, 1);
    memcpy(msg__bu8 + 1, secp256r1, 2);
    memcpy(msg__bu8 + 3, pub_point__rcu8.data__pcu8, pub_point__rcu8.len__dtl);

    // hash params
    FLEA_ALLOC_BUF(msg_hash__bu8, 20);
    FLEA_CCALL(THR_flea_compute_hash(flea_sha1, msg__bu8, pub_point__rcu8.len__dtl + 3, msg_hash__bu8, 20));


    // create signature
    FLEA_CCALL(
      THR_flea_pk_api__sign_digest(
        msg_hash__bu8,
        20,
        flea_sha256,              // TODO: derive from cert
        flea_rsa_pkcs1_v1_5_sign, // TODO: derive from cert
        &tls_ctx__pt->private_key__t,
        &sig_vec__t
      )
    );

    // send sig_hash alg + sig

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        sig_and_hash_alg,
        2
      )
    );


    flea_u16_t sig_len__u16 = 256; // TODO: derive from cert
    flea_u8_t sig_len_enc__bu8[2];
    flea__encode_U16_BE(sig_len__u16, sig_len_enc__bu8);

    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        sig_len_enc__bu8,
        sizeof(sig_len_enc__bu8)
      )
    );


    FLEA_CCALL(
      THR_flea_tls__send_handshake_message_content(
        &tls_ctx__pt->rec_prot__t,
        p_hash_ctx__pt,
        sig_vec__t.data__pu8,
        sig_vec__t.len__dtl
      )
    );
  }
  else
  {
    FLEA_THROW("KEX not supported", FLEA_ERR_INV_ARG);
  }

  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(pub_key__b_u8);
    FLEA_FREE_BUF_FINAL(priv_key__b_u8);
  );
} /* THR_flea_tls__send_server_kex */

static flea_err_t THR_flea_tls__send_cert_request(
  flea_tls_ctx_t*               tls_ctx,
  flea_tls_parallel_hash_ctx_t* p_hash_ctx
)
{
  // TODO!: change hardcoded values !!!
  flea_u8_t cert_types__au8[1] = {1};
  flea_u8_t sig_algs__au8[2]   = {0x04, 0x01}; // RSA + SHA256
  flea_u8_t sig_algs_len_enc__au8[2];
  flea_u8_t cert_authorities_len_enc__au8[2];
  flea_u8_t cert_types_len__u8         = sizeof(cert_types__au8);
  flea_u16_t sig_algs_len__u16         = sizeof(sig_algs__au8);
  flea_u16_t cert_authorities_len__u16 = 0;

  flea_u32_t hdr_len__u32 = 1 + cert_types_len__u8 + 2 + sig_algs_len__u16 + 2 + cert_authorities_len__u16;

  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_hdr(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      HANDSHAKE_TYPE_CERTIFICATE_REQUEST,
      hdr_len__u32
    )
  );

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      &cert_types_len__u8,
      1
    )
  );

  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      cert_types__au8,
      1
    )
  );

  flea__encode_U16_BE(sig_algs_len__u16, sig_algs_len_enc__au8);
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      sig_algs_len_enc__au8,
      2
    )
  );
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      sig_algs__au8,
      sizeof(sig_algs__au8)
    )
  );

  flea__encode_U16_BE(cert_authorities_len__u16, cert_authorities_len_enc__au8);
  FLEA_CCALL(
    THR_flea_tls__send_handshake_message_content(
      &tls_ctx->rec_prot__t,
      p_hash_ctx,
      cert_authorities_len_enc__au8,
      2
    )
  );
  // FLEA_CCALL(THR_flea_tls__send_handshake_message_content(&tls_ctx->rec_prot__t, hash_ctx, cert_authorities__au8, sizeof(cert_authorities__au8)));

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_tls__send_cert_request */

/*
 * Note: The version number in the PreMasterSecret is the version
 * offered by the client in the ClientHello.client_version, not the
 * version negotiated for the connection.
 *
 */
# ifdef FLEA_HAVE_RSA
static flea_err_t THR_flea_tls__read_client_key_exchange_rsa(
  flea_tls_ctx_t*           tls_ctx,
  flea_tls_handsh_reader_t* hs_rdr__pt,
  flea_ref_cu8_t*           server_key__pt,
  flea_byte_vec_t*          premaster_secret__pt
)
{
  flea_rw_stream_t* hs_rd_stream__pt;
  flea_u16_t enc_premaster_secret_len__u16;

  FLEA_DECL_OBJ(key__t, flea_private_key_t);

  // TODO: FIX HARDCODED LENGTH!
  const flea_u16_t max_enc_premaster_secret_len__u16 = 256;

  // FLEA_DECL_flea_byte_vec_t__CONSTR_HEAP_ALLOCATABLE_OR_STACK(decrypted__t, FLEA_RSA_MAX_MOD_BYTE_LEN);

  // flea_al_u16_t premaster_secret__len_u16 = 256;

  FLEA_DECL_BUF(enc_premaster_secret__bu8, flea_u8_t, 256); // TODO: need more ?
  FLEA_THR_BEG_FUNC();


  hs_rd_stream__pt = flea_tls_handsh_reader_t__get_read_stream(hs_rdr__pt);

  // read encrypted premaster secret length
  // TODO: stream function to read in the length
  FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, ((flea_u8_t*) &enc_premaster_secret_len__u16) + 1));
  FLEA_CCALL(THR_flea_rw_stream_t__read_byte(hs_rd_stream__pt, (flea_u8_t*) &enc_premaster_secret_len__u16));
  if(enc_premaster_secret_len__u16 > max_enc_premaster_secret_len__u16)
  {
    FLEA_THROW("encrypted premaster secret too long", FLEA_ERR_TLS_GENERIC);
  }

  // read encrypted premaster secret
  FLEA_ALLOC_BUF(enc_premaster_secret__bu8, enc_premaster_secret_len__u16);
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      enc_premaster_secret__bu8,
      enc_premaster_secret_len__u16
    )
  );

  // read server key
  // TODO: do one time when initializing TLS?
  FLEA_CCALL(THR_flea_private_key_t__ctor_pkcs8(&key__t, server_key__pt->data__pcu8, server_key__pt->len__dtl));

  FLEA_CCALL(
    THR_flea_pk_api__decrypt_message(
      flea_rsa_pkcs1_v1_5_encr, // TODO: derive from cert
      flea_sha256,              // TODO: derive from cert
      enc_premaster_secret__bu8,
      enc_premaster_secret_len__u16,
      premaster_secret__pt,

      /*tls_ctx->premaster_secret,
       * &premaster_secret__len_u16,*/
      &key__t,
      48
    )
  );


  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(enc_premaster_secret__bu8);
    flea_private_key_t__dtor(&key__t);
  );
} /* THR_flea_tls__read_client_key_exchange_rsa */

# endif /* ifdef FLEA_HAVE_RSA */

static flea_err_t THR_flea_tls__read_client_key_exchange(
  flea_tls_ctx_t*           tls_ctx,
  flea_tls_handsh_reader_t* hs_rdr__pt,
  flea_ref_cu8_t*           server_key__pt,
  flea_byte_vec_t*          premaster_secret__pt
)
{
  FLEA_THR_BEG_FUNC();
# ifdef FLEA_HAVE_RSA
  FLEA_CCALL(THR_flea_tls__read_client_key_exchange_rsa(tls_ctx, hs_rdr__pt, server_key__pt, premaster_secret__pt));
# else
  FLEA_THROW("unsupported key exchange variant", FLEA_ERR_INV_ALGORITHM);
# endif

  FLEA_THR_FIN_SEC_empty();
}

// TODO: define in build cfg or even better: calculate max possible sig size
# define FLEA_MAX_SIG_SIZE 512
static flea_err_t THR_flea_tls__read_cert_verify(
  flea_tls_ctx_t*               tls_ctx,
  flea_tls_handsh_reader_t*     hs_rdr__pt,
  flea_hash_ctx_t*              hash_ctx,
  flea_tls_parallel_hash_ctx_t* p_hash_ctx__pt
)
{
  flea_rw_stream_t* hs_rd_stream__pt;
  flea_u8_t sig_hash_alg__au8[2];
  flea_u8_t sig_len_to_dec__au8[2];
  flea_u16_t sig_len__u16;

  FLEA_DECL_BUF(messages_hash__bu8, flea_u8_t, FLEA_MAX_HASH_OUT_LEN);
  FLEA_DECL_BUF(sig__bu8, flea_u8_t, FLEA_MAX_SIG_SIZE);
  FLEA_DECL_flea_byte_vec_t__CONSTR_HEAP_ALLOCATABLE_OR_STACK(message_vec__t, FLEA_MAX_HASH_OUT_LEN);
  FLEA_DECL_flea_byte_vec_t__CONSTR_HEAP_ALLOCATABLE_OR_STACK(sig_vec__t, FLEA_MAX_SIG_SIZE);
  // TODO: avoid overhead of creating and deleting vectors?

  flea_hash_id_t hash_id__t;
  flea_pk_scheme_id_t pk_scheme_id__t;
  flea_u16_t hash_len__u8;

  FLEA_THR_BEG_FUNC();

  hs_rd_stream__pt = flea_tls_handsh_reader_t__get_read_stream(hs_rdr__pt);

  // read sig and hash algorithm
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      sig_hash_alg__au8,
      2
    )
  );

  // read signature length
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      sig_len_to_dec__au8,
      2
    )
  );
  sig_len__u16 = flea__decode_U16_BE(sig_len_to_dec__au8);

  // read signature
  FLEA_ALLOC_BUF(sig__bu8, sig_len__u16);
  FLEA_CCALL(
    THR_flea_rw_stream_t__read_full(
      hs_rd_stream__pt,
      sig__bu8,
      sig_len__u16
    )
  );

  FLEA_CCALL(flea_tls__get_hash_id_from_tls_id(sig_hash_alg__au8[0], &hash_id__t));
  FLEA_CCALL(flea_tls__get_pk_id_from_tls_sig_id(sig_hash_alg__au8[1], &pk_scheme_id__t));
  hash_len__u8 = flea_hash__get_output_length_by_id(hash_id__t);

  // check if we use the PRF hash function (copy is in hash_ctx) or one of the
  // functions in the p_hash_ctx
  FLEA_ALLOC_BUF(messages_hash__bu8, hash_len__u8);
  if(hash_id__t == flea_hash_ctx_t__get_hash_id(hash_ctx))
  {
    FLEA_CCALL(THR_flea_hash_ctx_t__final(hash_ctx, messages_hash__bu8));
  }
  else
  {
    FLEA_CCALL(THR_flea_tls_parallel_hash_ctx_t__final(p_hash_ctx__pt, hash_id__t, FLEA_FALSE, messages_hash__bu8));
  }

  FLEA_CCALL(
    THR_flea_byte_vec_t__set_content(
      &message_vec__t,
      messages_hash__bu8,
      hash_len__u8
    )
  );
  FLEA_CCALL(
    THR_flea_byte_vec_t__set_content(
      &sig_vec__t,
      sig__bu8,
      sig_len__u16
    )
  );

  FLEA_CCALL(
    THR_flea_pk_api__verify_digest(
      messages_hash__bu8,
      hash_len__u8,
      hash_id__t,
      pk_scheme_id__t,
      &tls_ctx->peer_pubkey,
      sig__bu8,
      sig_len__u16
    )
  );


  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(messages_hash__bu8);
    FLEA_FREE_BUF_FINAL(sig__bu8);
    flea_byte_vec_t__dtor(&message_vec__t);
    flea_byte_vec_t__dtor(&sig_vec__t);
  );
} /* THR_flea_tls__read_cert_verify */

static flea_err_t THR_flea_handle_handsh_msg(
  flea_tls_ctx_t*               tls_ctx,
  flea_tls__handshake_state_t*  handshake_state,
  flea_tls_parallel_hash_ctx_t* p_hash_ctx__pt,
  flea_byte_vec_t*              premaster_secret__pt,
  flea_bool_t                   is_reneg__b
)
{
  FLEA_DECL_OBJ(handsh_rdr__t, flea_tls_handsh_reader_t);
  FLEA_DECL_OBJ(hash_ctx_copy__t, flea_hash_ctx_t);
  flea_hash_id_t hash_id__t;

  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(THR_flea_tls_handsh_reader_t__ctor(&handsh_rdr__t, &tls_ctx->rec_prot__t));
  if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_FINISHED ||
    flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CERTIFICATE_VERIFY)
  {
    /*
     * for read_finished use a copy of hash_ctx where the finished message is not included yet
     * same for certificate verify message but use the appropriate hash function
     * instead of PRF hash
     */
    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CERTIFICATE_VERIFY)
    {
      // stop hashing for all functions but the one for PRF which is the only
      // one we will need in the following messages
      flea_tls_parallel_hash_ctx_t__stop_update_for_all_but_one(
        p_hash_ctx__pt,
        flea_tls_get_prf_hash_by_cipher_suite_id(tls_ctx->selected_cipher_suite__u16)
      );
    }
    hash_id__t = flea_tls_get_prf_hash_by_cipher_suite_id(tls_ctx->selected_cipher_suite__u16);
    FLEA_CCALL(
      THR_flea_tls_parallel_hash_ctx_t__create_hash_ctx_as_copy(
        &hash_ctx_copy__t,
        p_hash_ctx__pt,
        hash_id__t
      )
    );
  }
  FLEA_CCALL(THR_flea_tls_handsh_reader_t__set_hash_ctx(&handsh_rdr__t, p_hash_ctx__pt));


  if(handshake_state->expected_messages == FLEA_TLS_HANDSHAKE_EXPECT_CLIENT_HELLO)
  {
    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CLIENT_HELLO)
    {
      FLEA_CCALL(THR_flea_tls__read_client_hello(tls_ctx, &handsh_rdr__t));
      handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_NONE;
      FLEA_THR_RETURN();
    }
    else if(is_reneg__b)
    {
      FLEA_THROW(
        "server received no_renegotiation alert during renegotiation handshake",
        FLEA_ERR_TLS_REC_NORENEG_AL_DURING_RENEG
      );
    }
    else
    {
      FLEA_THROW("Unexpected message", FLEA_ERR_TLS_GENERIC);
    }
  }

  if(handshake_state->expected_messages == FLEA_TLS_HANDSHAKE_EXPECT_CERTIFICATE)
  {
    handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CLIENT_KEY_EXCHANGE;
    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CERTIFICATE)
    {
      // flea_public_key_t tmp_pubkey;
      // memset(&tmp_pubkey, 0, sizeof(tmp_pubkey)); // TODO: call ctor instead?
      // TODO: DETERMINE KEX TYPE DYNAMICALLY:
      flea_tls_cert_path_params_t cert_path_params__t =
      {.kex_type__e                  = flea_tls_kex__rsa, .client_cert_type__e = flea_tls_cl_cert__rsa_sign,
       .validate_server_or_client__e = FLEA_TLS_CLIENT,
       .hostn_valid_params__pt       = &tls_ctx->hostn_valid_params__t};

      // TODO: also check key usage and other constraints
      FLEA_CCALL(
        THR_flea_tls__read_certificate(
          tls_ctx,
          &handsh_rdr__t,
          &tls_ctx->peer_pubkey,
          &cert_path_params__t
        )
      );
      FLEA_THR_RETURN();
    }
  }
  if(handshake_state->expected_messages == FLEA_TLS_HANDSHAKE_EXPECT_CLIENT_KEY_EXCHANGE)
  {
    if(handshake_state->send_client_cert == FLEA_TRUE)
    {
      handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CERTIFICATE_VERIFY;
    }
    else
    {
      handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CHANGE_CIPHER_SPEC;
    }
    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CLIENT_KEY_EXCHANGE)
    {
      FLEA_CCALL(
        THR_flea_tls__read_client_key_exchange(
          tls_ctx,
          &handsh_rdr__t,
          tls_ctx->private_key__pt,
          premaster_secret__pt
        )
      );
      FLEA_THR_RETURN();
    }
  }

  if(handshake_state->expected_messages == FLEA_TLS_HANDSHAKE_EXPECT_CERTIFICATE_VERIFY)
  {
    handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CHANGE_CIPHER_SPEC;

    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_CERTIFICATE_VERIFY)
    {
      FLEA_CCALL(THR_flea_tls__read_cert_verify(tls_ctx, &handsh_rdr__t, &hash_ctx_copy__t, p_hash_ctx__pt));
      FLEA_THR_RETURN();
    }
  }

  if(handshake_state->expected_messages == FLEA_TLS_HANDSHAKE_EXPECT_FINISHED)
  {
    handshake_state->expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_NONE;
    if(flea_tls_handsh_reader_t__get_handsh_msg_type(&handsh_rdr__t) == HANDSHAKE_TYPE_FINISHED)
    {
      FLEA_CCALL(THR_flea_tls__read_finished(tls_ctx, &handsh_rdr__t, &hash_ctx_copy__t));
      // FLEA_CCALL(THR_flea_tls_handsh_reader_t__set_hash_ctx(&handsh_rdr__t, hash_ctx__pt));

      FLEA_THR_RETURN();
    }
    else
    {
      FLEA_THROW("Expected finished message, but got something else", FLEA_ERR_TLS_GENERIC);
    }
  }

  FLEA_THROW("No handshake message processed", FLEA_ERR_TLS_INVALID_STATE);

  FLEA_THR_FIN_SEC(
    flea_tls_handsh_reader_t__dtor(&handsh_rdr__t);
    flea_hash_ctx_t__dtor(&hash_ctx_copy__t);
  );
} /* THR_flea_handle_handsh_msg */

flea_err_t THR_flea_tls__server_handshake(
  flea_tls_ctx_t* tls_ctx,
  flea_bool_t     is_reneg__b
)
{
  FLEA_THR_BEG_FUNC();

# ifdef FLEA_USE_HEAP_BUF
  flea_byte_vec_t premaster_secret__t = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_ALLOCATABLE;
# else
  flea_u8_t premaster_secret__au8[256]; // TODO: SET CORRECT SIZE LIMIT
  flea_byte_vec_t premaster_secret__t = flea_byte_vec_t__CONSTR_EXISTING_BUF_EMPTY_ALLOCATABLE(
    premaster_secret__au8,
    sizeof(premaster_secret__au8)
    );
# endif

  // define and init state
  flea_tls__handshake_state_t handshake_state;
  flea_tls__handshake_state_ctor(&handshake_state);

  flea_tls_parallel_hash_ctx_t p_hash_ctx;
  flea_hash_id_t hash_ids[] = {flea_sha256, flea_sha1, flea_sha384}; // TODO123: not hardcoded!!!!!

  flea_tls_parallel_hash_ctx_t__INIT(&p_hash_ctx);
  FLEA_CCALL(THR_flea_tls_parallel_hash_ctx_t__ctor(&p_hash_ctx, hash_ids, FLEA_NB_ARRAY_ENTRIES(hash_ids)));


  flea_tls_set_tls_random(tls_ctx);
  // flea_public_key_t pubkey; // TODO: -> tls_ctx


  handshake_state.initialized       = FLEA_TRUE;
  handshake_state.expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CLIENT_HELLO;
  if(tls_ctx->trust_store__pt->nb_set_certs__u16 == 0)
  {
    handshake_state.send_client_cert = FLEA_FALSE;
  }
  else
  {
    handshake_state.send_client_cert = FLEA_TRUE;
  }

  while(handshake_state.finished != FLEA_TRUE)
  {
    /*
     * read next record
     */
    if(handshake_state.expected_messages != FLEA_TLS_HANDSHAKE_EXPECT_NONE)
    {
      ContentType cont_type__e;
      FLEA_CCALL(THR_flea_tls_rec_prot_t__get_current_record_type(&tls_ctx->rec_prot__t, &cont_type__e));

      // TODO: record type argument has to be removed because it's determined by the current connection state in tls_ctx
      if(cont_type__e == CONTENT_TYPE_HANDSHAKE)
      {
        FLEA_CCALL(
          THR_flea_handle_handsh_msg(
            tls_ctx,
            &handshake_state,
            &p_hash_ctx,
            &premaster_secret__t,
            is_reneg__b
          )
        );
      }
      else if(cont_type__e == CONTENT_TYPE_CHANGE_CIPHER_SPEC)
      {
        if(!(handshake_state.expected_messages & FLEA_TLS_HANDSHAKE_EXPECT_CHANGE_CIPHER_SPEC))
        {
          FLEA_THROW("Received unexpected message", FLEA_ERR_TLS_GENERIC);
        }
        else
        {
          flea_u8_t dummy_byte;
          flea_al_u16_t len_one__alu16 = 1;
          flea_al_u8_t key_block_len__alu8;

          FLEA_CCALL(
            THR_flea_tls_rec_prot_t__read_data(
              &tls_ctx->rec_prot__t,
              CONTENT_TYPE_CHANGE_CIPHER_SPEC,
              &dummy_byte,
              &len_one__alu16,
              flea_read_full
            )
          );

          /*
           * Enable encryption for incoming messages
           */
          // setup key material
          FLEA_CCALL(
            THR_flea_tls__create_master_secret(
              tls_ctx->security_parameters.client_random,
              tls_ctx->security_parameters.server_random,
              &premaster_secret__t,
              // tls_ctx->premaster_secret,
              tls_ctx->security_parameters.master_secret,
              tls_ctx->selected_cipher_suite__u16
            )
          );
          FLEA_CCALL(
            THR_flea_tls_get_key_block_len_from_cipher_suite_id(
              tls_ctx->selected_cipher_suite__u16,
              &key_block_len__alu8
            )
          );
          // TODO: key block need not be a member => make local
          FLEA_CCALL(
            THR_flea_tls__generate_key_block(
              // &tls_ctx->security_parameters,
              tls_ctx,
              tls_ctx->key_block,
              key_block_len__alu8
            )
          );

          // enable encryption for read direction
          FLEA_CCALL(
            THR_flea_tls_rec_prot_t__set_ciphersuite(
              &tls_ctx->rec_prot__t,
              flea_tls_read,
              FLEA_TLS_SERVER,
              tls_ctx->selected_cipher_suite__u16,
              tls_ctx->key_block
            )
          );


          handshake_state.expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_FINISHED;

          continue;
        }
      }
      else if(cont_type__e == CONTENT_TYPE_ALERT)
      {
        // TODO: handle alert message properly | UPDATE (Falko): should be
        // unneccessary now, can be removed
        FLEA_THROW("Received unhandled alert", FLEA_ERR_TLS_GENERIC);
      }
      else
      {
        FLEA_THROW("Received unexpected message", FLEA_ERR_TLS_GENERIC);
      }
    }
    // We don't expect another message so it's our turn to continue
    else
    {
      if(handshake_state.sent_first_round == FLEA_FALSE)
      {
        FLEA_CCALL(THR_flea_tls__send_server_hello(tls_ctx, &p_hash_ctx));

        FLEA_CCALL(
          THR_flea_tls__send_certificate(
            tls_ctx,
            &p_hash_ctx,
            tls_ctx->cert_chain__pt,
            tls_ctx->cert_chain_len__u8
          )
        );

        // send certificate request in case we want client authentication
        if(handshake_state.send_client_cert == FLEA_TRUE)
        {
          FLEA_CCALL(
            THR_flea_tls__send_cert_request(
              tls_ctx,
              &p_hash_ctx
            )
          );
        }

        // send server key exchange depending on cipher suite
        if(flea_tls_get_kex_method_by_cipher_suite_id(tls_ctx->selected_cipher_suite__u16) == FLEA_TLS_KEX_ECDHE)
        {
          FLEA_CCALL(THR_flea_tls__send_server_kex(tls_ctx, &p_hash_ctx));
        }

        FLEA_CCALL(
          THR_flea_tls__send_handshake_message(
            &tls_ctx->rec_prot__t,
            &p_hash_ctx,
            HANDSHAKE_TYPE_SERVER_HELLO_DONE,
            (flea_u8_t*) NULL,
            0
          )
        );

        handshake_state.sent_first_round = FLEA_TRUE;

        if(handshake_state.send_client_cert == FLEA_TRUE)
        {
          handshake_state.expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CERTIFICATE;
        }
        else
        {
          handshake_state.expected_messages = FLEA_TLS_HANDSHAKE_EXPECT_CLIENT_KEY_EXCHANGE;
        }
      }
      else
      {
        FLEA_CCALL(THR_flea_tls__send_change_cipher_spec(tls_ctx));

        FLEA_CCALL(
          THR_flea_tls_rec_prot_t__set_ciphersuite(
            &tls_ctx->rec_prot__t,
            flea_tls_write,
            FLEA_TLS_SERVER,
            tls_ctx->selected_cipher_suite__u16,
            tls_ctx->key_block
          )
        );

        FLEA_CCALL(THR_flea_tls__send_finished(tls_ctx, &p_hash_ctx));

        handshake_state.finished = FLEA_TRUE;
        break;
      }

      continue;
    }
  }
  FLEA_THR_FIN_SEC(
    flea_tls_parallel_hash_ctx_t__dtor(&p_hash_ctx);
    flea_byte_vec_t__dtor(&premaster_secret__t);
  );
} /* THR_flea_tls__server_handshake */

flea_err_t THR_flea_tls_ctx_t__ctor_server(
  flea_tls_ctx_t*          tls_ctx__pt,
  flea_rw_stream_t*        rw_stream__pt,
  flea_ref_cu8_t*          cert_chain__pt,
  flea_al_u8_t             cert_chain_len__alu8,
  const flea_cert_store_t* trust_store__pt,
  flea_ref_cu8_t*          server_key__pt,
  const flea_ref_cu16_t*   allowed_cipher_suites__prcu16,
  flea_rev_chk_mode_e      rev_chk_mode__e,
  const flea_byte_vec_t*   crl_der__pt,
  flea_al_u16_t            nb_crls__alu16
)
{
  flea_err_t err__t;

  FLEA_THR_BEG_FUNC();
  tls_ctx__pt->rev_chk_cfg__t.rev_chk_mode__e = rev_chk_mode__e;
  tls_ctx__pt->rev_chk_cfg__t.nb_crls__u16    = nb_crls__alu16;
  tls_ctx__pt->rev_chk_cfg__t.crl_der__pt     = crl_der__pt;
  tls_ctx__pt->cert_chain__pt     = cert_chain__pt;
  tls_ctx__pt->cert_chain_len__u8 = cert_chain_len__alu8;
  tls_ctx__pt->private_key__pt    = server_key__pt;
  tls_ctx__pt->trust_store__pt    = trust_store__pt;
  tls_ctx__pt->allowed_cipher_suites__prcu16      = allowed_cipher_suites__prcu16;
  tls_ctx__pt->security_parameters.connection_end = FLEA_TLS_SERVER;
  FLEA_CCALL(THR_flea_tls_ctx_t__construction_helper(tls_ctx__pt, rw_stream__pt, NULL, 0));
  err__t = THR_flea_tls__server_handshake(tls_ctx__pt, FLEA_FALSE);
  FLEA_CCALL(THR_flea_tls__handle_tls_error(&tls_ctx__pt->rec_prot__t, err__t));
  FLEA_THR_FIN_SEC_empty();
}

#endif /* ifdef FLEA_HAVE_TLS */
