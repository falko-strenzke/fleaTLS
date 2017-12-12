/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_tls_const__H_
#define _flea_tls_const__H_

#define FLEA_TLS_CERT_TYPE_RSA_SIGN   1
#define FLEA_TLS_CERT_TYPE_ECDSA_SIGN 64

#define FLEA_TLS_HELLO_RANDOM_SIZE    32
#define FLEA_TLS_MASTER_SECRET_SIZE   48


#define FLEA_CONST_TLS_GCM_RECORD_IV_LEN 8
#define FLEA_CONST_TLS_GCM_FIXED_IV_LEN  4
#define FLEA_CONST_TLS_GCM_TAG_LEN       16


#define FLEA_TLS_MAX_MAC_SIZE     FLEA_MAC_MAX_OUTPUT_LENGTH
#define FLEA_TLS_MAX_MAC_KEY_SIZE __FLEA_COMPUTED_MAC_MAX_KEY_LEN
#define FLEA_TLS_MAX_IV_SIZE      FLEA_MAX(FLEA_CIPHER_MAX_BLOCK_LEN, FLEA_CONST_TLS_GCM_RECORD_IV_LEN)

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif
#endif /* h-guard */
