/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */


#include "internal/common/default.h"
#include "internal/common/tls/tls_rec_prot.h"
#include "internal/common/tls/tls_ciph_suite.h"
#include "flea/error_handling.h"
#include "flea/bin_utils.h"
#include "flea/error.h"
#include "flea/alloc.h"
#include "flea/rng.h"
#include "flea/util.h"
#include "flea/tls.h"
#include "internal/common/tls/tls_common.h"
#include "internal/common/mask.h"
#include "internal/common/tls/tls_int.h"
#ifdef FLEA_HAVE_DTLS
# include "qheap/queue_heap.h"
#endif

// TODO: ANY CALCULATIONS FOR MAXIMAL SIZES MUST USE DTLS HDR-LEN IS CONFIGURED
// #define DBG_PRINT

#ifdef FLEA_HAVE_TLS


// # define FLEA_RP__IS_DTLS(rec_prot__pt) ((rec_prot__pt)->is_dtls_active__u8)


static void inc_seq_nbr(flea_u32_t* seq__au32)
{
  seq__au32[0]++;
  if(seq__au32[0] == 0)
  {
    seq__au32[1]++;
  }
}

# ifdef FLEA_HAVE_TLS_CS_CBC
static flea_err_e THR_flea_recprot_t__compute_mac_cbc_hmac(
  const flea_u8_t*    rec_hdr__pcu8,
  flea_tls_con_stt_t* conn_state__pt,
  flea_u8_t*          data,
  flea_u32_t          data_len,
  flea_u8_t*          mac_out,
  // flea_u16_t          epoch_u16,
  const flea_u8_t*    seq__pcu8
)
{
  flea_mac_ctx_t mac__t;
  flea_al_u8_t mac_len_out_alu8;
  flea_u8_t enc_len__au8[2];
  // flea_u8_t enc_seq_nbr__au8[8];
  // flea_u32_t seq_lo__u32, seq_hi__u32;

  flea_u8_t* mac_key    = conn_state__pt->suite_specific__u.cbc_hmac_conn_state__t.mac_key__bu8;
  flea_u8_t mac_len     = conn_state__pt->cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.mac_size__u8;
  flea_u8_t mac_key_len = conn_state__pt->cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.mac_key_size__u8;

  FLEA_THR_BEG_FUNC();

  flea_mac_ctx_t__INIT(&mac__t);

  /*
   * MAC(MAC_write_key, seq_num +
   *                      TLSCompressed.type +
   *                      TLSCompressed.version +
   *                      TLSCompressed.length +
   *                      TLSCompressed.fragment);
   */
  // 8 + 1 + (1+1) + 2 + length

  // TODO: make a function to set the encoded sequence number

  /*seq_lo__u32 = conn_state__pt->seqno_lo_hi__au32[0];
  seq_hi__u32 = conn_state__pt->seqno_lo_hi__au32[1];*/

  /* set the DTLS epcoh in the high u32 */

  /*if(epoch_u16)
  {
    FLEA_DBG_PRINTF("setting epoch for CBC ciphersuite's compute HMAC: %u\n", epoch_u16);
    seq_hi__u32 |= (epoch_u16 << 16);
  }
  flea__encode_U32_BE(seq_hi__u32, enc_seq_nbr__au8);
  flea__encode_U32_BE(seq_lo__u32, enc_seq_nbr__au8 + 4);*/

/*
  FLEA_DBG_PRINTF("cbc compute HMAC: encoded epoch | seq = ");
  for(unsigned i = 0; i < 8; i++)
  {
    FLEA_DBG_PRINTF("%02x ", enc_seq_nbr__au8[i]);
  }
  FLEA_DBG_PRINTF("\n");
  */

  FLEA_CCALL(
    THR_flea_mac_ctx_t__ctor(
      &mac__t,
      conn_state__pt->cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.mac_id,
      mac_key,
      mac_key_len
    )
  );
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&mac__t, seq__pcu8, 8));
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&mac__t, rec_hdr__pcu8, 3));

  enc_len__au8[0] = data_len >> 8;
  enc_len__au8[1] = data_len;
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&mac__t, enc_len__au8, sizeof(enc_len__au8)));
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&mac__t, data, data_len));

  mac_len_out_alu8 = mac_len;

  FLEA_CCALL(THR_flea_mac_ctx_t__final_compute(&mac__t, mac_out, &mac_len_out_alu8));

  FLEA_THR_FIN_SEC(
    flea_mac_ctx_t__dtor(&mac__t);
  );
} /* THR_flea_recprot_t__compute_mac_cbc_hmac */

# endif /* ifdef FLEA_HAVE_HMAC */

static void flea_recprot_t__discard_pending_write(flea_recprot_t* rec_prot__pt)
{
  // rec_prot__pt->write_ongoing__u8          = 0;
  FLEA_RP__SET_NO_WRITE_ONGOING(rec_prot__pt);
  rec_prot__pt->send_curr_rec_content_offs__u16 = 0;
  rec_prot__pt->send_curr_rec_content_len__u16  = 0;
}

static flea_err_e THR_flea_recprot_t__close_with_fatal_alert_and_throw(
  flea_recprot_t*               rec_prot__pt,
  flea_tls__alert_description_t alert_desc__e,
  flea_err_e                    error__e
)
{
  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(
    THR_flea_recprot_t__send_alert(
      rec_prot__pt,
      alert_desc__e,
      FLEA_TLS_ALERT_LEVEL_FATAL
    )
  );
  FLEA_RP__SET_SESSION_CLOSED(rec_prot__pt);
  FLEA_THROW("closing session with fatal alert", error__e);
  FLEA_THR_FIN_SEC_empty();
}

/* potentially sends alerts and throws if the received alert indicates this */
static flea_err_e THR_flea_recprot_t__handle_alert(
  flea_recprot_t* rec_prot__pt,
  flea_dtl_t      read_bytes_count__dtl
)
{
  FLEA_THR_BEG_FUNC();
  if(rec_prot__pt->curr_pt_content_len__u16 != 2)
  {
    FLEA_CCALL(
      THR_flea_recprot_t__close_with_fatal_alert_and_throw(
        rec_prot__pt,
        FLEA_TLS_ALERT_DESC_DECODE_ERROR,
        FLEA_ERR_TLS_INV_REC
      )
    );
  }

  // rec_prot__pt->curr_rec_content_offs__u16   = 0;
  rec_prot__pt->curr_rec_content_offs__u16 += 2;
  // rec_prot__pt->curr_rec_content_len__u16 = 0;
  if(rec_prot__pt->payload_buf__pu8[0] == FLEA_TLS_ALERT_LEVEL_FATAL)
  {
    FLEA_RP__SET_SESSION_CLOSED(rec_prot__pt);
    FLEA_THROW("received fatal alert", FLEA_ERR_TLS_REC_FATAL_ALERT);
  }

  else if(rec_prot__pt->payload_buf__pu8[1] == FLEA_TLS_ALERT_DESC_NO_RENEGOTIATION)
  {
    FLEA_THROW("received no renegotiation alert", FLEA_ERR_TLS_REC_NORENEG_AL_DURING_RENEG);
  }

  /*rec_prot__pt->curr_rec_content_offs__u16   = 0;
    rec_prot__pt->curr_rec_content_len__u16 = 0;*/
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__handle_alert */

# ifdef FLEA_HAVE_DTLS
static void flea_recprot_t__set_current_rd_rec_empty(flea_recprot_t* rec_prot__pt)
{
  rec_prot__pt->curr_rec_content_offs__u16 = 0;
  rec_prot__pt->curr_rec_content_len__u16  = 0;
  rec_prot__pt->curr_pt_content_len__u16   = 0;
}

#  if 0
void flea_recprot_t__extract_encrypted_record(
  flea_recprot_t*     rec_prot__pt,
  qheap_queue_heap_t* heap__pt,
  qh_al_hndl_t        hndl
)
{
  qheap_qh_append_to_queue(
    heap__pt,
    hndl,
    rec_prot__pt->send_rec_buf_raw__bu8,
    rec_prot__pt->curr_rec_content_len__u16 + FLEA_DTLS_RECORD_HDR_LEN
  );
  FLEA_RP__SET_NO_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt);
}

#  endif /* if 0 */

