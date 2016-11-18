/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */


#include "internal/common/default.h"
#include "flea/error_handling.h"
#include "flea/ber_dec.h"
#include "flea/x509.h"
#include "flea/pk_api.h"
#include "flea/alloc.h"
#include "flea/hash.h"
#include "flea/array_util.h"
#include "flea/cert_verify.h"
#include "flea/namespace_asn1.h"
#include "flea/ecc.h"
#include "flea/ec_key.h"
#include "flea/ecc_named_curves.h"
#include "flea/pubkey.h"

#ifdef FLEA_HAVE_ASYM_SIG

/* ... rsadsi: 1.2.840.113549 */
//const flea_u8_t rsadsi_oid_prefix__cau8[] = { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01  };
//subsequent 2 bytes determine encoding method
// ...1 => PKCS
//    ...1 PKCS#1
const flea_u8_t pkcs1_oid_prefix__cau8[] = { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01  };
//
//    ...7 OAEP (same hash function as for message hash?)
// 
// the following and last byte determines the hash algorithm:
//         5 => sha1
//        14 => sha224
// 0x0B = 11 => sha256
//        12 => sha384
//        13 => sha512
//
/* ANSI X9.62 Elliptic Curve Digital Signature Algorithm (ECDSA) algorithm with Secure Hash Algorithm, revision 2 (SHA2)  */
const flea_u8_t ecdsa_oid_prefix__acu8[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04 };
/* subsequent octets: 
 *                    3 => sha2
 *                      subsequent: specific sha2 variant
 *                    4.1 => sha1
 */


flea_err_t THR_flea_x509_verify_cert_signature( const flea_u8_t *enc_subject_cert__pcu8, flea_dtl_t enc_subject_cert_len__dtl, const flea_u8_t *enc_issuer_cert__pcu8, flea_dtl_t enc_issuer_cert_len__dtl)
{
  FLEA_DECL_OBJ(subj_ref__t, flea_x509_cert_ref_t);
  FLEA_DECL_OBJ(iss_ref__t, flea_x509_cert_ref_t);
 FLEA_THR_BEG_FUNC();
  FLEA_CCALL(THR_flea_x509_cert_ref_t__ctor(&subj_ref__t, enc_subject_cert__pcu8, enc_subject_cert_len__dtl));
  FLEA_CCALL(THR_flea_x509_cert_ref_t__ctor(&iss_ref__t, enc_issuer_cert__pcu8, enc_issuer_cert_len__dtl));
FLEA_CCALL(THR_flea_x509_verify_cert_ref_signature(&subj_ref__t, &iss_ref__t));
 FLEA_THR_FIN_SEC(
   flea_x509_cert_ref_t__dtor(&subj_ref__t); 
   flea_x509_cert_ref_t__dtor(&iss_ref__t); 
     ); 
}

flea_err_t THR_flea_x509_verify_cert_ref_signature(const flea_x509_cert_ref_t *subject_cert_ref__pt, const flea_x509_cert_ref_t *issuer_cert_ref__t)
{
 return THR_flea_x509_verify_cert_ref_signature_inherited_params(subject_cert_ref__pt, issuer_cert_ref__t, NULL, NULL);
}
flea_err_t THR_flea_x509_verify_cert_ref_signature_inherited_params(const flea_x509_cert_ref_t *subject_cert_ref__pt, const flea_x509_cert_ref_t *issuer_cert_ref__t, flea_ref_cu8_t *returned_verifiers_pub_key_params_mbn__prcu8,  const flea_ref_cu8_t *inherited_params_mbn__cprcu8  )
{ 
  flea_der_ref_t sig_content__t;
 FLEA_THR_BEG_FUNC();
 //if(subject_cert_ref__pt->subject_unique_id_as_bitstr__t.len__dtl < 2)   
//if(subject_cert_ref__pt->cert_signature_as_bit_string__t.data__pcu8
 FLEA_CCALL(THR_flea_ber_dec__get_ref_to_bit_string_content_no_unused_bits(&subject_cert_ref__pt->cert_signature_as_bit_string__t, &sig_content__t));
FLEA_CCALL( THR_flea_x509_verify_signature( // TODO: NEED TO RECOGNIZE SIGN OF MOD AND EXP INTS
      &subject_cert_ref__pt->tbs_sig_algid__t, 
      &issuer_cert_ref__t->subject_public_key_info__t,
      &subject_cert_ref__pt->tbs_ref__t,
      &sig_content__t,
      returned_verifiers_pub_key_params_mbn__prcu8,
      inherited_params_mbn__cprcu8
      ));
       
 FLEA_THR_FIN_SEC_empty(); 
  
}

