/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_cert_verify__H_
#define _flea_cert_verify__H_


#include "flea/ber_dec.h"
#include "flea/x509.h"
#include "flea/types.h"

#ifdef __cplusplus
extern "C" {
#endif

flea_err_t THR_flea_x509_verify_cert_signature( const flea_u8_t *enc_subject_cert__pcu8, flea_dtl_t enc_subject_cert_len__dtl, const flea_u8_t *enc_issuer_cert__pcu8, flea_dtl_t enc_issuer_cert_len__dtl);

/**
 * @param returned_verifiers_pub_key_params_mbn__prcu8 [out] receives the
 * parameters of the issuer public key
 * @inherited_params_mbn__cprcu8 if set, then these encoded public key
 * parameters are used instead of the one's in the subject's certificate
 */
flea_err_t THR_flea_x509_verify_cert_ref_signature(const flea_x509_cert_ref_t *subject_cert_ref__pt, const flea_x509_cert_ref_t *issuer_cert_ref__t );
flea_err_t THR_flea_x509_verify_cert_ref_signature_inherited_params(const flea_x509_cert_ref_t *subject_cert_ref__pt, const flea_x509_cert_ref_t *issuer_cert_ref__t, flea_ref_cu8_t *returned_verifiers_pub_key_params_mbn__prcu8,  const flea_ref_cu8_t *inherited_params_mbn__cprcu8  );

flea_err_t THR_flea_x509_verify_signature(const flea_x509_algid_ref_t *alg_id__pt, const flea_x509_public_key_info_t *public_key_info__pt, const flea_der_ref_t* tbs_data__pt, const flea_der_ref_t *signature__pt, flea_ref_cu8_t *returned_verifiers_pub_key_params_mbn__prcu8,  const flea_ref_cu8_t *inherited_params_mbn__cprcu8  );

#ifdef __cplusplus
}
#endif
#endif /* h-guard */