# endif /* ifdef FLEA_HAVE_DTLS */
flea_err_e THR_flea_recprot_t__ctor(
  flea_recprot_t*   rec_prot__pt,
  flea_al_u8_t      prot_vers_major,   // TODO: turn into u16?
  flea_al_u8_t      prot_vers_minor,
  flea_rw_stream_t* rw_stream__pt,
  flea_bool_t       is_dtls__b
)
{
  FLEA_THR_BEG_FUNC();

# ifdef FLEA_HEAP_MODE
  FLEA_ALLOC_MEM_ARR(rec_prot__pt->send_rec_buf_raw__bu8, FLEA_TLS_TRNSF_BUF_SIZE);
  FLEA_ALLOC_MEM_ARR(rec_prot__pt->alt_send_buf__raw__bu8, FLEA_TLS_ALT_SEND_BUF_SIZE);
# endif

  rec_prot__pt->record_hdr_len__u8 = FLEA_TLS_RECORD_HDR_LEN;
  if(is_dtls__b)
  {
    rec_prot__pt->record_hdr_len__u8 = FLEA_DTLS_RECORD_HDR_LEN;
    FLEA_RP__SET_DTLS(rec_prot__pt);
  }
  rec_prot__pt->alt_send_buf__raw_len__u16 = FLEA_TLS_ALT_SEND_BUF_SIZE;
  rec_prot__pt->send_rec_buf_raw_len__u16  = FLEA_TLS_TRNSF_BUF_SIZE;
  // TODO: THIS SHOULD BE CALCULATED DYNAMICALLY AS THE MIN(buffer capacity, allowed
  // pt size) for each cipher suite
  rec_prot__pt->record_plaintext_send_max_value__u16 = FLEA_TLS_RECORD_MAX_SEND_PLAINTEXT_SIZE;
  rec_prot__pt->prot_version__t.major      = prot_vers_major;
  rec_prot__pt->prot_version__t.minor      = prot_vers_minor;
  rec_prot__pt->rw_stream__pt              = rw_stream__pt;
  rec_prot__pt->curr_rec_content_offs__u16 = 0;
  rec_prot__pt->raw_read_buf_content__u16  = 0;

  rec_prot__pt->curr_rec_content_len__u16 = 0;

  flea_recprot_t__set_null_ciphersuite(rec_prot__pt, flea_tls_write);
  flea_recprot_t__set_null_ciphersuite(rec_prot__pt, flea_tls_read);
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__ctor */

void flea_recprot_t__set_null_ciphersuite(
  flea_recprot_t*       rec_prot__pt,
  flea_tls_stream_dir_e direction
)
{
  // rec_prot__pt->payload_max_len__u16     = rec_prot__pt->send_rec_buf_raw_len__u16 - rec_prot__pt->record_hdr_len__u8;
  // TODO: THIS LOOKS BAD (REMOVE ALL SEND BUFFER RELATED LENGTHS AND KEEP ONLY
  // THE MAX SEND_PT_LEN):
  // rec_prot__pt->alt_payload_max_len__u16 = rec_prot__pt->alt_send_buf__raw_len__u16 - rec_prot__pt->record_hdr_len__u8;

  if(direction == flea_tls_write)
  {
    flea_tls_con_stt_t__dtor(&rec_prot__pt->write_state__t);
    flea_tls_con_stt_t__ctor_no_cipher(&rec_prot__pt->write_state__t);
  }
  else
  {
    flea_tls_con_stt_t__dtor(&rec_prot__pt->read_state__t);
    flea_tls_con_stt_t__ctor_no_cipher(&rec_prot__pt->read_state__t);
  }
}

# ifdef FLEA_HAVE_TLS_CS_CBC
static flea_err_e THR_flea_recprot_t__set_cbc_cs_inner(
  flea_recprot_t*        rec_prot__pt,
  flea_tls_stream_dir_e  direction,
  flea_block_cipher_id_e block_cipher_id,
  flea_mac_id_e          mac_id,
  const flea_u8_t*       cipher_key__pcu8,
  flea_al_u8_t           cipher_key_len__alu8,
  const flea_u8_t*       mac_key__pcu8,
  flea_al_u8_t           mac_key_len__alu8,
  flea_al_u8_t           mac_size__alu8
)
{
  flea_tls_con_stt_t* conn_state__pt;
  flea_al_u16_t reserved_payl_len__alu16;
  flea_al_u16_t new_epoch__alu16 = 0;

  FLEA_THR_BEG_FUNC();
  if(direction == flea_tls_write)
  {
    conn_state__pt = &rec_prot__pt->write_state__t;
  }
  else
  {
    conn_state__pt = &rec_prot__pt->read_state__t;
  }
#  ifdef FLEA_HAVE_DTLS
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    new_epoch__alu16 = conn_state__pt->epoch__u16 + 1;
    FLEA_DBG_PRINTF("new epoch for new cbc conn (dir = %u) = %u\n", direction, new_epoch__alu16);
  }
#  endif /* ifdef FLEA_HAVE_DTLS */
  flea_tls_con_stt_t__dtor(conn_state__pt);
  FLEA_CCALL(
    THR_flea_tls_con_stt_t__ctor_cbc_hmac(
      conn_state__pt,
      block_cipher_id,
      mac_id,
      cipher_key__pcu8,
      cipher_key_len__alu8,
      mac_key__pcu8,
      mac_key_len__alu8,
      mac_size__alu8,
      new_epoch__alu16
    )
  );
  FLEA_DBG_PRINTF("new epoch cbc conn after cbc conn ctor = %u\n", conn_state__pt->epoch__u16);

  // reserved_payl_len__alu16 = mac_size__alu8 + 2 * rec_prot__pt->read_state__t.reserved_iv_len__u8; /* 2* block size: one for IV, one for padding */
  reserved_payl_len__alu16 = mac_size__alu8 + 2 * conn_state__pt->reserved_iv_len__u8; /* 2* block size: one for IV, one for padding */

  if(((reserved_payl_len__alu16 + rec_prot__pt->record_hdr_len__u8) >= rec_prot__pt->send_rec_buf_raw_len__u16) ||
    ((reserved_payl_len__alu16 + rec_prot__pt->record_hdr_len__u8) >= rec_prot__pt->alt_send_buf__raw_len__u16))
  {
    FLEA_THROW("send/receive buffer is too small", FLEA_ERR_BUFF_TOO_SMALL);
  }

  // rec_prot__pt->payload_max_len__u16 = rec_prot__pt->send_rec_buf_raw_len__u16 - rec_prot__pt->record_hdr_len__u8 - reserved_payl_len__alu16;
  rec_prot__pt->reserved_payl_len__u16 = reserved_payl_len__alu16;

  // rec_prot__pt->alt_payload_max_len__u16 = rec_prot__pt->record_plaintext_send_max_value__u16;
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__set_cbc_cs_inner */

static flea_err_e THR_flea_recprot_t__set_cbc_cs(
  flea_recprot_t*                 rec_prot__pt,
  flea_tls_stream_dir_e           direction,
  flea_tls__connection_end_t      conn_end__e,
  const flea_tls__cipher_suite_t* suite__pt,
  const flea_u8_t*                key_block__pcu8
)
{
  FLEA_THR_BEG_FUNC();
  flea_al_u8_t mac_key_len__alu8, mac_len__alu8, cipher_key_len__alu8;
  flea_al_u8_t mac_key_offs__alu8 = 0, ciph_key_offs__alu8 = 0;

  /*FLEA_CCALL(THR_flea_tls_get_cipher_suite_by_id(suite_id, &suite__pt));
   * if(suite__pt == NULL)
   * {
   * FLEA_THROW("invalid ciphersuite selected", FLEA_ERR_INT_ERR);
   * }*/
  mac_key_len__alu8    = suite__pt->mac_key_size;
  mac_len__alu8        = suite__pt->mac_size;
  cipher_key_len__alu8 = suite__pt->enc_key_size;
  if((direction == flea_tls_write && conn_end__e == FLEA_TLS_SERVER) ||
    (direction == flea_tls_read && conn_end__e == FLEA_TLS_CLIENT)
  )
  {
    mac_key_offs__alu8  = mac_key_len__alu8;
    ciph_key_offs__alu8 = cipher_key_len__alu8;
  }

  FLEA_CCALL(
    THR_flea_recprot_t__set_cbc_cs_inner(
      rec_prot__pt,
      direction,
      FLEA_TLS_CIPHER_RAW_ID(suite__pt->cipher),
      flea_tls__map_hmac_to_hash(suite__pt->hash_algorithm),
      key_block__pcu8 + 2 * mac_key_len__alu8 + ciph_key_offs__alu8,
      cipher_key_len__alu8,
      key_block__pcu8 + mac_key_offs__alu8,
      mac_key_len__alu8,
      mac_len__alu8
    )
  );

  FLEA_DBG_PRINTF("setting mac key for ");
  if(direction == flea_tls_read)
  {
    FLEA_DBG_PRINTF("incoming");
  }
  else
  {
    FLEA_DBG_PRINTF("outgoing");
  }

  FLEA_DBG_PRINTF(" direction = ");
  FLEA_DBG_PRINT_BYTE_ARRAY(key_block__pcu8 + mac_key_offs__alu8, mac_key_len__alu8);
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__set_cbc_cs */

# endif /* ifdef FLEA_HAVE_TLS_CS_CBC */

# ifdef FLEA_HAVE_TLS_CS_GCM
static flea_err_e THR_flea_recprot_t__set_gcm_cs_inner(
  flea_recprot_t*       rec_prot__pt,
  flea_tls_stream_dir_e direction,
  flea_ae_id_e          ae_cipher_id,
  const flea_u8_t*      cipher_key__pcu8,
  flea_al_u8_t          cipher_key_len__alu8,
  const flea_u8_t*      fixed_iv__pcu8,
  flea_al_u8_t          fixed_iv_len__alu8
)
{
  flea_tls_con_stt_t* conn_state__pt;
  flea_al_u16_t reserved_payl_len__alu16;
  flea_al_u16_t new_epoch__alu16 = 0;

  FLEA_THR_BEG_FUNC();

  if(direction == flea_tls_write)
  {
    conn_state__pt = &rec_prot__pt->write_state__t;
  }
  else
  {
    conn_state__pt = &rec_prot__pt->read_state__t;
  }

#  ifdef FLEA_HAVE_DTLS
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    new_epoch__alu16 = conn_state__pt->epoch__u16 + 1;
    FLEA_DBG_PRINTF("new epoch for new gcm conn = %u\n", new_epoch__alu16);
  }
#  endif /* ifdef FLEA_HAVE_DTLS */
  flea_tls_con_stt_t__dtor(conn_state__pt);
  FLEA_CCALL(
    THR_flea_tls_con_stt_t__ctor_gcm(
      conn_state__pt,
      ae_cipher_id,
      cipher_key__pcu8,
      cipher_key_len__alu8,
      fixed_iv__pcu8,
      fixed_iv_len__alu8,
      new_epoch__alu16
    )
  );

  /* 16 is the GCM tag length */
  // TODO: ERROR, NEED TO USE CONN STATE, NOT READ STATE!:
  // reserved_payl_len__alu16 = 16 + rec_prot__pt->read_state__t.reserved_iv_len__u8;
  reserved_payl_len__alu16 = 16 + conn_state__pt->reserved_iv_len__u8;
// TODO: FACTOR THIS OUT, COMPARE CBC
  if(((reserved_payl_len__alu16 + rec_prot__pt->record_hdr_len__u8) > rec_prot__pt->send_rec_buf_raw_len__u16) ||
    ((reserved_payl_len__alu16 + rec_prot__pt->record_hdr_len__u8) > rec_prot__pt->alt_send_buf__raw_len__u16))
  {
    FLEA_THROW("send/receive buffer is too small", FLEA_ERR_BUFF_TOO_SMALL);
  }

  // rec_prot__pt->payload_max_len__u16 = rec_prot__pt->send_rec_buf_raw_len__u16 - rec_prot__pt->record_hdr_len__u8 - reserved_payl_len__alu16;

  rec_prot__pt->reserved_payl_len__u16 = reserved_payl_len__alu16;
  // rec_prot__pt->alt_payload_max_len__u16 = rec_prot__pt->record_plaintext_send_max_value__u16;
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__set_gcm_cs_inner */

static flea_err_e THR_flea_recprot_t__set_gcm_cs(
  flea_recprot_t*                 rec_prot__pt,
  flea_tls_stream_dir_e           direction,
  flea_tls__connection_end_t      conn_end__e,
  const flea_tls__cipher_suite_t* suite__pt,
  const flea_u8_t*                key_block__pcu8
)
{
  FLEA_THR_BEG_FUNC();
  flea_al_u8_t fixed_iv_len__alu8, cipher_key_len__alu8;
  flea_al_u8_t fixed_iv_offs__alu8 = 0, ciph_key_offs__alu8 = 0;
  if(suite__pt == NULL)
  {
    FLEA_THROW("invalid ciphersuite selected", FLEA_ERR_INT_ERR);
  }
  fixed_iv_len__alu8   = FLEA_CONST_TLS_GCM_FIXED_IV_LEN;
  cipher_key_len__alu8 = suite__pt->enc_key_size;
  if((direction == flea_tls_write && conn_end__e == FLEA_TLS_SERVER) ||
    (direction == flea_tls_read && conn_end__e == FLEA_TLS_CLIENT)
  )
  {
    fixed_iv_offs__alu8 = fixed_iv_len__alu8;
    ciph_key_offs__alu8 = cipher_key_len__alu8;
  }

  FLEA_CCALL(
    THR_flea_recprot_t__set_gcm_cs_inner(
      rec_prot__pt,
      direction,
      FLEA_TLS_CIPHER_RAW_ID(suite__pt->cipher),
      key_block__pcu8 + ciph_key_offs__alu8,
      cipher_key_len__alu8,
      key_block__pcu8 + 2 * cipher_key_len__alu8 + fixed_iv_offs__alu8,
      fixed_iv_len__alu8   // 4
    )
  );
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__set_gcm_cs */

# endif /* ifdef FLEA_HAVE_TLS_CS_GCM */

# ifdef FLEA_HAVE_DTLS
flea_err_e THR_flea_recprot_t__set_dtls_conn_state_and_epoch_and_sqn_in_write_conn(
  flea_recprot_t*                    rec_prot__pt,
  const flea_dtls_conn_state_data_t* conn_state_data__pt,
  flea_tls__connection_end_t         conn_end__e
)
{
  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(
    THR_flea_recprot_t__set_ciphersuite(
      rec_prot__pt,
      flea_tls_write,
      conn_end__e,
      conn_state_data__pt->cipher_suite_id,
      flea_byte_vec_t__GET_DATA_PTR(&conn_state_data__pt->write_key_block__t)
    )
  );
  /* set the previous seq and epoch */
  rec_prot__pt->write_state__t.epoch__u16 = conn_state_data__pt->write_epoch__u16;
  memcpy(
    rec_prot__pt->write_state__t.seqno_lo_hi__au32,
    conn_state_data__pt->write_sqn__au32,
    sizeof(conn_state_data__pt->write_sqn__au32)
  );
  FLEA_THR_FIN_SEC_empty();
}

# endif /* ifdef FLEA_HAVE_DTLS */

flea_err_e THR_flea_recprot_t__set_ciphersuite(
  flea_recprot_t*            rec_prot__pt,
  flea_tls_stream_dir_e      direction,
  flea_tls__connection_end_t conn_end__e,
  flea_tls_cipher_suite_id_t suite_id,
  const flea_u8_t*           key_block__pcu8
)
{
  const flea_tls__cipher_suite_t* suite__pt;

  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));

# ifdef FLEA_HAVE_DTLS
  /* DTLS must be able to revert to null cs for retransmission */
  if(suite_id == 0)
  {
    flea_recprot_t__set_null_ciphersuite(rec_prot__pt, direction);
    FLEA_THR_RETURN();
  }
# endif /* ifdef FLEA_HAVE_DTLS */

  FLEA_CCALL(THR_flea_tls_get_cipher_suite_by_id(suite_id, &suite__pt));


# ifdef FLEA_HAVE_TLS_CS_GCM
  if(FLEA_TLS_IS_AE_CIPHER(suite__pt->cipher))
  {
    FLEA_CCALL(
      THR_flea_recprot_t__set_gcm_cs(
        rec_prot__pt,
        direction,
        conn_end__e,
        suite__pt,
        key_block__pcu8
      )
    );
  }
  else
# endif /* ifdef FLEA_HAVE_TLS_CS_GCM */
# ifdef FLEA_HAVE_TLS_CS_CBC
  {
    FLEA_CCALL(
      THR_flea_recprot_t__set_cbc_cs(
        rec_prot__pt,
        direction,
        conn_end__e,
        suite__pt,
        key_block__pcu8
      )
    );
  }
# endif /* ifdef FLEA_HAVE_TLS_CS_CBC */
  { }
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__set_ciphersuite */

static void flea_recprot_t__set_record_header(
  flea_recprot_t*          rec_prot__pt,
  flea_tls_rec_cont_type_e content_type__e
)
{
  rec_prot__pt->send_buf_raw__pu8[0] = content_type__e;
  rec_prot__pt->send_buf_raw__pu8[1] = rec_prot__pt->prot_version__t.major;
  rec_prot__pt->send_buf_raw__pu8[2] = rec_prot__pt->prot_version__t.minor;
# ifdef FLEA_HAVE_DTLS
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    // flea_al_s8_t i;
    // flea_u32_t low__u32 = rec_prot__pt->write_state__t.seqno_lo_hi__au32[1];

    // TODO: CONSIDER WRITING THE EPOCH INTO THE SEQ EVERYTIME IT CHANGES, THIS
    // WOULD SIMPLIFY THE CODE HERE
    rec_prot__pt->send_buf_raw__pu8[3] = rec_prot__pt->write_state__t.epoch__u16 >> 8;
    rec_prot__pt->send_buf_raw__pu8[4] = rec_prot__pt->write_state__t.epoch__u16;
    rec_prot__pt->send_buf_raw__pu8[5] = rec_prot__pt->write_state__t.seqno_lo_hi__au32[1] >> 8;
    rec_prot__pt->send_buf_raw__pu8[6] = rec_prot__pt->write_state__t.seqno_lo_hi__au32[1];
    flea__encode_U32_BE(rec_prot__pt->write_state__t.seqno_lo_hi__au32[0], &rec_prot__pt->send_buf_raw__pu8[7]);

    /*for(i = 3; i >= 0; i--)
    {
     rec_prot__pt->send_buf_raw__pu8[7+i] = low__u32;
     low__u32 >>= 8;
    }*/
// uint16 epoch;                                    // New field
//          uint48 sequence_number;                          // New field
//           uint16 length;

    // after setting the header, increment the record sequ.
  }
# endif /* ifdef FLEA_HAVE_DTLS */
  rec_prot__pt->send_curr_rec_content_len__u16  = 0;
  rec_prot__pt->send_curr_rec_content_offs__u16 = 0;
}

static flea_bool_t flea_recprot_t__have_pending_read_data_in_current_record(const flea_recprot_t* rec_prot__pt)
{
  return (rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16 > 0);
}

flea_err_e THR_flea_recprot_t__wrt_data(
  flea_recprot_t*          rec_prot__pt,
  flea_tls_rec_cont_type_e content_type__e,
  const flea_u8_t*         data__pcu8,
  flea_dtl_t               data_len__dtl
)
{
  flea_al_u16_t buf_free_len__alu16;

  FLEA_THR_BEG_FUNC();

  if(FLEA_RP__IS_PENDING_CLOSE_NOTIFY(rec_prot__pt))
  {
    FLEA_THROW("connection closed by peer", FLEA_ERR_TLS_REC_CLOSE_NOTIFY);
  }
  if(FLEA_RP__IS_SESSION_CLOSED(rec_prot__pt))
  {
    FLEA_THROW("tls session closed", FLEA_ERR_TLS_SESSION_CLOSED);
  }
  if(FLEA_RP__IS_WRITE_ONGOING(rec_prot__pt))
  {
    if(rec_prot__pt->send_buf_raw__pu8[0] != content_type__e)
    {
      FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));
      flea_recprot_t__set_record_header(rec_prot__pt, content_type__e);
    }
    rec_prot__pt->send_buf_raw__pu8 = rec_prot__pt->alt_send_buf__raw__bu8;
    // rec_prot__pt->send_payload_max_len__u16 = rec_prot__pt->alt_payload_max_len__u16;
  }
  else
  {
    rec_prot__pt->send_buf_raw__pu8 = rec_prot__pt->alt_send_buf__raw__bu8;
    // rec_prot__pt->send_payload_max_len__u16 = rec_prot__pt->alt_payload_max_len__u16;

    flea_recprot_t__set_record_header(rec_prot__pt, content_type__e);
  }
  rec_prot__pt->send_payload_buf__pu8 = rec_prot__pt->send_buf_raw__pu8 + rec_prot__pt->record_hdr_len__u8
    + rec_prot__pt->write_state__t.reserved_iv_len__u8;

  buf_free_len__alu16 = rec_prot__pt->record_plaintext_send_max_value__u16
    - rec_prot__pt->send_curr_rec_content_len__u16;
  while(data_len__dtl)
  {
    // rec_prot__pt->write_ongoing__u8 = 1;
    FLEA_RP__SET_WRITE_ONGOING(rec_prot__pt);
    flea_al_u16_t to_go__alu16 = FLEA_MIN(data_len__dtl, buf_free_len__alu16);
    memcpy(
      rec_prot__pt->send_payload_buf__pu8 + rec_prot__pt->send_curr_rec_content_len__u16,
      data__pcu8,
      to_go__alu16
    );
    data_len__dtl -= to_go__alu16;
    data__pcu8    += to_go__alu16;
    rec_prot__pt->send_curr_rec_content_len__u16 += to_go__alu16;
    buf_free_len__alu16 -= to_go__alu16;

    /**
     * send out the CCS, because it must be encrypted under the current epoch.
     */
    if(buf_free_len__alu16 == 0 ||
      (FLEA_RP__IS_DTLS(rec_prot__pt) && (content_type__e == CONTENT_TYPE_CHANGE_CIPHER_SPEC)))
    {
      FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));
      buf_free_len__alu16 = rec_prot__pt->record_plaintext_send_max_value__u16
        - rec_prot__pt->send_curr_rec_content_len__u16; // <= TODO: send_curr_rec_content_len__u16 is always zero here
      /* no need to write new header in this case: content stays, length comes later */
    }
  }