flea_err_t THR_get_hash_id_from_x509_id_for_rsa(flea_u8_t cert_id__u8, flea_hash_id_t* result__pt)
{
  FLEA_THR_BEG_FUNC();
  switch(cert_id__u8)
  {
    case 5: 
      *result__pt = flea_sha1;
      break;
    case 14:
      *result__pt = flea_sha224;
      break;
    case 11:
      *result__pt = flea_sha256;
      break;
    case 12:
      *result__pt = flea_sha384;
      break;
    case 13:
      *result__pt = flea_sha512;
      break;
    default:
      FLEA_THROW("unrecognized hash function", FLEA_ERR_X509_UNRECOG_HASH_FUNCTION);

  }
  FLEA_THR_FIN_SEC_empty(); 
}

flea_err_t THR_get_hash_id_from_x509_id_for_ecdsa(const flea_u8_t cert_id__pcu8[2], flea_hash_id_t* result__pt)
{
  FLEA_THR_BEG_FUNC();
  if(cert_id__pcu8[0] == 3)
  {
    /* sha2 */
    switch(cert_id__pcu8[1])
    {
    case 1:
      *result__pt = flea_sha224;
      break;
    case 2:
      *result__pt = flea_sha256;
      break;
    case 3:
      *result__pt = flea_sha384;
      break;
    case 4:
      *result__pt = flea_sha512;
      break;
    default:
    FLEA_THROW("unsupported ECDSA variant", FLEA_ERR_X509_UNSUPP_ALGO_VARIANT);
    }
  }else if (cert_id__pcu8[0] == 4 && cert_id__pcu8[1] == 1)
  {
    *result__pt = flea_sha1;
  }
  else
  {
    FLEA_THROW("unsupported ECDSA variant", FLEA_ERR_X509_UNSUPP_ALGO_VARIANT);
  }
  
  FLEA_THR_FIN_SEC_empty(); 
}

#ifdef FLEA_HAVE_ECC

#if 0
static flea_err_t THR_flea_x509_parse_ecc_public_key(const flea_der_ref_t *encoded_public_key_value__pt, flea_ecc_pub_key_t * key_to_set__pt)
{

  /*FLEA_DECL_OBJ(source__t, flea_data_source_t);
  FLEA_DECL_OBJ(dec__t, flea_ber_dec_t);
  flea_data_source_mem_help_t hlp__t;*/
  FLEA_THR_BEG_FUNC();
  //printf("ecc param len = %u\n", encoded_parameters__pt->len__dtl);

  /*FLEA_CCALL(THR_flea_data_source_t__ctor_memory(&source__t, encoded_public_key_value__pt->data__pcu8, encoded_public_key_value__pt->len__dtl, &hlp__t));
  FLEA_CCALL(THR_flea_ber_dec_t__ctor(&dec__t, &source__t, 0));*/
   
   FLEA_CCALL(THR_flea_ec_key__decode_uncompressed_point(encoded_public_key_value__pt, &key_to_set__pt->pub_point_x__cru8, &key_to_set__pt->pub_point_y__cru8));
  FLEA_THR_FIN_SEC_empty(
      /*flea_data_source_t__dtor(&source__t); 
      flea_ber_dec_t__dtor(&dec__t);*/
      );
}
#endif


