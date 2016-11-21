/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */


#include "internal/common/default.h"
#include "flea/error_handling.h"
#include "flea/error.h"
#include "flea/alloc.h"
#include "flea/cert_verify.h"
#include "flea/ber_dec.h"
#include "test_data_x509_certs.h"

#include <string.h>

#ifdef FLEA_HAVE_ASYM_SIG

#ifdef FLEA_HAVE_RSA
flea_err_t THR_flea_test_cert_verify_rsa()
{

  FLEA_THR_BEG_FUNC();
#if defined FLEA_USE_STACK_BUF && (FLEA_RSA_MAX_KEY_BIT_SIZE >= 4096)
  FLEA_CCALL(THR_flea_x509_verify_cert_signature(test_cert_tls_server_1, sizeof(test_cert_tls_server_1), flea_test_cert_issuer_of_tls_server_1__cau8, sizeof(flea_test_cert_issuer_of_tls_server_1__cau8)));
#endif
  FLEA_THR_FIN_SEC_empty();
   
}
#endif

#ifdef FLEA_HAVE_ECDSA
flea_err_t THR_flea_test_cert_verify_ecdsa()
{
  FLEA_THR_BEG_FUNC();
#if defined FLEA_HAVE_ECDSA && FLEA_ECC_MAX_MOD_BIT_SIZE >= 384
  FLEA_CCALL(THR_flea_x509_verify_cert_signature(test_self_signed_ecdsa_csca_cert__acu8, sizeof(test_self_signed_ecdsa_csca_cert__acu8), test_self_signed_ecdsa_csca_cert__acu8, sizeof(test_self_signed_ecdsa_csca_cert__acu8)));
#endif
  FLEA_THR_FIN_SEC_empty();
   
}
#endif


#endif /* #ifdef FLEA_HAVE_ASYM_SIG */