# if 0
  if(FLEA_RP__IS_DTLS(rec_prot__pt) && (content_type__e == CONTENT_TYPE_CHANGE_CIPHER_SPEC))
  {
    rec_prot__pt->write_state__t.seqno_lo_hi__au32[0] = 0; // INCREMENTING HERE, BUT THE RECORD MAY STILL BE AWAITING ENCRYPTION
    rec_prot__pt->write_state__t.seqno_lo_hi__au32[1] = 0;
    rec_prot__pt->write_state__t.epoch__u16++;

    /** In case of DTLS, as in TLS, the activation of the new write connection
     * state is done in the TLS logic layer, directly after the logical sending
     * of the of the CCS. The above code ensures that the CCS record is
     * prepared under the write connection before the activation of the new
     * connection. In case of a flight retransmission from the flight buffer,
     * the old write connection is restored before the flight including the
     * CCS is sent out.
     */

    FLEA_DBG_PRINTF("increased next write epoch to %u\n", rec_prot__pt->write_state__t.epoch__u16);
  }
# endif /* if 0 */
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__wrt_data */

# ifdef FLEA_HAVE_TLS_CS_CBC
static flea_err_e THR_flea_recprot_t__decr_rcrd_cbc_hmac(
  flea_recprot_t*          rec_prot__pt,
  flea_al_u16_t*           decrypted_len__palu16,
  flea_tls_rec_cont_type_e content_type__e
)
{
  flea_u32_t seq_lo__u32, seq_hi__u32;
  flea_u8_t enc_seq_nbr__au8[8];
  flea_al_s16_t max_padd_len__als16;
  flea_al_u16_t plaintext_len__alu16;
  flea_al_u16_t i__alu16;
  flea_al_u8_t left_range_mask__alu8;
  flea_al_u8_t padd_err__alu8;

  flea_u8_t mac_len =
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.mac_size__u8;
  flea_u8_t* enc_key = rec_prot__pt->read_state__t.suite_specific__u.cbc_hmac_conn_state__t.cipher_key__bu8;
  flea_u8_t iv_len   = flea_block_cipher__get_block_size(
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_id
  );
  flea_u8_t enc_key_len =
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_key_size__u8;

  FLEA_DECL_BUF(mac__bu8, flea_u8_t, FLEA_TLS_MAX_MAC_SIZE);
  flea_al_u16_t padding_len__alu16;
  flea_al_u8_t last_padd_byte__alu8;
  flea_u8_t* data        = rec_prot__pt->payload_buf__pu8;
  flea_u8_t* iv          = data;
  flea_al_u16_t data_len = rec_prot__pt->curr_rec_content_len__u16;

  FLEA_THR_BEG_FUNC();


#  ifdef FLEA_HAVE_DTLS
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    memcpy(
      enc_seq_nbr__au8,
      rec_prot__pt->send_rec_buf_raw__bu8 + FLEA_DTLS_REC_HDR_OFFS__SEQ,
      FLEA_DTLS_REC_HDR_LEN__SEQ
    );
  }
  else
#  endif /* ifdef FLEA_HAVE_DTLS */
  {
    seq_lo__u32 = rec_prot__pt->read_state__t.seqno_lo_hi__au32[0];
    seq_hi__u32 = rec_prot__pt->read_state__t.seqno_lo_hi__au32[1];

    flea__encode_U32_BE(seq_hi__u32, enc_seq_nbr__au8);
    flea__encode_U32_BE(seq_lo__u32, enc_seq_nbr__au8 + 4);
  }
  if(data_len < 2 * iv_len)
  {
    /* sending a different alert than BAD_RECORD_MAC here causes TLS_Attacker to
     * signal false positives */
    FLEA_THROW(
      "invalid payload length of encrypted TLS_**_WITH_**_CBC_SHA** message",
      FLEA_ERR_TLS_ENCOUNTERED_BAD_RECORD_MAC   /*FLEA_ERR_TLS_PROT_DECODE_ERR*/
    );
  }

  /*
   * First decrypt
   */

  if(THR_flea_cbc_mode__decrypt_data(
      rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_id,
      enc_key,
      enc_key_len,
      iv,
      iv_len,
      data + iv_len,
      data + iv_len,
      data_len - iv_len
  )
  )
  {
    /* there is no need to hide this type of error */
    FLEA_THROW("error during decryption", FLEA_ERR_TLS_ENCOUNTERED_BAD_RECORD_MAC);
  }


  plaintext_len__alu16 = data_len - iv_len;

  max_padd_len__als16 = plaintext_len__alu16 - mac_len - 1;
  if(max_padd_len__als16 < 1)
  {
    /* sending a different alert than BAD_RECORD_MAC here causes TLS_Attacker to
     * signal false positives */
    FLEA_THROW(
      "invalid plaintext size in CBC-based ciphersuite",
      FLEA_ERR_TLS_ENCOUNTERED_BAD_RECORD_MAC   /*FLEA_ERR_TLS_PROT_DECODE_ERR*/
    );
  }
  if(max_padd_len__als16 > 256)
  {
    max_padd_len__als16 = 256;
  }

  /*
   * Remove padding
   */
  last_padd_byte__alu8  = data[data_len - 1];
  padding_len__alu16    = last_padd_byte__alu8 + 1;
  left_range_mask__alu8 = 0;
  padd_err__alu8        = 0;
  for(i__alu16 = 0; i__alu16 <= (flea_al_u16_t) max_padd_len__als16; i__alu16++)
  {
    flea_al_u8_t last_iter_mask__alu8;
    flea_al_u8_t diff__alu8         = last_padd_byte__alu8;
    flea_al_u8_t padding_byte__alu8 = data[data_len - 1 - i__alu16];
    diff__alu8            ^= padding_byte__alu8;
    padd_err__alu8        |= (diff__alu8 & ~left_range_mask__alu8);
    last_iter_mask__alu8   = ~flea_expand_u32_to_u32_mask((i__alu16 + 1) ^ padding_len__alu16);
    left_range_mask__alu8 |= last_iter_mask__alu8;
  }

  /*
   * Check MAC
   */
  data_len = plaintext_len__alu16 - (padding_len__alu16) - mac_len;
  /* capture underflow */
  if(data_len > plaintext_len__alu16)
  {
    FLEA_THROW("insufficient size of hmac-cbc record payload", FLEA_ERR_TLS_ENCOUNTERED_BAD_RECORD_MAC);
  }
  FLEA_ALLOC_BUF(mac__bu8, mac_len);
#  ifdef FLEA_SCCM_USE_CACHEWARMING_IN_TA_CM
  /* cache warming */
  FLEA_CCALL(
    THR_flea_recprot_t__compute_mac_cbc_hmac(
      rec_prot__pt->send_rec_buf_raw__bu8,
      &rec_prot__pt->read_state__t,
      data + iv_len,
      plaintext_len__alu16,
      mac__bu8,
      // FLEA_RP__GET_RD_CURR_REC_EPOCH(rec_prot__pt),
      enc_seq_nbr__au8
      // rec_prot__pt->write_next_rec_epoch__u16
    )
  );
#  endif /* ifdef FLEA_SCCM_USE_CACHEWARMING_IN_TA_CM */
  /* compute the actual MAC */
  FLEA_CCALL(
    THR_flea_recprot_t__compute_mac_cbc_hmac(
      rec_prot__pt->send_rec_buf_raw__bu8,
      &rec_prot__pt->read_state__t,
      data + iv_len,
      data_len,
      mac__bu8,
      enc_seq_nbr__au8
      // FLEA_RP__GET_RD_CURR_REC_EPOCH(rec_prot__pt)
    )
  );

  FLEA_DBG_PRINTF("cbc hmac computed mac tag = ");
  FLEA_DBG_PRINT_BYTE_ARRAY(mac__bu8, mac_len);
  FLEA_DBG_PRINTF("cbc hmac received mac tag = ");
  FLEA_DBG_PRINT_BYTE_ARRAY(data + iv_len + data_len, mac_len);

  padd_err__alu8 |= !flea_sec_mem_equal(mac__bu8, data + iv_len + data_len, mac_len);
  if(padd_err__alu8)
  {
    flea_bool_t found__b = FLEA_FALSE;
    for(i__alu16 = 0; i__alu16 <= (flea_al_u16_t) max_padd_len__als16; i__alu16++)
    {
      flea_al_u16_t maced_data_len__alu16 = plaintext_len__alu16 - i__alu16 - mac_len;

      if(maced_data_len__alu16 == data_len)
      {
        found__b = FLEA_TRUE;
        continue;
      }
      FLEA_CCALL(
        THR_flea_recprot_t__compute_mac_cbc_hmac(
          rec_prot__pt->send_rec_buf_raw__bu8,
          &rec_prot__pt->read_state__t,
          data + iv_len,
          maced_data_len__alu16,
          mac__bu8,
          enc_seq_nbr__au8
          // FLEA_RP__GET_RD_CURR_REC_EPOCH(rec_prot__pt)
        )
      );
    }
    if(!found__b)
    {
      /* this cannot happen */
      FLEA_THROW("internal error", FLEA_ERR_INT_ERR);
    }
    FLEA_THROW("MAC failure", FLEA_ERR_TLS_ENCOUNTERED_BAD_RECORD_MAC);
  }
  memmove(data, data + iv_len, data_len);
  *decrypted_len__palu16 = data_len;
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(mac__bu8);
  );
} /* THR_flea_recprot_t__decr_rcrd_cbc_hmac */

static flea_err_e THR_flea_recprot_t__encr_rcrd_cbc_hmac(
  flea_recprot_t* rec_prot__pt,
  flea_al_u16_t*  encrypted_len__palu16
)
{
  flea_al_u16_t length_tot;

  FLEA_THR_BEG_FUNC();
  flea_u8_t* enc_key = rec_prot__pt->write_state__t.suite_specific__u.cbc_hmac_conn_state__t.cipher_key__bu8;
  flea_u8_t iv_len   = flea_block_cipher__get_block_size(
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_id
  );
  flea_u8_t mac_len =
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.mac_size__u8;
  flea_u8_t enc_key_len =
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_key_size__u8;
  flea_u8_t* iv       = rec_prot__pt->send_buf_raw__pu8 + rec_prot__pt->record_hdr_len__u8;
  flea_u8_t block_len = iv_len;

  flea_u8_t* data        = rec_prot__pt->send_payload_buf__pu8;
  flea_al_u16_t data_len = rec_prot__pt->send_curr_rec_content_len__u16;
  flea_u8_t padding_len  = (block_len - (data_len + mac_len + 1) % block_len) + 1; // +1 for padding_length entry

  flea_u8_t* mac     = data + data_len;
  flea_u8_t* padding = mac + mac_len;
  flea_dtl_t input_output_len;
  flea_u8_t k;
  flea_u8_t enc_seq_nbr__au8[8];
  flea_u32_t seq_lo__u32, seq_hi__u32;


  seq_lo__u32 = rec_prot__pt->write_state__t.seqno_lo_hi__au32[0];
  seq_hi__u32 = rec_prot__pt->write_state__t.seqno_lo_hi__au32[1];

  /* set the DTLS epcoh in the high u32 */

  if(rec_prot__pt->write_state__t.epoch__u16)
  {
    FLEA_DBG_PRINTF("setting epoch for CBC ciphersuite's compute HMAC: %u\n", rec_prot__pt->write_state__t.epoch__u16);
    seq_hi__u32 |= (rec_prot__pt->write_state__t.epoch__u16 << 16);
  }
  flea__encode_U32_BE(seq_hi__u32, enc_seq_nbr__au8);
  flea__encode_U32_BE(seq_lo__u32, enc_seq_nbr__au8 + 4);

  FLEA_DBG_PRINTF("cbc compute HMAC: encoded epoch | seq = ");
  for(unsigned i = 0; i < 8; i++)
  {
    FLEA_DBG_PRINTF("%02x ", enc_seq_nbr__au8[i]);
  }
  FLEA_DBG_PRINTF("\n");

  FLEA_CCALL(
    THR_flea_recprot_t__compute_mac_cbc_hmac(
      rec_prot__pt->send_buf_raw__pu8,
      &rec_prot__pt->write_state__t,
      data,
      data_len,
      mac,
      enc_seq_nbr__au8
    )
  );

  FLEA_CCALL(THR_flea_rng__randomize(iv, iv_len));

  input_output_len = data_len + padding_len + mac_len;

  for(k = 0; k < padding_len; k++)
  {
    padding[k] = padding_len - 1;
  }

  FLEA_CCALL(
    THR_flea_cbc_mode__encrypt_data(
      rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.cbc_hmac_config__t.cipher_id,
      enc_key,
      enc_key_len,
      iv,
      iv_len,
      data,
      data,
      input_output_len
    )
  );

  length_tot = input_output_len + iv_len;
  rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8 - 2] = length_tot >> 8;
  rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8 - 1] = length_tot;
  *encrypted_len__palu16 = input_output_len;
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__encr_rcrd_cbc_hmac */

# endif /* ifdef FLEA_HAVE_HMAC */