static flea_err_t THR_flea_x509_verify_ecdsa_signature(const flea_der_ref_t *oid_ref__pt, const flea_public_key_t *ver_key__pt, /*const flea_x509_public_key_info_t *public_key_info__pt,*/ /*const flea_der_ref_t *public_key_value__pt,*/ const flea_der_ref_t *der_enc_signature__pt, const flea_der_ref_t *tbs_data__pt)
{

  flea_hash_id_t ecdsa_hash_id__t;
  //flea_der_ref_t concat_sig_ref__t;
  //flea_public_key_t ver_key__t;
  FLEA_THR_BEG_FUNC();
  /* allocating DER encoded size wastes a few bytes of RAM but saves some code */
  FLEA_CCALL(THR_get_hash_id_from_x509_id_for_ecdsa(oid_ref__pt->data__pcu8 + sizeof(ecdsa_oid_prefix__acu8), &ecdsa_hash_id__t));
  FLEA_CCALL(THR_flea_public_key_t__verify_signature(ver_key__pt, flea_ecdsa_emsa1, tbs_data__pt, der_enc_signature__pt, ecdsa_hash_id__t));
  // TODO: REMOVE 
  //FLEA_CCALL(THR_flea_x509_parse_ecc_public_params(&public_key_info__pt->algid__t.params_ref_as_tlv__t, &pk_par__u.ecc_dom_par__t));
  //
  //decode the signature:
  FLEA_THR_FIN_SEC(
      );
}
#endif /* #ifdef FLEA_HAVE_ECC */


