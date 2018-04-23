/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_tls_rec_prot__H_
#define _flea_tls_rec_prot__H_

#include "flea/types.h"
#include "flea/error.h"
#include "internal/common/tls/tls_ciph_suite.h"
#include "internal/common/tls/tls_conn_state.h"
#include "internal/common/tls/tls_ctx_fwd.h"
// #include "internal/common/tls/tls_common.h"
#include "flea/rw_stream.h"
#include "internal/common/tls/tls_rec_prot_fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FLEA_HAVE_TLS

# define FLEA_TLS_RECORD_HDR_LEN      5
# define FLEA_TLS_TRNSF_BUF_SIZE      (FLEA_TLS_MAX_RECORD_SIZE + FLEA_TLS_RECORD_HDR_LEN)
# define FLEA_TLS_STD_MAX_RECORD_SIZE 18432

typedef enum
{
  CONTENT_TYPE_CHANGE_CIPHER_SPEC = 20,
  CONTENT_TYPE_ALERT              = 21,
  CONTENT_TYPE_HANDSHAKE          = 22,
  CONTENT_TYPE_APPLICATION_DATA   = 23,
  CONTENT_TYPE_HEARTBEAT          = 24
} flea_tls_rec_cont_type_e;

typedef enum
{
  FLEA_TLS_ALERT_DESC_CLOSE_NOTIFY                = 0,
  FLEA_TLS_ALERT_DESC_UNEXPECTED_MESSAGE          = 10,
  FLEA_TLS_ALERT_DESC_BAD_RECORD_MAC              = 20,
  FLEA_TLS_ALERT_DESC_DECRYPTION_FAILED_RESERVED  = 21,
  FLEA_TLS_ALERT_DESC_RECORD_OVERFLOW             = 22,
  FLEA_TLS_ALERT_DESC_DECOMPRESSION_FAILURE       = 30,
  FLEA_TLS_ALERT_DESC_HANDSHAKE_FAILURE           = 40,
  FLEA_TLS_ALERT_DESC_NO_CERTIFICATE_RESERVED     = 41,
  FLEA_TLS_ALERT_DESC_BAD_CERTIFICATE             = 42,
  FLEA_TLS_ALERT_DESC_UNSUPPORTED_CERTIFICATE     = 43,
  FLEA_TLS_ALERT_DESC_CERTIFICATE_REVOKED         = 44,
  FLEA_TLS_ALERT_DESC_CERTIFICATE_EXPIRED         = 45,
  FLEA_TLS_ALERT_DESC_CERTIFICATE_UNKNOWN         = 46,
  FLEA_TLS_ALERT_DESC_ILLEGAL_PARAMETER           = 47,
  FLEA_TLS_ALERT_DESC_UNKNOWN_CA                  = 48,
  FLEA_TLS_ALERT_DESC_ACCESS_DENIED               = 49,
  FLEA_TLS_ALERT_DESC_DECODE_ERROR                = 50,
  FLEA_TLS_ALERT_DESC_DECRYPT_ERROR               = 51,
  FLEA_TLS_ALERT_DESC_EXPORT_RESTRICTION_RESERVED = 60,
  FLEA_TLS_ALERT_DESC_PROTOCOL_VERSION            = 70,
  FLEA_TLS_ALERT_DESC_INSUFFICIENT_SECURITY       = 71,
  FLEA_TLS_ALERT_DESC_INTERNAL_ERROR              = 80,
  FLEA_TLS_ALERT_DESC_USER_CANCELED               = 90,
  FLEA_TLS_ALERT_DESC_NO_RENEGOTIATION            = 100,
  FLEA_TLS_ALERT_DESC_UNSUPPORTED_EXTENSION       = 110,
  FLEA_TLS_ALERT_DESC_UNKNOWN_PSK_IDENTITY        = 115,
  FLEA_TLS_ALERT_NO_ALERT                         = 255
} flea_tls__alert_description_t;

typedef enum
{
  FLEA_TLS_ALERT_LEVEL_WARNING = 1,
  FLEA_TLS_ALERT_LEVEL_FATAL   = 2
} flea_tls__alert_level_t;