# ifdef FLEA_HAVE_TLS_CS_GCM
static flea_err_e THR_flea_recprot_t__decr_rcrd_gcm(
  flea_recprot_t*          rec_prot__pt,
  flea_al_u16_t*           decrypted_len__palu16,
  flea_tls_rec_cont_type_e content_type__e
)
{
  flea_u32_t seq_lo__u32, seq_hi__u32;
  flea_u8_t enc_seq_nbr__au8[8];
  flea_u8_t gcm_header__au8[13];
  flea_u8_t enc_data_len__au8[2];
  flea_u8_t* gcm_tag__pu8;
  flea_u8_t gcm_tag_len__u8         = FLEA_CONST_TLS_GCM_TAG_LEN;
  const flea_u8_t record_iv_len__u8 =
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.record_iv_length__u8;
  const flea_u8_t fixed_iv_len__u8 =
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.fixed_iv_length__u8;


  FLEA_THR_BEG_FUNC();
  flea_u8_t* enc_key    = rec_prot__pt->read_state__t.suite_specific__u.gcm_conn_state__t.cipher_key__bu8;
  flea_u8_t enc_key_len =
    rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.cipher_key_size__u8;

  // iv = fixed_iv + record_iv (record_iv is directly adjacent in memory)
  flea_u8_t* iv    = rec_prot__pt->read_state__t.suite_specific__u.gcm_conn_state__t.fixed_iv__bu8;
  flea_u8_t iv_len = fixed_iv_len__u8 + record_iv_len__u8;

  flea_u8_t* data        = rec_prot__pt->payload_buf__pu8;
  flea_al_u16_t data_len = rec_prot__pt->curr_rec_content_len__u16;

  if(data_len <= record_iv_len__u8 + gcm_tag_len__u8)
  {
    FLEA_THROW("invalid payload length of encrypted TLS_**_WITH_**_GCM_SHA** message", FLEA_ERR_TLS_PROT_DECODE_ERR);
  }

  // copy received explicit nonce into record iv
  memcpy(iv + fixed_iv_len__u8, data, record_iv_len__u8);
#  ifdef FLEA_HAVE_DTLS
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    memcpy(
      enc_seq_nbr__au8,
      rec_prot__pt->send_rec_buf_raw__bu8 + FLEA_DTLS_REC_HDR_OFFS__SEQ,
      FLEA_DTLS_REC_HDR_LEN__SEQ
    );
  }
  else
#  endif /* ifdef FLEA_HAVE_DTLS */
  {
    seq_lo__u32 = rec_prot__pt->read_state__t.seqno_lo_hi__au32[0];
    seq_hi__u32 = rec_prot__pt->read_state__t.seqno_lo_hi__au32[1];
    flea__encode_U32_BE(seq_hi__u32, enc_seq_nbr__au8); // TODO: GET RID OF THE BUFFER, ENCODE DIRECTLY IN GCM HEADER
    flea__encode_U32_BE(seq_lo__u32, enc_seq_nbr__au8 + 4);
  }


  *decrypted_len__palu16 = data_len - record_iv_len__u8 - gcm_tag_len__u8;

  // copy seq nr, type, version, length into gcm header
  enc_data_len__au8[0] = *decrypted_len__palu16 >> 8;
  enc_data_len__au8[1] = *decrypted_len__palu16;
  memcpy(gcm_header__au8, enc_seq_nbr__au8, 8);
  gcm_header__au8[8]  = rec_prot__pt->send_rec_buf_raw__bu8[0];
  gcm_header__au8[9]  = rec_prot__pt->prot_version__t.major;
  gcm_header__au8[10] = rec_prot__pt->prot_version__t.minor;
  memcpy(gcm_header__au8 + 11, enc_data_len__au8, 2);

  gcm_tag__pu8 = data + (data_len - gcm_tag_len__u8);
  FLEA_CCALL(
    THR_flea_ae__decrypt(
      rec_prot__pt->read_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.cipher_id,
      enc_key,
      enc_key_len,
      iv,
      iv_len,
      gcm_header__au8,
      sizeof(gcm_header__au8),
      data + record_iv_len__u8,
      data + record_iv_len__u8,
      *decrypted_len__palu16,
      gcm_tag__pu8,
      gcm_tag_len__u8
    )
  );

  memmove(data, data + record_iv_len__u8, *decrypted_len__palu16);
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__decr_rcrd_gcm */

static flea_err_e THR_flea_recprot_t__encr_rcrd_gcm(
  flea_recprot_t* rec_prot__pt,
  flea_al_u16_t*  encrypted_len__palu16
)
{
  flea_al_u16_t length_tot;
  flea_u8_t enc_seq_nbr__au8[8];
  flea_u32_t seq_lo__u32, seq_hi__u32;
  flea_u8_t* gcm_tag__pu8;
  flea_u8_t gcm_tag_len__u8 = FLEA_CONST_TLS_GCM_TAG_LEN;
  flea_u8_t gcm_header__au8[13]; // 8+1+2+2
  flea_u8_t enc_data_len__au8[2];

  FLEA_THR_BEG_FUNC();
  flea_u8_t* enc_key    = rec_prot__pt->write_state__t.suite_specific__u.gcm_conn_state__t.cipher_key__bu8;
  flea_u8_t enc_key_len =
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.cipher_key_size__u8;


  // iv = fixed_iv + record_iv (record_iv is directly adjacent in memory)
  flea_u8_t* iv    = rec_prot__pt->write_state__t.suite_specific__u.gcm_conn_state__t.fixed_iv__bu8;
  flea_u8_t iv_len =
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.fixed_iv_length__u8
    + rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.record_iv_length__u8;

  // copy sequence number to explicit nonce part as suggested in RFC 5288
  // TODO: REMOVE UNNECCESSARY DECODING/ ENCODING!
  seq_lo__u32 = rec_prot__pt->write_state__t.seqno_lo_hi__au32[0];
  seq_hi__u32 = rec_prot__pt->write_state__t.seqno_lo_hi__au32[1];

  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    seq_hi__u32 |= (rec_prot__pt->write_state__t.epoch__u16 << 16);
  }
  // TODO: FOR DTLS, COMPUTE RECORD MAC (PROBABLY) DIRECTLY ON ENCODED RECORD HRD+DATA
  flea__encode_U32_BE(seq_hi__u32, enc_seq_nbr__au8);
  flea__encode_U32_BE(seq_lo__u32, enc_seq_nbr__au8 + 4);
  memcpy(
    rec_prot__pt->write_state__t.suite_specific__u.gcm_conn_state__t.record_iv__bu8,
    enc_seq_nbr__au8,
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.record_iv_length__u8
  );

  // write explicit part of nonce/iv before the encrypted data
  flea_u8_t* expl_nonce = rec_prot__pt->send_buf_raw__pu8 + rec_prot__pt->record_hdr_len__u8;
  memcpy(
    expl_nonce,
    enc_seq_nbr__au8,
    rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.record_iv_length__u8
  );

  flea_u8_t* data        = rec_prot__pt->send_payload_buf__pu8;
  flea_al_u16_t data_len = rec_prot__pt->send_curr_rec_content_len__u16;

  // copy seq nr, type, version, length into gcm header
  enc_data_len__au8[0] = data_len >> 8;
  enc_data_len__au8[1] = data_len;
  memcpy(gcm_header__au8, enc_seq_nbr__au8, 8);
  gcm_header__au8[8]  = rec_prot__pt->send_buf_raw__pu8[0];
  gcm_header__au8[9]  = rec_prot__pt->prot_version__t.major;
  gcm_header__au8[10] = rec_prot__pt->prot_version__t.minor;
  memcpy(gcm_header__au8 + 11, enc_data_len__au8, 2);

  // set gcm tag to point "behind the data"
  gcm_tag__pu8 = data + data_len;

  FLEA_CCALL(
    THR_flea_ae__encrypt(
      rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.cipher_id,
      enc_key,
      enc_key_len,
      iv,   // fixed iv || record iv
      iv_len,
      gcm_header__au8,
      sizeof(gcm_header__au8),
      data,
      data,
      data_len,
      gcm_tag__pu8,
      gcm_tag_len__u8
    )
  );

  // copy authentication tag

  length_tot = data_len
    + rec_prot__pt->write_state__t.cipher_suite_config__t.suite_specific__u.gcm_config__t.record_iv_length__u8
    + gcm_tag_len__u8;
  rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8 - 2] = length_tot >> 8;
  rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8 - 1] = length_tot;
  *encrypted_len__palu16 = data_len + gcm_tag_len__u8;

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__encr_rcrd_gcm */

# endif /* ifdef FLEA_HAVE_TLS_CS_GCM */
flea_err_e THR_flea_recprot_t__write_flush(
  flea_recprot_t* rec_prot__pt
)
{
  FLEA_THR_BEG_FUNC();
  if((rec_prot__pt->send_curr_rec_content_len__u16 == 0) || !FLEA_RP__IS_WRITE_ONGOING(rec_prot__pt))
  {
    FLEA_THR_RETURN();
  }
  FLEA_DBG_PRINTF(
    "recprot_t__write_flush() called with non-zero content: send_curr_rec_content_len = %u, record type byte = %02x\n",
    rec_prot__pt->send_curr_rec_content_len__u16,
    rec_prot__pt->send_buf_raw__pu8[0]
  );
# ifdef FLEA_HAVE_TLS_CS_CBC
  if(rec_prot__pt->write_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_cbc_cipher_suite)
  {
    flea_al_u16_t encrypted_len__alu16;
    FLEA_CCALL(THR_flea_recprot_t__encr_rcrd_cbc_hmac(rec_prot__pt, &encrypted_len__alu16));
    FLEA_CCALL(
      THR_flea_rw_stream_t__write(
        rec_prot__pt->rw_stream__pt,
        rec_prot__pt->send_buf_raw__pu8,
        encrypted_len__alu16 + rec_prot__pt->record_hdr_len__u8 + rec_prot__pt->write_state__t.reserved_iv_len__u8
      )
    );

    inc_seq_nbr(rec_prot__pt->write_state__t.seqno_lo_hi__au32);
  }
  else
# endif /* ifdef FLEA_HAVE_TLS_CS_CBC */
# ifdef FLEA_HAVE_TLS_CS_GCM
  if(rec_prot__pt->write_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_gcm_cipher_suite)
  {
    flea_al_u16_t encrypted_len__alu16;
    FLEA_CCALL(THR_flea_recprot_t__encr_rcrd_gcm(rec_prot__pt, &encrypted_len__alu16));
    FLEA_CCALL(
      THR_flea_rw_stream_t__write(
        rec_prot__pt->rw_stream__pt,
        rec_prot__pt->send_buf_raw__pu8,
        encrypted_len__alu16 + rec_prot__pt->record_hdr_len__u8 + rec_prot__pt->write_state__t.reserved_iv_len__u8
      )
    );

    inc_seq_nbr(rec_prot__pt->write_state__t.seqno_lo_hi__au32);
  }
  else
# endif /* ifdef FLEA_HAVE_TLS_CS_GCM */
  if(rec_prot__pt->write_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_null_cipher_suite)
  {
    rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8
    - 2] = rec_prot__pt->send_curr_rec_content_len__u16 >> 8;
    rec_prot__pt->send_buf_raw__pu8[rec_prot__pt->record_hdr_len__u8
    - 1] = rec_prot__pt->send_curr_rec_content_len__u16;

    FLEA_DBG_PRINTF(
      "sending unencrypted record, dtls = %u, hdr-len = %u\n",
      FLEA_RP__IS_DTLS(rec_prot__pt),
      rec_prot__pt->record_hdr_len__u8
    );
    FLEA_DBG_PRINTF("  with header  = ");
    FLEA_DBG_PRINT_BYTE_ARRAY(rec_prot__pt->send_buf_raw__pu8, rec_prot__pt->record_hdr_len__u8);
    FLEA_DBG_PRINTF("  with content = ");
    FLEA_DBG_PRINT_BYTE_ARRAY(
      rec_prot__pt->send_buf_raw__pu8 + rec_prot__pt->record_hdr_len__u8,
      rec_prot__pt->send_curr_rec_content_len__u16
    );
    FLEA_CCALL(
      THR_flea_rw_stream_t__write(
        rec_prot__pt->rw_stream__pt,
        rec_prot__pt->send_buf_raw__pu8,
        rec_prot__pt->send_curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8
      )
    );
    inc_seq_nbr(rec_prot__pt->write_state__t.seqno_lo_hi__au32);
  }
  else
  {
    FLEA_THROW("unsupported ciphersuite", FLEA_ERR_INT_ERR);
  }
  FLEA_CCALL(THR_flea_rw_stream_t__flush_write(rec_prot__pt->rw_stream__pt));

  flea_recprot_t__discard_pending_write(rec_prot__pt);

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__write_flush */

flea_al_u16_t flea_recprot_t__get_current_max_pt_expansion(flea_recprot_t* rec_prot__pt)
{
  return rec_prot__pt->reserved_payl_len__u16;
}

flea_al_u16_t flea_recprot_t__get_current_max_record_pt_size(flea_recprot_t* rec_prot__pt)
{
  return rec_prot__pt->record_plaintext_send_max_value__u16;
}