flea_err_t THR_flea_x509_verify_signature(const flea_x509_algid_ref_t *alg_id__pt, const flea_x509_public_key_info_t *public_key_info__pt, const flea_der_ref_t* tbs_data__pt, const flea_der_ref_t *signature__pt, flea_ref_cu8_t *returned_verifiers_pub_key_params_mbn__prcu8,  const flea_ref_cu8_t *inherited_params_mbn__cprcu8  )
{
  const flea_der_ref_t *oid_ref__pt = &alg_id__pt->oid_ref__t;
  /*FLEA_DECL_OBJ(key_dec__t, flea_ber_dec_t);
  FLEA_DECL_OBJ(source__t, flea_data_source_t);*/
  //flea_data_source_mem_help_t hlp__t;
flea_public_key_t ver_key__t = flea_publick_key_t__INIT_VALUE;
  //flea_der_ref_t public_key_value__t; /* actual representation of the public key */
  //flea_der_ref_t public_key_as_bitstr__t;
  // TODO: MAKE SIMPLE VERIFY FUNCTION:
  //FLEA_DECL_OBJ(verifier__t, flea_pk_signer_t);
  FLEA_THR_BEG_FUNC();
#if 0
  FLEA_CCALL(THR_flea_data_source_t__ctor_memory(&source__t, public_key_info__pt->public_key_as_tlv__t.data__pcu8, public_key_info__pt->public_key_as_tlv__t.len__dtl, &hlp__t));
  FLEA_CCALL(THR_flea_ber_dec_t__ctor(&key_dec__t, &source__t, 0)); // TODO: SET LIMIT (ALSO ELSEWHERE)

  /* valid for both ECDSA and RSA */
    FLEA_CCALL(THR_flea_ber_dec_t__get_ref_to_raw_cft(&key_dec__t, FLEA_ASN1_CFT_MAKE2(FLEA_ASN1_UNIVERSAL_PRIMITIVE, FLEA_ASN1_BIT_STRING), &public_key_as_bitstr__t));
    FLEA_CCALL(THR_flea_ber_dec__get_ref_to_bit_string_content_no_unused_bits(&public_key_as_bitstr__t, &public_key_value__t));
  /* determine primitive */
#endif

  //FLEA_CCALL(THR_flea_public_key_t__ctor(&ver_key__t, flea_ecc, &public_key_info__pt->public_key_as_tlv__t, &public_key_info__pt->algid__t.params_ref_as_tlv__t));
    // TODO: IFDEF RSA
  
    if(returned_verifiers_pub_key_params_mbn__prcu8)
    {
      *returned_verifiers_pub_key_params_mbn__prcu8 = alg_id__pt->params_ref_as_tlv__t;
    }
  if(((oid_ref__pt->len__dtl == sizeof(pkcs1_oid_prefix__cau8) + 1)) && !memcmp(oid_ref__pt->data__pcu8, pkcs1_oid_prefix__cau8, sizeof(pkcs1_oid_prefix__cau8)))
  {
    //flea_pub_key_param_u pk_par__u;
    //flea_der_ref_t public_mod__t;
    //flea_der_ref_t public_exp__t;
    flea_hash_id_t hash_id__t; 
    /* create hash ctx */

    FLEA_CCALL(THR_flea_public_key_t__ctor(&ver_key__t, flea_rsa_key,  &public_key_info__pt->public_key_as_tlv__t, NULL));
    FLEA_CCALL(THR_get_hash_id_from_x509_id_for_rsa(oid_ref__pt->data__pcu8[sizeof(pkcs1_oid_prefix__cau8)], &hash_id__t));

    //FLEA_CCALL(THR_flea_x509_parse_rsa_public_key(&public_key_value__t, &public_mod__t, &pk_par__u.rsa_public_exp__ru8));
  //pk_par__u.rsa_public_exp__ru8 = ver_key__t.pubkey_with_params__u.rsa_public_val__t.pub_exp__rcu8;

  FLEA_CCALL(THR_flea_public_key_t__verify_signature(&ver_key__t, flea_rsa_pkcs1_v1_5_sign, tbs_data__pt, signature__pt, hash_id__t));
    /*FLEA_CCALL(THR_flea_pk_api__verify_signature(
          tbs_data__pt,
          signature__pt,
          //&public_mod__t,
         &ver_key__t.pubkey_with_params__u.rsa_public_val__t.mod__rcu8,
          flea_rsa_pkcs1_v1_5_sign, // TODO: GENERALIZE
          hash_id__t,
          &pk_par__u
        //&ver_key__t.pubkey_with_params__u.rsa_public_val__t.pub_exp__rcu8
          ));*/


  } 
#ifdef FLEA_HAVE_ECC
  else if(oid_ref__pt->len__dtl == sizeof(ecdsa_oid_prefix__acu8) + 2 && !memcmp(oid_ref__pt->data__pcu8, ecdsa_oid_prefix__acu8, sizeof(ecdsa_oid_prefix__acu8)))
  {
flea_bool_t are_keys_params_implicit__b;
    //const flea_ref_cu8_t *params_to_use__prcu8 = (inherited_params_mbn__cprcu8 != NULL) ? inherited_params_mbn__cprcu8 : &public_key_info__pt->algid__t.params_ref_as_tlv__t;
    FLEA_CCALL(THR_flea_public_key_t__ctor_inherited_params(&ver_key__t, flea_ecc_key, &public_key_info__pt->public_key_as_tlv__t, &public_key_info__pt->algid__t.params_ref_as_tlv__t, inherited_params_mbn__cprcu8, &are_keys_params_implicit__b));
    if(are_keys_params_implicit__b && returned_verifiers_pub_key_params_mbn__prcu8)
    {
      returned_verifiers_pub_key_params_mbn__prcu8->data__pcu8 = NULL;
      returned_verifiers_pub_key_params_mbn__prcu8->len__dtl = 0;
    }
    FLEA_CCALL(THR_flea_x509_verify_ecdsa_signature(oid_ref__pt, &ver_key__t, /*public_key_info__pt, &public_key_value__t,*/ signature__pt, tbs_data__pt ));
  }
#endif /* #ifdef FLEA_HAVE_ECC */
  else
  {
    FLEA_THROW("unsupported primitive", FLEA_ERR_X509_UNSUPP_PRIMITIVE);
  }

  FLEA_THR_FIN_SEC(
      //flea_pk_signer_t__dtor(&verifier__t);
      flea_public_key_t__dtor(&ver_key__t);
/*      flea_data_source_t__dtor(&source__t); 
      flea_ber_dec_t__dtor(&key_dec__t);*/
      ); 

}

#endif /* #ifdef FLEA_HAVE_ASYM_SIG */