struct struct_flea_tls_rec_prot_t
{
  flea_tls_conn_state_t        read_state__t;
  flea_tls_conn_state_t        write_state__t;
# ifdef FLEA_HEAP_MODE
  flea_u8_t*                   send_rec_buf_raw__bu8;
  flea_u8_t*                   alt_send_buf__raw__bu8;
# else
  flea_u8_t                    send_rec_buf_raw__bu8[FLEA_TLS_TRNSF_BUF_SIZE + FLEA_TLS_RECORD_HDR_LEN ];
  flea_u8_t                    alt_send_buf__raw__bu8[FLEA_TLS_ALT_SEND_BUF_SIZE];
# endif // ifdef FLEA_HEAP_MODE
  flea_u16_t                   alt_send_buf__raw_len__u16;
  flea_u16_t                   send_rec_buf_raw_len__u16;
  flea_u16_t                   send_buf_raw_len__u16;
  flea_u8_t*                   payload_buf__pu8;
  flea_u8_t*                   send_payload_buf__pu8;
  flea_u8_t*                   send_buf_raw__pu8;
  flea_u16_t                   payload_max_len__u16;
  flea_u16_t                   alt_payload_max_len__u16;
  flea_u16_t                   send_payload_max_len__u16;
  flea_u16_t                   payload_used_len__u16;
  flea_u16_t                   send_payload_used_len__u16;
  flea_u16_t                   payload_offset__u16;
  flea_u16_t                   send_payload_offset__u16;
  flea_tls__protocol_version_t prot_version__t;
  flea_rw_stream_t*            rw_stream__pt;
  flea_u8_t                    write_ongoing__u8;
  flea_u16_t                   read_bytes_from_current_record__u16;
  flea_u16_t                   current_record_content_len__u16;
  flea_u8_t                    is_session_closed__u8;
  flea_u8_t                    is_current_record_alert__u8;
  flea_u8_t                    pending_close_notify__u8;
};

# ifdef FLEA_HEAP_MODE
#  define flea_tls_rec_prot_t__INIT_VALUE {.send_rec_buf_raw__bu8 = 0}
# else
#  define flea_tls_rec_prot_t__INIT_VALUE {.send_rec_buf_raw__bu8[0] = 0}
# endif
# define flea_tls_rec_prot_t__INIT(__p) memset ((__p), 0, sizeof(*(__p))

void flea_tls_rec_prot_t__dtor(flea_tls_rec_prot_t* rec_prot__pt);

flea_err_e THR_flea_tls_rec_prot_t__ctor(
  flea_tls_rec_prot_t* rec_prot__pt,
  flea_al_u8_t         prot_vers_major,
  flea_al_u8_t         prot_vers_minor,
  flea_rw_stream_t*    rw_stream__pt
);

void flea_tls_rec_prot_t__set_null_ciphersuite(
  flea_tls_rec_prot_t*  rec_prot__pt,
  flea_tls_stream_dir_e direction
);


flea_err_e THR_flea_tls_rec_prot_t__write_data(
  flea_tls_rec_prot_t*     rec_prot__pt,
  flea_tls_rec_cont_type_e content_type__e,
  const flea_u8_t*         data__pcu8,
  flea_dtl_t               data_len__dtl
);

flea_err_e THR_flea_tls_rec_prot_t__write_flush(
  flea_tls_rec_prot_t* rec_prot__pt
);

flea_err_e THR_flea_tls_rec_prot_t__read_data(
  flea_tls_rec_prot_t*     rec_prot__pt,
  flea_tls_rec_cont_type_e cont_type__e,
  flea_u8_t*               data__pu8,
  flea_dtl_t*              data_len__pdtl,
  flea_stream_read_mode_e  rd_mode__e
);


flea_err_e THR_flea_tls_rec_prot_t__get_current_record_type(
  flea_tls_rec_prot_t*      rec_prot__pt,
  flea_tls_rec_cont_type_e* cont_type__pe,
  flea_stream_read_mode_e   rd_mode__e
);

flea_err_e THR_flea_tls_rec_prot_t__send_record(
  flea_tls_rec_prot_t*     rec_prot__pt,
  const flea_u8_t*         bytes,
  flea_dtl_t               bytes_len,
  flea_tls_rec_cont_type_e content_type
);

flea_err_e THR_flea_tls_rec_prot_t__send_alert(
  flea_tls_rec_prot_t*          rec_prot__pt,
  flea_tls__alert_description_t description,
  flea_tls__alert_level_t       level
);

flea_err_e THR_flea_tls_rec_prot_t__send_alert_and_throw(
  flea_tls_rec_prot_t*          rec_prot__pt,
  flea_tls__alert_description_t description,
  flea_err_e                    err__t
);

flea_err_e THR_flea_tls_rec_prot_t__set_ciphersuite(
  flea_tls_rec_prot_t*       rec_prot__pt,
  flea_tls_stream_dir_e      direction,
  flea_tls__connection_end_t conn_end__e,
  flea_tls_cipher_suite_id_t suite_id,
  const flea_u8_t*           key_block__pcu8
);

flea_bool_t flea_tls_rec_prot_t__have_done_initial_handshake(const flea_tls_rec_prot_t* rec_prot__pt);

void flea_tls_rec_prot_t__discard_current_read_record(flea_tls_rec_prot_t* rec_prot__pt);

flea_err_e THR_flea_tls_rec_prot_t__close_and_send_close_notify(flea_tls_rec_prot_t* rec_prot__pt);
#endif // ifdef FLEA_HAVE_TLS
#ifdef __cplusplus
}
#endif
#endif /* h-guard */