flea_err_e THR_flea_recprot_t__send_record(
  flea_recprot_t*          rec_prot__pt,
  const flea_u8_t*         bytes__pcu8,
  flea_dtl_t               bytes_len__dtl,
  flea_tls_rec_cont_type_e content_type
)
{
  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(THR_flea_recprot_t__wrt_data(rec_prot__pt, content_type, bytes__pcu8, bytes_len__dtl));

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_tls__send_record */

flea_err_e THR_flea_recprot_t__send_alert_and_throw(
  flea_recprot_t*               rec_prot__pt,
  flea_tls__alert_description_t description,
  flea_err_e                    err__t
)
{
  flea_tls__alert_level_t lev = FLEA_TLS_ALERT_LEVEL_FATAL;

  FLEA_THR_BEG_FUNC();
  if(FLEA_RP__IS_SESSION_CLOSED(rec_prot__pt))
  {
    FLEA_THROW(
      "unable to send fatal alert due to session being already closed",
      FLEA_ERR_TLS_SESSION_CLOSED_WHEN_TRYING_TO_SEND_ALERT
    );
  }
  if(description == FLEA_TLS_ALERT_DESC_CLOSE_NOTIFY)
  {
    lev = FLEA_TLS_ALERT_LEVEL_WARNING;
  }
  if(description != FLEA_TLS_ALERT_NO_ALERT)
  {
    flea_recprot_t__discard_pending_write(rec_prot__pt);
    FLEA_CCALL(THR_flea_recprot_t__send_alert(rec_prot__pt, description, lev));
    FLEA_RP__SET_SESSION_CLOSED(rec_prot__pt);
  }
  FLEA_THROW("throwing error after (potentially) sending fatal TLS alert", err__t);

  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_recprot_t__send_alert(
  flea_recprot_t*               rec_prot__pt,
  flea_tls__alert_description_t description,
  flea_tls__alert_level_t       level
)
{
  FLEA_THR_BEG_FUNC();

  flea_u8_t alert_bytes[2];
  alert_bytes[0] = level;
  alert_bytes[1] = description;
  FLEA_CCALL(THR_flea_recprot_t__send_record(rec_prot__pt, alert_bytes, sizeof(alert_bytes), CONTENT_TYPE_ALERT));
  FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_recprot_t__close_and_send_close_notify(flea_recprot_t* rec_prot__pt)
{
  FLEA_THR_BEG_FUNC();

  if(!FLEA_RP__IS_SESSION_CLOSED(rec_prot__pt))
  {
    flea_recprot_t__discard_pending_write(rec_prot__pt);
    FLEA_CCALL(
      THR_flea_recprot_t__send_alert(
        rec_prot__pt,
        FLEA_TLS_ALERT_DESC_CLOSE_NOTIFY,
        FLEA_TLS_ALERT_LEVEL_WARNING
      )
    );
    FLEA_RP__SET_SESSION_CLOSED(rec_prot__pt);
  }
  FLEA_THR_FIN_SEC_empty();
}

void flea_recprot_t__discard_current_read_record(flea_recprot_t* rec_prot__pt)
{
  rec_prot__pt->curr_rec_content_offs__u16 = rec_prot__pt->curr_pt_content_len__u16;
  // rec_prot__pt->raw_read_buf_content__u16 = 0; // TODO: THIS CAN BE MORE THAN
  // THE CURRENT RECORD!
}

static flea_err_e THR_flea_recprot_t__decrypt_current_record(flea_recprot_t* rec_prot__pt)
{
  flea_al_u16_t raw_rec_content_len__alu16 = rec_prot__pt->curr_rec_content_len__u16;

  FLEA_THR_BEG_FUNC();

  if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_null_cipher_suite)
  {
    rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
  }
# ifdef FLEA_HAVE_TLS_CS_CBC
  if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_cbc_cipher_suite)
  {
    FLEA_CCALL(
      THR_flea_recprot_t__decr_rcrd_cbc_hmac(
        rec_prot__pt,
        &raw_rec_content_len__alu16,
        (flea_tls_rec_cont_type_e) rec_prot__pt->send_rec_buf_raw__bu8[0]
      )
    );
    rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
    // TODO: REMOVE THIS, NOT NEEDED FOR DTLS
    inc_seq_nbr(rec_prot__pt->read_state__t.seqno_lo_hi__au32);
    FLEA_DBG_PRINTF("after record decryption (CBC)\n");
  }
# endif  /* ifdef FLEA_HAVE_TLS_CS_CBC */
# ifdef FLEA_HAVE_TLS_CS_GCM
  if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_gcm_cipher_suite)
  {
    FLEA_CCALL(
      THR_flea_recprot_t__decr_rcrd_gcm(
        rec_prot__pt,
        &raw_rec_content_len__alu16,
        (flea_tls_rec_cont_type_e) rec_prot__pt->send_rec_buf_raw__bu8[0]
      )
    );
    rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
    // TODO: NOT NEEDED FOR DTLS, BUT ALSO DOESN'T DO ANY HARM
    inc_seq_nbr(rec_prot__pt->read_state__t.seqno_lo_hi__au32);
    FLEA_DBG_PRINTF("after record decryption (GCM)\n");
  }
# endif  /* ifdef FLEA_HAVE_TLS_CS_GCM */
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__decrypt_current_record */

flea_err_e THR_flea_recprot_t__set_encr_rd_rec_and_decrypt_it(
  flea_recprot_t*     rec_prot__pt,
  qheap_queue_heap_t* heap__pt,
  qh_al_hndl_t        hndl__alqhh
)
{
  FLEA_THR_BEG_FUNC();
  qh_size_t len__qsz = qheap_qh_get_queue_len(heap__pt, hndl__alqhh);
  FLEA_DBG_PRINTF("serradi: total len of encr rec incl hdr = %u\n", len__qsz);
  qheap_qh_read(heap__pt, hndl__alqhh, &rec_prot__pt->send_rec_buf_raw__bu8[0], len__qsz);
  rec_prot__pt->curr_rec_content_len__u16  = len__qsz - FLEA_DTLS_RECORD_HDR_LEN;
  rec_prot__pt->raw_read_buf_content__u16  = len__qsz;
  rec_prot__pt->curr_rec_content_offs__u16 = 0;
  FLEA_DBG_PRINTF("serradi: setting content len of encrypted record = %u\n", rec_prot__pt->curr_rec_content_len__u16);
  /* sets curr_pt_content_len__u16: */
  FLEA_CCALL(THR_flea_recprot_t__decrypt_current_record(rec_prot__pt));
  FLEA_DBG_PRINTF("serradi: decrypted the record, plaintext len = %u\n", rec_prot__pt->curr_pt_content_len__u16);
  FLEA_THR_FIN_SEC_empty();
}

# ifdef FLEA_HAVE_DTLS
flea_err_e THR_flea_recprot_t__write_encr_rec_to_queue(
  flea_recprot_t*     rec_prot__pt,
  qheap_queue_heap_t* qh__pt,
  qh_al_hndl_t        hndl_for_encryped_rec__alqhh
)
{
  FLEA_THR_BEG_FUNC();
  flea_u16_t append_len__u16 = rec_prot__pt->curr_rec_content_len__u16 + FLEA_DTLS_RECORD_HDR_LEN;
  if(!FLEA_RP__IS_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt))
  {
    FLEA_THROW("invalid request for encrypted record", FLEA_ERR_INT_ERR);
  }
  if(rec_prot__pt->curr_rec_content_offs__u16 != 0)
  {
    FLEA_THROW("non-zero rd offset when requesting encrypted record", FLEA_ERR_INT_ERR);
  }
  if(qheap_qh_append_to_queue(
      qh__pt,
      hndl_for_encryped_rec__alqhh,
      rec_prot__pt->send_rec_buf_raw__bu8,
      append_len__u16
  ))
  {
    FLEA_THROW("could not write the full encrypted record to the queue", FLEA_ERR_BUFF_TOO_SMALL);
  }
  rec_prot__pt->curr_rec_content_offs__u16 = rec_prot__pt->curr_pt_content_len__u16;
  // rec_prot__pt->curr_rec_content_offs__u16 += append_len__u16;
  FLEA_RP__SET_NO_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt);
  FLEA_THR_FIN_SEC_empty();
}

#  if 0
flea_err_e THR_flea_recprot_t__increment_read_epoch(flea_recprot_t* rec_prot__pt)
{
  FLEA_THR_BEG_FUNC();
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    rec_prot__pt->read_state__t.epoch__u16++;
    FLEA_DBG_PRINTF("incremented read epoch to %u\n", rec_prot__pt->read_state__t.epoch__u16);
    if(rec_prot__pt->read_state__t.epoch__u16 == 0)
    {
      FLEA_THROW("DTLS epoch exhausted", FLEA_ERR_TLS_SQN_EXHAUSTED);
    }
  }
  // NOTE: TODO: WHEN INVOKING RECORD_RECEIVED, THE EPOCHE FROM THE SEQ MUST
  // BE USED

  FLEA_THR_FIN_SEC_empty();
}

#  endif /* if 0 */

flea_bool_t flea_recprot_t__is_rd_buf_empty(flea_recprot_t* rec_prot__pt)
{
  if( /* check if the first record has pending read data */
    (rec_prot__pt->curr_rec_content_offs__u16 < rec_prot__pt->curr_pt_content_len__u16)
    ||
    /* check for pending record behind the current one */
    (rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8 <
    rec_prot__pt->raw_read_buf_content__u16)
  )
  {
    return FLEA_FALSE;
  }
  return FLEA_TRUE;
}

/**
 * current_or_next_record_for_content_type__b = TRUE means that the function
 * shall read in the next record, if no record with remaining unread content is
 * held as the current record, irrespectively of whether *data_len__pdtl > 0.
 */
static flea_err_e THR_flea_recprot_t__read_data_inner_dtls(
  flea_recprot_t*               rec_prot__pt,
  flea_u8_t*                    data__pu8,
  flea_dtl_t*                   data_len__pdtl,
  flea_tls__protocol_version_t* prot_version_mbn__pt,
  flea_bool_t                   do_verify_prot_version__b,
  flea_tls_rec_cont_type_e      cont_type__e,
  flea_tls_rec_cont_type_e*     result_cont_type__pe,
  flea_stream_read_mode_e       rd_mode__e
)
{
  flea_al_u16_t to_cp__alu16, read_bytes_count__dtl = 0;
  flea_dtl_t data_len__dtl = *data_len__pdtl;

  flea_bool_t is_handsh_msg_during_app_data__b = FLEA_FALSE;
  flea_bool_t is_rec_from_prev_epoch__b        = FLEA_FALSE;
  // flea_bool_t is_handsh_msg_fr_prev_epoch__b = FLEA_FALSE;

  flea_bool_t current_or_next_record_for_content_type__b = (result_cont_type__pe != NULL);

  FLEA_THR_BEG_FUNC();
  *data_len__pdtl = 0;

  if(result_cont_type__pe)
  {
    *result_cont_type__pe = CONTENT_TYPE_ANY;
  }

  if(FLEA_RP__IS_PENDING_CLOSE_NOTIFY(rec_prot__pt))
  {
    FLEA_THROW("connection closed by peer", FLEA_ERR_TLS_REC_CLOSE_NOTIFY);
  }
  if(FLEA_RP__IS_SESSION_CLOSED(rec_prot__pt))
  {
    FLEA_THROW("tls session closed", FLEA_ERR_TLS_SESSION_CLOSED);
  }
  if(FLEA_RP__IS_WRITE_ONGOING(rec_prot__pt))
  {
    FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));
    // rec_prot__pt->raw_read_buf_content__u16 = 0;
    // rec_prot__pt->curr_rec_content_len__u16     = 0;
  }

  if(data_len__dtl && FLEA_RP__IS_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt))
  {
    FLEA_THROW("read request to encrypted record", FLEA_ERR_INT_ERR);
  }

  /*FLEA_DBG_PRINTF("\nread_data_inner starting\n");
  FLEA_DBG_PRINTF("current_or_next_record_for_content_type__b  = %u\n", current_or_next_record_for_content_type__b);
  FLEA_DBG_PRINTF("data_read for length = %u\n", data_len__dtl);*/
  rec_prot__pt->payload_buf__pu8 = rec_prot__pt->send_rec_buf_raw__bu8 + rec_prot__pt->record_hdr_len__u8;
  /* output data from a possibly held record witz nonzero payload data left */


  if(result_cont_type__pe && rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16)
  {
    *result_cont_type__pe = rec_prot__pt->send_rec_buf_raw__bu8[0];
  }

  to_cp__alu16 = FLEA_MIN(
    data_len__dtl,
    rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16
  );
  memcpy(data__pu8, rec_prot__pt->payload_buf__pu8 + rec_prot__pt->curr_rec_content_offs__u16, to_cp__alu16);
  rec_prot__pt->curr_rec_content_offs__u16 += to_cp__alu16;
  data_len__dtl         -= to_cp__alu16;
  data__pu8             += to_cp__alu16;
  read_bytes_count__dtl += to_cp__alu16;

  // FLEA_DBG_PRINTF("read_bytes before loop = %u\n", read_bytes_count__dtl);
  // enter only if
  // - called with current_or_next_record_for_content_type__b
  //   OR
  // - called with non-zero length and data copied above did not suffice.
  /* get new record hdr and content */
  if((current_or_next_record_for_content_type__b &&
    !flea_recprot_t__have_pending_read_data_in_current_record(rec_prot__pt)) || data_len__dtl)
  {
    /* start reading a new record */
    flea_stream_read_mode_e local_rd_mode__e = rd_mode__e;
    flea_dtl_t raw_read_len__dtl;
    flea_al_u8_t hdr_pos__alu8;

    if(local_rd_mode__e == flea_read_full)
    {
      local_rd_mode__e = flea_read_blocking;
    }

#  ifdef FLEA_DO_DBG_PRINT
    unsigned dbg_loop_cnt = 0;
#  endif
    do
    {
      flea_al_u16_t curr_rec_full_len__alu16 = 0;
#  ifdef FLEA_DO_DBG_PRINT
      if(rec_prot__pt->curr_rec_content_len__u16)
      {
        FLEA_DBG_PRINTF("dbg_loop_cnt = %u\n", dbg_loop_cnt++);
        unsigned i;
        FLEA_DBG_PRINTF("entering loop\n");
        FLEA_DBG_PRINTF("rec_prot__pt->curr_rec_content_len__u16 = %u\n", rec_prot__pt->curr_rec_content_len__u16);
        if(rec_prot__pt->curr_rec_content_len__u16)
        {
          FLEA_DBG_PRINTF("record_content with hdr = ");
          for(i = 0; i < rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8; i++)
          {
            FLEA_DBG_PRINTF("%02x ", rec_prot__pt->send_rec_buf_raw__bu8[i]);
          }
          FLEA_DBG_PRINTF("\n");
        }
        FLEA_DBG_PRINTF("current rec content offs = %u\n", rec_prot__pt->curr_rec_content_offs__u16);
        FLEA_DBG_PRINTF("current rec pt len = %u\n", rec_prot__pt->curr_pt_content_len__u16);

        FLEA_DBG_PRINTF("rec_prot__pt->raw_read_buf_content__u16= %u\n", rec_prot__pt->raw_read_buf_content__u16);
        FLEA_DBG_PRINTF("raw read buf content = ");
        for(i = 0; i < rec_prot__pt->raw_read_buf_content__u16; i++)
        {
          FLEA_DBG_PRINTF("%02x ", rec_prot__pt->send_rec_buf_raw__bu8[i]);
        }
        FLEA_DBG_PRINTF("\n");
      }
#  endif /* ifdef FLEA_DO_DBG_PRINT */

      /* if we arrive here, there is the need to read a further record, either for the content type or for content data */
      /* test if there is a further record in the current buffer. */
      if(rec_prot__pt->curr_rec_content_offs__u16 == rec_prot__pt->curr_pt_content_len__u16)
      {
        /* the current record has been completely read (or was empty in the first place) */

        /* now determine whether there is a further record behing the current one, and if so shift it in. otherwise, reset the buffer state to 'empty'. */

        if(rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8 <
          rec_prot__pt->raw_read_buf_content__u16)
        {
          if(rec_prot__pt->curr_rec_content_len__u16)// || rec_prot__pt->skip_empty_record__b)
          {
            curr_rec_full_len__alu16 = rec_prot__pt->record_hdr_len__u8 + rec_prot__pt->curr_rec_content_len__u16;
          }
          FLEA_DBG_PRINTF("shifting down next record, curr_rec_full_len__alu16 = %u\n", curr_rec_full_len__alu16);
          flea_al_u16_t next_rec_size__alu16 = rec_prot__pt->raw_read_buf_content__u16 - curr_rec_full_len__alu16;
          if(next_rec_size__alu16)
          {
            flea_al_u16_t shift_size__alu16;
            shift_size__alu16 = curr_rec_full_len__alu16;
            FLEA_DBG_PRINTF(
              "shifting down record, next_rec_size__alu16 = %u, raw_read_buf_content__u16 = %u, curr_rec_full_len__alu16 = %u\n",
              next_rec_size__alu16,
              rec_prot__pt->raw_read_buf_content__u16,
              curr_rec_full_len__alu16
            );
            memmove(
              rec_prot__pt->send_rec_buf_raw__bu8,
              rec_prot__pt->send_rec_buf_raw__bu8 + shift_size__alu16,
              next_rec_size__alu16
            );
          }


          if(rec_prot__pt->curr_rec_content_len__u16)// || rec_prot__pt->skip_empty_record__b)
          {
            /* if no data is requested, and only the content type is desired, then
             * this loop is only entered if there is no pending data in the current
             * record.
             * if data is request, and there was some data left in the current
             * record, it did not suffice if we arrive here*/

            /* if there was a previous record held, then now is the time to remove
             * it. thus we reduced raw_read_buf_content__u16 by the removed hdr len. content size zero records are discarded directly when detected.*/
            rec_prot__pt->raw_read_buf_content__u16 -= rec_prot__pt->record_hdr_len__u8
              + rec_prot__pt->curr_rec_content_len__u16;
          }
        }
        else
        {
          flea_recprot_t__set_current_rd_rec_empty(rec_prot__pt);

          rec_prot__pt->raw_read_buf_content__u16 = 0;
        }
      }


      const flea_al_u8_t rec_hdr_up_to_incl_version__calu8 = 3;
      /* read the hdr */
      /* header must be parsed even if record has been shifted in */
      {
        do
        {
          raw_read_len__dtl = 0;
          if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
          {
            raw_read_len__dtl = FLEA_TLS_TRNSF_BUF_SIZE - rec_prot__pt->raw_read_buf_content__u16;

            // TODO: read until version first, then make potentially TLS=>DTLS
            // transition,
            // that is: if (not already version=DTLS1.2) AND (DTLS is allowed)
            // FLEA_DBG_PRINTF("initial read request for  %u bytes\n", raw_read_len__dtl);
            FLEA_CCALL(
              THR_flea_rw_stream_t__read(
                rec_prot__pt->rw_stream__pt,
                &rec_prot__pt->send_rec_buf_raw__bu8[rec_prot__pt->raw_read_buf_content__u16],
                &raw_read_len__dtl,   // this becomes the maximal record size (?), then change:
                local_rd_mode__e   // read_mode = non_blocking => local_read_mode = non_blocking (v)
                //         ... blocking     =>                   ... blocking (was: " => full")
                //         ... full         =>                   ... blocking (was: " => full") (we don't yet know how many bytes we actually expect!

                //         expected for TLS: depending on read mode, as before
                //          for DTLS: full record
                //         to check once content length is known:
                //        TLS: if rd_mode = blocking: at least one data byte can be returned (i.e. at least one full record must have been read, and more will not be read in this function invocation)
                //       if rd_mode = full: if current record completely read, then continue reading next record.
                //      if not all requested bytes have been read after the first failing of incomplete record read, return T/O
                //     DTLS: always: content length must be within currently read packet size
                //    if rd_mode = blocking: like TLS
                //   if rd_mode = full:  like TLS

              )
            );
            // FLEA_DBG_PRINTF("initial read request actual read size = %u bytes\n", raw_read_len__dtl);
          }
          rec_prot__pt->raw_read_buf_content__u16 += raw_read_len__dtl;
#  ifdef FLEA_DO_DBG_PRINT

          /*FLEA_DBG_PRINTF(
            "before version check: rec_prot__pt->raw_read_buf_content__u16 = %u\n",
            rec_prot__pt->raw_read_buf_content__u16
          );*/

          /*FLEA_DBG_PRINTF("raw read buf content = ");
          for(unsigned i = 0; i < rec_prot__pt->raw_read_buf_content__u16; i++)
          {
            FLEA_DBG_PRINTF("%02x ", rec_prot__pt->send_rec_buf_raw__bu8[i]);
          }
          FLEA_DBG_PRINTF("\n");
          */
#  endif /* ifdef FLEA_DO_DBG_PRINT */

          /*FLEA_DBG_PRINTF("prot_version_mbn__pt = %p\n", prot_version_mbn__pt);
        FLEA_DBG_PRINTF("prot_version_mbn__pt->major = %u\n", prot_version_mbn__pt->major);
        FLEA_DBG_PRINTF("prot_version_mbn__pt->minor = %u\n", prot_version_mbn__pt->minor);*/
          if(rec_prot__pt->raw_read_buf_content__u16 >= rec_hdr_up_to_incl_version__calu8)
          {
            if(((rec_prot__pt->send_rec_buf_raw__bu8[1] == FLEA_DTLS_1_X_VERSION_MAJOR) &&
              ((rec_prot__pt->send_rec_buf_raw__bu8[2] == FLEA_DTLS_1_0_VERSION_MINOR) ||
              (rec_prot__pt->send_rec_buf_raw__bu8[2] == FLEA_DTLS_1_2_VERSION_MINOR))))
            {
              FLEA_DBG_PRINTF(
                "DTLS 1.2 requested by peer, checking if (possibly again, already done) switching to it is allowed\n"
              );
              if(FLEA_RP__IS_DTLS(rec_prot__pt))
              {
                // TODO: REMOVE, AND ALSO VERIFY DTLS VERSION CHECK AGAIN
                FLEA_DBG_PRINTF("activating DTLS\n");
                rec_prot__pt->record_hdr_len__u8 = FLEA_DTLS_RECORD_HDR_LEN;
                rec_prot__pt->payload_buf__pu8   = rec_prot__pt->send_rec_buf_raw__bu8
                  + rec_prot__pt->record_hdr_len__u8;

                /* note:
                   - receive length is not affected, since the DTLS header
                   length is considered in the calculation of the buffer size
                   - send len is affected directly */

                /*rec_prot__pt->record_plaintext_send_max_value__u16 = FLEA_MIN(
                  FLEA_TLS_RECORD_MAX_SEND_PLAINTEXT_SIZE,
                  rec_prot__pt->record_plaintext_send_max_value__u16
                );*/
                // rec_prot__pt->is_dtls_active__u8 = 1;
              }
            }
          }
          if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
          {
            rec_prot__pt->raw_read_buf_content__u16 = 0;
          }
          /* we had to read header bytes, but couldn't */
          if(rec_prot__pt->raw_read_buf_content__u16 == 0)
          {
            if(local_rd_mode__e == flea_read_nonblocking)
            {
              /* discard empty read or non-full header read */

              /*FLEA_DBG_PRINTF(
                "THR_flea_recprot_t__read_data_inner_dtls returning with %u read bytes\n",
                rec_prot__pt->raw_read_buf_content__u16
              );*/
              *data_len__pdtl = 0;
              FLEA_THR_RETURN();
            }
            else
            {
              /*FLEA_DBG_PRINTF(
                "needed = rec_prot__pt->record_hdr_len__u8 = %u, current_raw_read_content = %u, raw_read_len__dtl = %u\n",
                rec_prot__pt->record_hdr_len__u8,
                rec_prot__pt->raw_read_buf_content__u16,
                raw_read_len__dtl
                );*/
              FLEA_THROW("0 bytes returned from blocking read", FLEA_ERR_TIMEOUT_ON_STREAM_READ);
            }
          }
        } while(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8);
        /* header is read completely */
        if(rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_ALERT)
        {
          FLEA_RP__SET_CURRENT_RECORD_ALERT(rec_prot__pt);
        }
        else
        {
          FLEA_RP__SET_NOT_CURRENT_RECORD_ALERT(rec_prot__pt);
        }
        if(!FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt))
        {
          /* in case of DTLS we must also accept CCS for now in case of retransmission by
           * the peer in order to trigger us to retransmit the very final flight of the
           * handshake. If this
           */
          if(
            (cont_type__e == CONTENT_TYPE_APPLICATION_DATA) &&
            ((rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_HANDSHAKE) ||
            (rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_CHANGE_CIPHER_SPEC))
          )
          {
            is_handsh_msg_during_app_data__b = FLEA_TRUE;
          }
          else if(!current_or_next_record_for_content_type__b &&
            (cont_type__e != rec_prot__pt->send_rec_buf_raw__bu8[0]))
          {
#  if 0
#   ifdef FLEA_DO_DBG_PRINT
            FLEA_DBG_PRINTF("content type required: %u, ", cont_type__e);
            FLEA_DBG_PRINTF("content type found: %u\n", rec_prot__pt->send_rec_buf_raw__bu8[0]);
            unsigned add = 0;
            if(FLEA_RP__IS_DTLS(rec_prot__pt))
            {
              add = 8;
            }
            FLEA_DBG_PRINTF(
              "this record's content len = %u\n",
              (rec_prot__pt->send_rec_buf_raw__bu8[3 + add] << 8) | rec_prot__pt->send_rec_buf_raw__bu8[4 + add]
            );
#   endif /* ifdef FLEA_DO_DBG_PRINT */
#  endif /* if 0 */
            FLEA_THROW("content type does not match", FLEA_ERR_TLS_INV_REC_HDR);
          }
          else
          {
            FLEA_DBG_PRINTF("content type inquired: %u\n", rec_prot__pt->send_rec_buf_raw__bu8[0]);
          }

#  if 0
#   ifdef FLEA_DO_DBG_PRINT
          unsigned add = 0;
          if(FLEA_RP__IS_DTLS(rec_prot__pt))
          {
            add = 8;
          }
          FLEA_DBG_PRINTF(
            "this record's content len = %u\n",
            (rec_prot__pt->send_rec_buf_raw__bu8[3 + add] << 8) | rec_prot__pt->send_rec_buf_raw__bu8[4 + add]
          );
#   endif /* ifdef FLEA_DO_DBG_PRINT */
#  endif /* if 0 */

          if(do_verify_prot_version__b)
          {
            if((prot_version_mbn__pt->major != rec_prot__pt->send_rec_buf_raw__bu8[1]) ||
              (prot_version_mbn__pt->minor != rec_prot__pt->send_rec_buf_raw__bu8[2]))
            {
              FLEA_THROW("invalid protocol version in record", FLEA_ERR_TLS_INV_REC_HDR);
            }
          }
          else if(prot_version_mbn__pt)
          {
            flea_al_u8_t add = 0;

            /*FLEA_DBG_PRINTF(
              "setting the record version number into result struct (dtls_active = %u)\n",
              rec_prot__pt->is_dtls_active__u8
            );*/
            // TODO: FOR DTLS, this is incorrent, when openssl sends DTLS1.0 in
            // record, but DTLS1.2 in handshake msg
            // => find out what the meaning is
            if(FLEA_RP__IS_DTLS(rec_prot__pt))
            {
              add = 8;
            }
            prot_version_mbn__pt->major = rec_prot__pt->send_rec_buf_raw__bu8[1 + add];
            prot_version_mbn__pt->minor = rec_prot__pt->send_rec_buf_raw__bu8[2 + add];
          }
        }
        hdr_pos__alu8 = 3;
        flea_al_u8_t i;
        flea_al_u16_t rec_epoch__alu16;
        // flea_al_u16_t old_epoch__alu16 = FLEA_RP__GET_RD_CURR_REC_EPOCH(rec_prot__pt);
        rec_prot__pt->read_state__t.seqno_lo_hi__au32[0] = 0;
        rec_prot__pt->read_state__t.seqno_lo_hi__au32[1] = 0;

        /* read seqno is currently needed in order to use this value from the
         * clientHello in the helloVerifyRequest */
        for(i = 0; i < 8; i++)
        {
          flea_u32_t s__u32 = rec_prot__pt->read_state__t.seqno_lo_hi__au32[1 - (i / 4)];
          s__u32 <<= 8;
          s__u32  |= rec_prot__pt->send_rec_buf_raw__bu8[hdr_pos__alu8++];
          rec_prot__pt->read_state__t.seqno_lo_hi__au32[1 - (i / 4)] = s__u32;
        }
        rec_epoch__alu16 = FLEA_RP__GET_RD_CURR_REC_EPOCH(rec_prot__pt);

        FLEA_DBG_PRINTF("from recevied record hdr: epoch | seq = ");
        for(int i = 1; i >= 0; i--)
        {
          FLEA_DBG_PRINTF("%08x ", rec_prot__pt->read_state__t.seqno_lo_hi__au32[i]);
        }
        FLEA_DBG_PRINTF("\n");

        FLEA_DBG_PRINTF("epoch decoded from incoming record header = %u\n", rec_epoch__alu16);
        if(rec_epoch__alu16 != rec_prot__pt->read_state__t.epoch__u16)
        {
          FLEA_DBG_PRINTF(
            "received record from wrong epoch = %u, expected epoch = %u\n",
            rec_epoch__alu16,
            rec_prot__pt->read_state__t.epoch__u16
          );
          if(rec_epoch__alu16 == rec_prot__pt->read_state__t.epoch__u16 + 1)
          {
            /* can be an out of order finished or application data */

            /* set current_rec_undecrypted = true
             * return
             *
             */
            FLEA_RP__SET_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt);
          }
          else if(1 + rec_epoch__alu16 == rec_prot__pt->read_state__t.epoch__u16)
          {
            // if(1 + rec_epoch__alu16 == rec_prot__pt->read_state__t.epoch__u16)
            {
              FLEA_DBG_PRINTF("outer recv rec. from prev-epoch\n");
              /* may be a retransmission of the previous epoch */
              // is_handsh_msg_fr_prev_epoch__b  = FLEA_TRUE;
              /* during read_app_data, we have to throw */
#  if 0
              if(  /*cont_type__e == CONTENT_TYPE_APPLICATION_DATA &&*/
                ((rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_HANDSHAKE) &&
                (rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_CHANGE_CIPHER_SPEC)))
#  endif
              {
                FLEA_DBG_PRINTF("recv rec. from prev-epoch\n");
                is_rec_from_prev_epoch__b = FLEA_TRUE;
              }
            }
            // continue;
          }
          else
          {
            FLEA_DBG_PRINTF("TODO: discarding record due to invalid epoch\n");
            // TODO: DISCARD/IGNORE THIS RECORD, BUT NOT THE WHOLE PACKET
            // TODO: TEST FOR EPOCH +- 2 and more
          }
        }
        // TODO: CHECK THAT RECORD IS WITHIN ACCEPTANCE WINDOW


        rec_prot__pt->curr_rec_content_len__u16  = rec_prot__pt->send_rec_buf_raw__bu8[hdr_pos__alu8++] << 8;
        rec_prot__pt->curr_rec_content_len__u16 |= rec_prot__pt->send_rec_buf_raw__bu8[hdr_pos__alu8];

        if(result_cont_type__pe)
        {
          *result_cont_type__pe = rec_prot__pt->send_rec_buf_raw__bu8[0];
        }


        FLEA_DBG_PRINTF("newley read record content len = %u\n", rec_prot__pt->curr_rec_content_len__u16);

        // if DTLS: check the the content was completely received // MAY
        // THERE BY ADDITIONAL RECORDS IN THE PACKET? ANSWER: yes!
        // => TODO: for DTLS, after processing this record, shift down the received data
        //          for TSL, do the same (cannot read only the header of the initial ClientHello: if the client uses DTLS, then the packet would already be lost)

        if(rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8 >
          rec_prot__pt->raw_read_buf_content__u16)
        {
          FLEA_DBG_PRINTF(
            "[prev-epoch] discarding rec due to insuff. len.: lhs = %u, rhs = %u \n",
            rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8,
            rec_prot__pt->raw_read_buf_content__u16
          );
          rec_prot__pt->curr_rec_content_len__u16 = 0;
          rec_prot__pt->raw_read_buf_content__u16 = 0;
          continue;
          // TODO: test this ( DISCARD THE RECORD, in this case the whole rec_buf can be
          // discarded)
        }
        // }
      } /* end of 'read the hdr' */

      rec_prot__pt->curr_rec_content_offs__u16 = 0;

      if(FLEA_RP__IS_DTLS_REC_FROM_FUT_EPOCH(rec_prot__pt) || is_rec_from_prev_epoch__b)
      {
        /* skip over the current record so that a potentially trailing record would be read next */
        // rec_prot__pt->curr_rec_content_offs__u16 = rec_prot__pt->curr_pt_content_len__u16;
        flea_recprot_t__discard_current_read_record(rec_prot__pt);
        if(is_rec_from_prev_epoch__b)
        {
          FLEA_DBG_PRINTF("throwing 'recv rec. from prev-epoch'\n");
          FLEA_THROW("received record from previous epoch", FLEA_EXC_TLS_HS_MSG_FR_PREV_EPOCH);
        }

        /* we expect always that the first the record type is determined. That call would return here.
         * Subsequently, the encrypted record is read completely using the designated special function*/
        FLEA_THR_RETURN();
      }


      FLEA_CCALL(THR_flea_recprot_t__decrypt_current_record(rec_prot__pt));
      if(rec_prot__pt->curr_pt_content_len__u16 > FLEA_TLS_RECORD_MAX_RECEIVE_PLAINTEXT_SIZE)
      {
        FLEA_THROW("record plaintext size too large", FLEA_ERR_TLS_RECORD_OVERFLOW);
      }

      if(is_handsh_msg_during_app_data__b)
      {
        if((rec_prot__pt->curr_pt_content_len__u16 >= FLEA_DTLS_HANDSH_HDR_LEN))
        {
          flea_al_u8_t type =
            rec_prot__pt->send_rec_buf_raw__bu8[FLEA_DTLS_RECORD_HDR_LEN + FLEA_DTLS_HS_HDR_OFFS__MSG_TYPE];
          /* check if this can be a record to trigger renegotiation */
          if((type == HANDSHAKE_TYPE_CLIENT_HELLO) || (type == HANDSHAKE_TYPE_HELLO_REQUEST))
          {
            /* is NOT triggered during handshake, only during rd_app_data */
            FLEA_THROW(
              "received tls handshake message when app data was expected",
              FLEA_EXC_TLS_HS_MSG_DURING_APP_DATA
            );

            /* it is OK to drop any other records silently in this case. The first flight of a renegotiation request
             * from the peer contains only one of the two handshake msg types checked for in this conditional.
             */
          }
        }
        FLEA_DBG_PRINTF("dropping non-initial handshake msg during read_app_data\n");
        // rec_prot__pt->curr_rec_content_offs__u16 = rec_prot__pt->curr_pt_content_len__u16;
        flea_recprot_t__discard_current_read_record(rec_prot__pt);
        // IE: DROP THE RECORD
      }
      else if(FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt))
      {
        if(rec_prot__pt->payload_buf__pu8[1] == FLEA_TLS_ALERT_DESC_CLOSE_NOTIFY)
        {
          if(((rd_mode__e == flea_read_full) && data_len__dtl) || !read_bytes_count__dtl)
          {
            FLEA_THROW("received close notify", FLEA_ERR_TLS_REC_CLOSE_NOTIFY);
          }
          else
          {
            FLEA_RP__SET_PENDING_CLOSE_NOTIFY(rec_prot__pt);
          }
        }

        FLEA_CCALL(THR_flea_recprot_t__handle_alert(rec_prot__pt, read_bytes_count__dtl));
      }
      else
      {
        to_cp__alu16 = FLEA_MIN(
          rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16,
          data_len__dtl
        );
#  if 0
#   ifdef FLEA_DO_DBG_PRINT
        FLEA_DBG_PRINTF("before trailing read: previously read %u bytes\n", read_bytes_count__dtl);
        FLEA_DBG_PRINTF("trailing read: reading %u bytes = ", to_cp__alu16);
        unsigned i;
        for(i = 0; i < to_cp__alu16; i++)
        {
          FLEA_DBG_PRINTF("%02x ", rec_prot__pt->payload_buf__pu8[i]);
        }
        FLEA_DBG_PRINTF("\n");
#   endif /* ifdef FLEA_DO_DBG_PRINT */
#  endif /* if 0 */


        memcpy(data__pu8, rec_prot__pt->payload_buf__pu8, to_cp__alu16);
        rec_prot__pt->curr_rec_content_offs__u16 += to_cp__alu16;
        read_bytes_count__dtl += to_cp__alu16;
        data_len__dtl         -= to_cp__alu16;
        data__pu8 += to_cp__alu16;
        // *data_len__pdtl = read_bytes_count__dtl; // IS SUPERFLOUS
      }
    } while(
      FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt) || ((rd_mode__e == flea_read_full) && data_len__dtl) ||
      ((rd_mode__e == flea_read_blocking) && !read_bytes_count__dtl)
    );
  } /* end of ' get new record hdr and content' */

  *data_len__pdtl = read_bytes_count__dtl;

  /*if(!flea_recprot_t__have_pending_read_data(rec_prot__pt))
    {
    flea_recprot_t__discard_current_read_record(rec_prot__pt);
    }*/
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__read_data_inner_dtls */

# endif /* ifdef FLEA_HAVE_DTLS */
static flea_err_e THR_flea_recprot_t__read_data_inner_tls(
  flea_recprot_t*               rec_prot__pt,
  flea_u8_t*                    data__pu8,
  flea_dtl_t*                   data_len__pdtl,
  flea_tls__protocol_version_t* prot_version_mbn__pt,
  flea_bool_t                   do_verify_prot_version__b,
  flea_tls_rec_cont_type_e      cont_type__e,
  flea_tls_rec_cont_type_e*     result_cont_type__pe,
  flea_stream_read_mode_e       rd_mode__e
)
{
  flea_al_u16_t to_cp__alu16, read_bytes_count__dtl = 0;
  flea_dtl_t data_len__dtl = *data_len__pdtl;

  flea_dtl_t raw_read_len__dtl;
  flea_al_u8_t hdr_pos__alu8;
  flea_bool_t is_handsh_msg_during_app_data__b = FLEA_FALSE;

  flea_bool_t current_or_next_record_for_content_type__b = (result_cont_type__pe != NULL);

  FLEA_THR_BEG_FUNC();
  *data_len__pdtl = 0;

  if(result_cont_type__pe)
  {
    *result_cont_type__pe = CONTENT_TYPE_ANY;
  }

  if(FLEA_RP__IS_PENDING_CLOSE_NOTIFY(rec_prot__pt))
  {
    FLEA_THROW("connection closed by peer", FLEA_ERR_TLS_REC_CLOSE_NOTIFY);
  }
  if(FLEA_RP__IS_SESSION_CLOSED(rec_prot__pt))
  {
    FLEA_THROW("tls session closed", FLEA_ERR_TLS_SESSION_CLOSED);
  }
  if(FLEA_RP__IS_WRITE_ONGOING(rec_prot__pt))
  {
    FLEA_CCALL(THR_flea_recprot_t__write_flush(rec_prot__pt));
  }

  rec_prot__pt->payload_buf__pu8 = rec_prot__pt->send_rec_buf_raw__bu8 + rec_prot__pt->record_hdr_len__u8;


  if(result_cont_type__pe && rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16)
  {
    *result_cont_type__pe = rec_prot__pt->send_rec_buf_raw__bu8[0];
  }

  /* output data from a possibly held record witz nonzero payload data left */
  to_cp__alu16 = FLEA_MIN(
    data_len__dtl,
    rec_prot__pt->curr_pt_content_len__u16 - rec_prot__pt->curr_rec_content_offs__u16
  );

  memcpy(data__pu8, rec_prot__pt->payload_buf__pu8 + rec_prot__pt->curr_rec_content_offs__u16, to_cp__alu16);
  rec_prot__pt->curr_rec_content_offs__u16 += to_cp__alu16;
  data_len__dtl         -= to_cp__alu16;
  data__pu8             += to_cp__alu16;
  read_bytes_count__dtl += to_cp__alu16;
  // FLEA_DBG_PRINTF("read_bytes before loop = %u\n", read_bytes_count__dtl );
  // enter only if
  // - called with current_or_next_record_for_content_type__b
  //   OR
  // - called with non-zero length and data copied above did not suffice.
  /* get new record hdr and content */
  if(!((current_or_next_record_for_content_type__b &&
    !flea_recprot_t__have_pending_read_data_in_current_record(rec_prot__pt)) || data_len__dtl))
  {
    FLEA_THR_RETURN();
  }
  /* start reading a new record */
  flea_stream_read_mode_e local_rd_mode__e = rd_mode__e;

  /*if(local_rd_mode__e == flea_read_blocking)
    {
    local_rd_mode__e = flea_read_full;
    }*/
  if(local_rd_mode__e == flea_read_full)
  {
    local_rd_mode__e = flea_read_blocking;
  }

  do
  {
//      flea_al_u16_t curr_rec_full_len__alu16 = 0;

    /* invalidate completely read record */

    if((rec_prot__pt->raw_read_buf_content__u16 >= rec_prot__pt->record_hdr_len__u8) &&
      (
        rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8 ==
        rec_prot__pt->raw_read_buf_content__u16))
    {
      rec_prot__pt->curr_rec_content_offs__u16 = 0;
      rec_prot__pt->curr_rec_content_len__u16  = 0;
      rec_prot__pt->curr_pt_content_len__u16   = 0;
      rec_prot__pt->raw_read_buf_content__u16  = 0;
    }

    /* if we get here, we either need a new header or (further) content */
    /* test if content might have been only partially read: */

    // if(rec_prot__pt->raw_read_buf_content__u16 > rec_ &&

    const flea_al_u8_t rec_hdr_up_to_incl_version__calu8 = 3;
    /* read the hdr */
    /* header must be parsed even if record has been shifted in */
    // if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
    {
      // while(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
      do
      {
        raw_read_len__dtl = 0;
        // raw_read_len__dtl = rec_prot__pt->record_hdr_len__u8 - rec_prot__pt->raw_read_buf_content__u16;
        if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
        {
          // raw_read_len__dtl = FLEA_TLS_TRNSF_BUF_SIZE - rec_prot__pt->raw_read_buf_content__u16; // TODO: IF DTLS IS EXCLUDED (EITHER BY CONFIGURATION OR BECAUSE THE INITIAL RECORD HAS ALREADY BEEN READ, CONSIDER TO RENDER THIS JUST TO THE TLS RECORD HDR SIZE => UNUSED BUFFER SPACE CAN BE USED FOR SENDING)
          raw_read_len__dtl = FLEA_TLS_RECORD_HDR_LEN;
          // TODO: read until version first, then make potentially TLS=>DTLS
          // transition,
          // that is: if (not already version=DTLS1.2) AND (DTLS is allowed)
          FLEA_CCALL(
            THR_flea_rw_stream_t__read(
              rec_prot__pt->rw_stream__pt,
              &rec_prot__pt->send_rec_buf_raw__bu8[rec_prot__pt->raw_read_buf_content__u16],
              &raw_read_len__dtl,     // this becomes the maximal record size (?), then change:
              local_rd_mode__e     // read_mode = non_blocking => local_read_mode = non_blocking (v)
              //         ... blocking     =>                   ... blocking (was: " => full")
              //         ... full         =>                   ... blocking (was: " => full") (we don't yet know how many bytes we actually expect!

              //         expected for TLS: depending on read mode, as before
              //          for DTLS: full record
              //         to check once content length is known:
              //        TLS: if rd_mode = blocking: at least one data byte can be returned (i.e. at least one full record must have been read, and more will not be read in this function invocation)
              //       if rd_mode = full: if current record completely read, then continue reading next record.
              //      if not all requested bytes have been read after the first failing of incomplete record read, return T/O
              //     DTLS: always: content length must be within currently read packet size
              //    if rd_mode = blocking: like TLS
              //   if rd_mode = full:  like TLS

            )
          );
        }
        rec_prot__pt->raw_read_buf_content__u16 += raw_read_len__dtl;

        if(rec_prot__pt->raw_read_buf_content__u16 >= rec_hdr_up_to_incl_version__calu8)
        {
          if(((rec_prot__pt->send_rec_buf_raw__bu8[1] == FLEA_DTLS_1_X_VERSION_MAJOR) &&
            ((rec_prot__pt->send_rec_buf_raw__bu8[2] == FLEA_DTLS_1_0_VERSION_MINOR) ||
            (rec_prot__pt->send_rec_buf_raw__bu8[2] == FLEA_DTLS_1_2_VERSION_MINOR))))
          {
            if(FLEA_RP__IS_DTLS(rec_prot__pt))
            {
              rec_prot__pt->record_hdr_len__u8 = FLEA_DTLS_RECORD_HDR_LEN;
              rec_prot__pt->payload_buf__pu8   = rec_prot__pt->send_rec_buf_raw__bu8
                + rec_prot__pt->record_hdr_len__u8;

              /* note:
                 - receive length is not affected, since the DTLS header
                 length is considered in the calculation of the buffer size
                 - send len is affected directly */

              // rec_prot__pt->is_dtls_active__u8 = 1;
            }
          }
        }
        if(FLEA_RP__IS_DTLS(rec_prot__pt))
        {
          if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
          {
            rec_prot__pt->raw_read_buf_content__u16 = 0;
          }
        }
        /* we had to read header bytes, but couldn't */
        if(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8)
        {
          if(local_rd_mode__e == flea_read_nonblocking)
          {
            *data_len__pdtl = 0;
            FLEA_THR_RETURN();
          }
          else
          {
            if(!raw_read_len__dtl)
            {
              FLEA_THROW("0 bytes returned from blocking read", FLEA_ERR_TIMEOUT_ON_STREAM_READ);
            }
          }
        }
      } while(rec_prot__pt->raw_read_buf_content__u16 < rec_prot__pt->record_hdr_len__u8);
      /* header is read completely */
      if(rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_ALERT)
      {
        FLEA_RP__SET_CURRENT_RECORD_ALERT(rec_prot__pt);
      }
      else
      {
        FLEA_RP__SET_NOT_CURRENT_RECORD_ALERT(rec_prot__pt);
      }
      if(!FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt))
      {
        if(
          (cont_type__e == CONTENT_TYPE_APPLICATION_DATA) &&
          (rec_prot__pt->send_rec_buf_raw__bu8[0] == CONTENT_TYPE_HANDSHAKE))
        {
          is_handsh_msg_during_app_data__b = FLEA_TRUE;
        }
        else if(!current_or_next_record_for_content_type__b &&
          (cont_type__e != rec_prot__pt->send_rec_buf_raw__bu8[0]))
        {
          FLEA_THROW("content type does not match", FLEA_ERR_TLS_INV_REC_HDR);
        }
        else
        {
          FLEA_DBG_PRINTF("content type inquired: %u\n", rec_prot__pt->send_rec_buf_raw__bu8[0]);
        }


        if(do_verify_prot_version__b)
        {
          if((prot_version_mbn__pt->major != rec_prot__pt->send_rec_buf_raw__bu8[1]) ||
            (prot_version_mbn__pt->minor != rec_prot__pt->send_rec_buf_raw__bu8[2]))
          {
            FLEA_THROW("invalid protocol version in record", FLEA_ERR_TLS_INV_REC_HDR);
          }
        }
        else if(prot_version_mbn__pt)
        {
          prot_version_mbn__pt->major = rec_prot__pt->send_rec_buf_raw__bu8[1];
          prot_version_mbn__pt->minor = rec_prot__pt->send_rec_buf_raw__bu8[2];
        }
      }
      hdr_pos__alu8 = 3;

      rec_prot__pt->curr_rec_content_len__u16  = rec_prot__pt->send_rec_buf_raw__bu8[hdr_pos__alu8++] << 8;
      rec_prot__pt->curr_rec_content_len__u16 |= rec_prot__pt->send_rec_buf_raw__bu8[hdr_pos__alu8];

      if(result_cont_type__pe)
      {
        *result_cont_type__pe = rec_prot__pt->send_rec_buf_raw__bu8[0];
      }

      /*if(rec_prot__pt->curr_rec_content_len__u16 == 0)
      {
        rec_prot__pt->skip_empty_record__b = FLEA_TRUE;
      }*/

      if(rec_prot__pt->curr_rec_content_len__u16 > FLEA_TLS_TRNSF_BUF_SIZE - rec_prot__pt->record_hdr_len__u8)
      {
        FLEA_THROW("received record does not fit into receive buffer", FLEA_ERR_TLS_EXCSS_REC_LEN);
      }
      if(FLEA_RP__IS_DTLS(rec_prot__pt))
      {
        if(rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8 >
          rec_prot__pt->raw_read_buf_content__u16)
        {
          rec_prot__pt->curr_rec_content_len__u16 = 0;
          rec_prot__pt->raw_read_buf_content__u16 = 0;
          continue;
          // TODO: test this ( DISCARD THE RECORD, in this case the whole rec_buf can be
          // discarded)
        }
      }
    }   /* end of 'read the hdr' */

    /* complete the content read if necessary (not relevant for DTLS) (TODO: still needed to consider a further read at all?) */

    // TODO: SHOULD THIS BE AN IF INSTEAD OF A WHILE ? "while" means: timeout of the underlying read stream is applied per-read attempt, which may only return a single byte. "if" means: apply the timeout to a single read which requests the necessary content length.
    while(rec_prot__pt->raw_read_buf_content__u16 <
      rec_prot__pt->curr_rec_content_len__u16 + rec_prot__pt->record_hdr_len__u8)
    {
      flea_stream_read_mode_e content_read_mode__e = local_rd_mode__e;
      flea_al_u16_t needed_read_len__alu16;
      if(FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt))
      {
        content_read_mode__e = flea_read_full;
      }
      needed_read_len__alu16 = raw_read_len__dtl = rec_prot__pt->curr_rec_content_len__u16
        - (rec_prot__pt->raw_read_buf_content__u16 - rec_prot__pt->record_hdr_len__u8);
      FLEA_CCALL(
        THR_flea_rw_stream_t__read(
          rec_prot__pt->rw_stream__pt,
          rec_prot__pt->payload_buf__pu8 + rec_prot__pt->raw_read_buf_content__u16
          - rec_prot__pt->record_hdr_len__u8,
          &raw_read_len__dtl,
          content_read_mode__e
        )
      );
      rec_prot__pt->raw_read_buf_content__u16 += raw_read_len__dtl;

      if(raw_read_len__dtl < needed_read_len__alu16)
      {
        if(local_rd_mode__e == flea_read_nonblocking)
        {
          *data_len__pdtl = 0;
          FLEA_THR_RETURN();
        }
        else
        {
          if(raw_read_len__dtl == 0)
          {
            FLEA_THROW("0 bytes returned from blocking read", FLEA_ERR_TIMEOUT_ON_STREAM_READ);
          }
        }
      }
    }   /* did read full content */

    rec_prot__pt->curr_rec_content_offs__u16 = 0;


    FLEA_CCALL(THR_flea_recprot_t__decrypt_current_record(rec_prot__pt));
    // raw_rec_content_len__alu16 = rec_prot__pt->curr_rec_content_len__u16;

    /*if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_null_cipher_suite)
    {
      rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
    }*/
# if 0
#  ifdef FLEA_HAVE_TLS_CS_CBC
    if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_cbc_cipher_suite)
    {
      FLEA_CCALL(
        THR_flea_recprot_t__decr_rcrd_cbc_hmac(
          rec_prot__pt,
          &raw_rec_content_len__alu16,
          (flea_tls_rec_cont_type_e) rec_prot__pt->send_rec_buf_raw__bu8[0]
        )
      );
      rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
      inc_seq_nbr(rec_prot__pt->read_state__t.seqno_lo_hi__au32);
    }
#  endif /* ifdef FLEA_HAVE_TLS_CS_CBC */
#  ifdef FLEA_HAVE_TLS_CS_GCM
    if(rec_prot__pt->read_state__t.cipher_suite_config__t.cipher_suite_class__e == flea_gcm_cipher_suite)
    {
      FLEA_CCALL(
        THR_flea_recprot_t__decr_rcrd_gcm(
          rec_prot__pt,
          &raw_rec_content_len__alu16,
          (flea_tls_rec_cont_type_e) rec_prot__pt->send_rec_buf_raw__bu8[0]
        )
      );
      rec_prot__pt->curr_pt_content_len__u16 = raw_rec_content_len__alu16;
      inc_seq_nbr(rec_prot__pt->read_state__t.seqno_lo_hi__au32);
    }
#  endif /* ifdef FLEA_HAVE_TLS_CS_GCM */
# endif /* if 0 */
    if(rec_prot__pt->curr_pt_content_len__u16 > FLEA_TLS_RECORD_MAX_RECEIVE_PLAINTEXT_SIZE)
    {
      FLEA_THROW("record plaintext size too large", FLEA_ERR_TLS_RECORD_OVERFLOW);
    }

    if(is_handsh_msg_during_app_data__b)
    {
      FLEA_THROW("received tls handshake message when app data was expected", FLEA_EXC_TLS_HS_MSG_DURING_APP_DATA);
    }
    else if(FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt))
    {
      if(rec_prot__pt->payload_buf__pu8[1] == FLEA_TLS_ALERT_DESC_CLOSE_NOTIFY)
      {
        if(((rd_mode__e == flea_read_full) && data_len__dtl) || !read_bytes_count__dtl)
        {
          FLEA_THROW("received close notify", FLEA_ERR_TLS_REC_CLOSE_NOTIFY);
        }
        else
        {
          FLEA_RP__SET_PENDING_CLOSE_NOTIFY(rec_prot__pt);
        }
      }

      FLEA_CCALL(THR_flea_recprot_t__handle_alert(rec_prot__pt, read_bytes_count__dtl));
    }
    else
    {
      to_cp__alu16 = FLEA_MIN(rec_prot__pt->curr_pt_content_len__u16, data_len__dtl);
# ifdef FLEA_DO_DBG_PRINT
      FLEA_DBG_PRINTF("trailing read: reading %u bytes = ", to_cp__alu16);
      unsigned i;
      for(i = 0; i < to_cp__alu16; i++)
      {
        FLEA_DBG_PRINTF("%02x ", rec_prot__pt->payload_buf__pu8[i]);
      }
      FLEA_DBG_PRINTF("\n");
# endif /* ifdef FLEA_DO_DBG_PRINT */


      memcpy(data__pu8, rec_prot__pt->payload_buf__pu8, to_cp__alu16);
      rec_prot__pt->curr_rec_content_offs__u16 += to_cp__alu16;
      read_bytes_count__dtl += to_cp__alu16;
      data_len__dtl         -= to_cp__alu16;
      data__pu8      += to_cp__alu16;
      *data_len__pdtl = read_bytes_count__dtl;
    }
  } while(
    FLEA_RP__IS_CURRENT_RECORD_ALERT(rec_prot__pt) || ((rd_mode__e == flea_read_full) && data_len__dtl) ||
    ((rd_mode__e == flea_read_blocking) && !read_bytes_count__dtl)
  );


  /*if(!flea_recprot_t__have_pending_read_data(rec_prot__pt))
    {
    flea_recprot_t__discard_current_read_record(rec_prot__pt);
    }*/
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_recprot_t__read_data_inner_tls */

static flea_err_e THR_flea_recprot_t__read_data_inner(
  flea_recprot_t*               rec_prot__pt,
  flea_u8_t*                    data__pu8,
  flea_dtl_t*                   data_len__pdtl,
  flea_tls__protocol_version_t* prot_version_mbn__pt,
  flea_bool_t                   do_verify_prot_version__b,
  flea_tls_rec_cont_type_e      cont_type__e,
  flea_tls_rec_cont_type_e*     result_cont_type__pe,
  flea_stream_read_mode_e       rd_mode__e
)
{
  if(FLEA_RP__IS_DTLS(rec_prot__pt))
  {
    return THR_flea_recprot_t__read_data_inner_dtls(
      rec_prot__pt,
      data__pu8,
      data_len__pdtl,
      prot_version_mbn__pt,
      do_verify_prot_version__b,
      cont_type__e,
      result_cont_type__pe,
      rd_mode__e
    );
  }
  else
  {
    return THR_flea_recprot_t__read_data_inner_tls(
      rec_prot__pt,
      data__pu8,
      data_len__pdtl,
      prot_version_mbn__pt,
      do_verify_prot_version__b,
      cont_type__e,
      result_cont_type__pe,
      rd_mode__e
    );
  }
}

/* get the content type of the currently (and not yet completely read) or newly received buffer */
flea_err_e THR_flea_recprot_t__get_current_record_type(
  flea_recprot_t*           rec_prot__pt,
  flea_tls_rec_cont_type_e* cont_type__pe,
  flea_stream_read_mode_e   rd_mode__e
)
{
  FLEA_THR_BEG_FUNC();
  flea_tls__protocol_version_t dummy_version__t;
  flea_dtl_t read_len_zero__dtl = 0;
  FLEA_CCALL(
    THR_flea_recprot_t__read_data_inner(
      rec_prot__pt,
      NULL,
      &read_len_zero__dtl,
      &dummy_version__t,
      FLEA_FALSE,
      0 /*dummy_content_type */,
      cont_type__pe,
      // FLEA_TRUE,
      rd_mode__e
    )
  );
  // *cont_type__pe = rec_prot__pt->send_rec_buf_raw__bu8[0];
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_recprot_t__read_data(
  flea_recprot_t*          rec_prot__pt,
  flea_tls_rec_cont_type_e cont_type__e,
  flea_u8_t*               data__pu8,
  flea_dtl_t*              data_len__pdtl,
  flea_stream_read_mode_e  rd_mode__e
)
{
  // TODO: FLATTEN, CALLED FUNCTION CAN BE INCLUDED HERE!
  return THR_flea_recprot_t__read_data_inner(
    rec_prot__pt,
    data__pu8,
    data_len__pdtl,
    NULL,
    FLEA_FALSE,
    cont_type__e,
    NULL,
    rd_mode__e
  );
}

flea_bool_t flea_recprot_t__have_done_initial_handshake(const flea_recprot_t* rec_prot__pt)
{
  if(rec_prot__pt->write_state__t.cipher_suite_config__t.cipher_suite_class__e != flea_null_cipher_suite)
  {
    return FLEA_TRUE;
  }
  return FLEA_FALSE;
}

flea_al_u16_t flea_recprot_t__get_curr_wrt_rcrd_rem_free_len(const flea_recprot_t* rec_prot__pt)
{
  return rec_prot__pt->record_plaintext_send_max_value__u16
         - rec_prot__pt->send_curr_rec_content_len__u16;
}

void flea_recprot_t__set_max_pt_len(
  flea_recprot_t* rec_prot__pt,
  flea_u16_t      pt_len__u16
)
{
  // rec_prot__pt->alt_payload_max_len__u16 = pt_len__u16;
  if(pt_len__u16 < rec_prot__pt->record_plaintext_send_max_value__u16)
  {
    rec_prot__pt->record_plaintext_send_max_value__u16 = pt_len__u16;
  }
}

# ifdef FLEA_HEAP_MODE

flea_err_e THR_flea_recprot_t__resize_send_plaintext_size(
  flea_recprot_t* rec_prot__pt,
  flea_al_u16_t   new_len__alu16
)
{
  FLEA_THR_BEG_FUNC();
  new_len__alu16 += FLEA_TLS_RECORD_MAX_SEND_ADD_DATA_SIZE + rec_prot__pt->record_hdr_len__u8;

  FLEA_FREE_MEM_CHECK_SET_NULL_SECRET_ARR(
    rec_prot__pt->alt_send_buf__raw__bu8,
    rec_prot__pt->alt_send_buf__raw_len__u16
  );

  FLEA_ALLOC_BUF(
    rec_prot__pt->alt_send_buf__raw__bu8,
    new_len__alu16
  );

  rec_prot__pt->alt_send_buf__raw_len__u16 = new_len__alu16;

  FLEA_THR_FIN_SEC_empty();
}

# endif /* ifdef FLEA_HEAP_MODE */


void flea_recprot_t__dtor(flea_recprot_t* rec_prot__pt)
{
  /* no way to handle error here: */
  if(rec_prot__pt->payload_buf__pu8)
  {
    flea_err_e dummy;
    flea_err_e ignore = THR_flea_recprot_t__close_and_send_close_notify(rec_prot__pt);
    dummy  = ignore;
    ignore = dummy;
  }
  flea_tls_con_stt_t__dtor(&rec_prot__pt->write_state__t);
  flea_tls_con_stt_t__dtor(&rec_prot__pt->read_state__t);
  FLEA_FREE_MEM_CHECK_NULL_SECRET_ARR(rec_prot__pt->send_rec_buf_raw__bu8, FLEA_TLS_TRNSF_BUF_SIZE);
  FLEA_FREE_MEM_CHECK_NULL_SECRET_ARR(rec_prot__pt->alt_send_buf__raw__bu8, rec_prot__pt->alt_send_buf__raw_len__u16);
  flea_recprot_t__INIT(rec_prot__pt);
}

flea_err_e THR_flea_recprot_t__send_change_cipher_spec_directly(
  flea_recprot_t* rec_prot__pt
)
{
  FLEA_THR_BEG_FUNC();

  const flea_u8_t css_bytes[1] = {1};

  FLEA_CCALL(
    THR_flea_recprot_t__send_record(
      rec_prot__pt,
      css_bytes,
      sizeof(css_bytes),
      CONTENT_TYPE_CHANGE_CIPHER_SPEC
    )
  );

  FLEA_THR_FIN_SEC_empty();
}

#endif /* ifdef FLEA_HAVE_TLS */
