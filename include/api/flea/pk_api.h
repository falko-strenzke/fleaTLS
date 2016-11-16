/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_pk_api__H_
#define _flea_pk_api__H_

#include "internal/common/default.h"
#include "flea/types.h"
#include "flea/hash.h"
#include "internal/common/pk_api_int.h"
#include "flea/ec_gfp_dom_par.h"
#include "flea/pubkey.h"

#ifdef __cplusplus
extern "C" {
#endif



#define FLEA_PK_ID_OFFS_PRIMITIVE 4

/**
 * Supported encryption and signature public key primitives.
 */
typedef enum
{
  flea_rsa_sign = 0 << FLEA_PK_ID_OFFS_PRIMITIVE,
  flea_rsa_encr = 1 << FLEA_PK_ID_OFFS_PRIMITIVE,
  flea_ecdsa = 2 << FLEA_PK_ID_OFFS_PRIMITIVE
} flea_pk_primitive_id_t;

/**
 * Supported public key encoding schemes.
 */
typedef enum { flea_emsa1 = 0, flea_pkcs1_v1_5 = 1, flea_oaep = 2 } flea_pk_encoding_id_t;

/**
 * Supported public key encryption and signature configurations.
 */
typedef enum
{
  flea_ecdsa_emsa1 = flea_ecdsa | flea_emsa1,
  flea_rsa_oaep_encr = flea_rsa_encr | flea_oaep,
  flea_rsa_pkcs1_v1_5_encr = flea_rsa_encr | flea_pkcs1_v1_5,
  flea_rsa_pkcs1_v1_5_sign = flea_rsa_sign | flea_pkcs1_v1_5,
} flea_pk_scheme_id_t;

typedef enum { flea_rsa_key, flea_ecc_key } flea_pub_key_type_t;

// TODO: provide key type parameter for params ( put the union into an outer
// type with that field) 
// AND make setter for RSA at least
typedef union
{
 flea_ref_u8_t rsa_public_exp__ru8;

#ifdef FLEA_HAVE_ECC
 flea_ec_gfp_dom_par_ref_t ecc_dom_par__t;
#endif /* #ifdef FLEA_HAVE_ECC */
} flea_pub_key_param_u;

/**
 * Supported
 */
typedef enum { flea_sign, flea_verify } flea_pk_signer_direction_t;

struct struct_flea_pk_config_t;
typedef struct struct_flea_pk_config_t flea_pk_config_t;

/**
 * Public signer struct. Used to perform signature generation and verification.
 */
typedef struct
{
  flea_hash_ctx_t hash_ctx;
  flea_hash_id_t hash_id__t;
} flea_pk_signer_t;


#define flea_pk_signer_t__INIT_VALUE { .hash_ctx = flea_hash_ctx_t__INIT_VALUE }

#ifdef FLEA_USE_HEAP_BUF
#define flea_pk_signer_t__INIT(__p) do { flea_hash_ctx_t__INIT(&(__p)->hash_ctx); } while(0)
#else
/* needed for secret wiping in hash ctx*/
#define flea_pk_signer_t__INIT(__p) do { flea_hash_ctx_t__INIT(&(__p)->hash_ctx); } while(0) 
#endif


/**
 * Construct a public key signer object. Can be used signature generation or
 * verification.
 *
 * @param signer the signer object to create
 * @param hash_id the ID of the hash algorithm to use in the public key scheme
 * to hash the message
 *
 * @return flea error code
 */
flea_err_t THR_flea_pk_signer_t__ctor(flea_pk_signer_t* signer, flea_hash_id_t hash_id);

/**
 * Destroy a public key signer object.
 *
 * @param signer the signer object to destroy
 */
void flea_pk_signer_t__dtor(flea_pk_signer_t* signer);

/**
 * Update a public key signer object with signature data.
 *
 * @param signer the signer object to use
 * @param message pointer to the message data
 * @param message_len the length of message
 *
 * @return flea error code
 */
flea_err_t THR_flea_pk_signer_t__update(flea_pk_signer_t* signer, const flea_u8_t* message, flea_al_u16_t message_len);


/**
 * Finalize the signature verification.
 *
 * @param signer the signer object to use
 * @param id the ID of the signature scheme to use
 * @param key pointer to the public key to be used in the operation
 * @param key_len the length of key
 * @param params the parameters to be used for the public key operation. 
 *    in case of ECDSA, a pointer to the domain parameters in flea's internal
 *    format must be provided. in case of RSA, the public exponent must be
 *    provided.
 * @param params_len the length of params
 * @param signature pointer to the memory area for the signature to be verified.
 * @param signature_len length of signature
 * @return flea error code FLEA_ERR_FINE indicates successful verification and FLEA_ERR_INV_SIGNATURE indicates a
 * failed signature verification
 */
//flea_err_t THR_flea_pk_signer_t__final_verify(flea_pk_signer_t* signer, flea_pk_scheme_id_t id, const flea_u8_t* key, flea_al_u16_t key_len, const flea_u8_t* params, flea_al_u16_t params_len, const flea_u8_t* signature, flea_al_u16_t signature_len);
flea_err_t THR_flea_pk_signer_t__final_verify (flea_pk_signer_t* signer__pt, flea_pk_scheme_id_t id__t, const flea_u8_t* key__pcu8, flea_al_u16_t key_len__alu16, const flea_u8_t* signature__pu8, flea_al_u16_t signature_len__alu16, const flea_pub_key_param_u *param__pu);


flea_err_t THR_flea_pk_api__verify_signature(const flea_ref_cu8_t *message__prcu8, const flea_ref_cu8_t * signature__prcu8, const flea_ref_cu8_t *key__rcu8, flea_pk_scheme_id_t pk_scheme_id__t, flea_hash_id_t hash_id__t, const flea_pub_key_param_u *param__pu);

/**
 * Finalize the signature generation.
 *
 * @param signer the signer object to use
 * @param id the ID of the signature scheme to use
 * @param key pointer to the private key to be used in the operation
 * @param key_len the length of key
 * @param params the parameters to be used for the public key operation
 *    in case of ECDSA, a pointer to the domain parameters in flea's internal
 *    format must be provided. in case of RSA, the public exponent must be
 *    provided.
 * @param params_len the length of params
 * @param signature pointer to the memory area for the signature. this memory area will receive the generated signature.
 * @param signature_len this pointer must
 * point to the available length of the buffer signature, upon function return, the value
 * of the pointer target will be updated to the number of actual signature bytes written.
 * @return flea error code
 */
//flea_err_t THR_flea_pk_signer_t__final_sign(flea_pk_signer_t* signer, flea_pk_scheme_id_t id, const flea_u8_t* key, flea_al_u16_t key_len, const flea_u8_t* params, flea_al_u16_t params_len, flea_u8_t* signature, flea_al_u16_t* signature_len);
flea_err_t THR_flea_pk_signer_t__final_sign (flea_pk_signer_t* signer__pt, flea_pk_scheme_id_t id__t, const flea_u8_t* key__pcu8, flea_al_u16_t key_len__alu16, flea_u8_t* signature__pu8, flea_al_u16_t* signature_len__palu16, const flea_pub_key_param_u *param__pu);

/**
 *  Encrypt a message using a public key scheme.
 *
 *  @param id ID of the encryption scheme to use
 *  @param hash_id ID of the hash scheme to use (if applicable)
 *  @param message the message to be encrypted
 *  @param message_len the length of message
 *  @param result buffer to store the ciphertext
 *  @param result_len must point to a variable representing the length available in result, after function return it will hold the
 *  number of bytes written to result
 *  @param key the public key to use for the encryption
 *  @param key_len the length of key
 *  @param params public parameters associated with the key
 *  @param params_len the length of params
 */
flea_err_t THR_flea_pk_api__encrypt_message( flea_pk_scheme_id_t id, flea_hash_id_t hash_id, const flea_u8_t* message, flea_al_u16_t message_len, flea_u8_t* result, flea_al_u16_t* result_len, const flea_u8_t* key, flea_al_u16_t key_len, const flea_u8_t* params, flea_al_u16_t params_len);

/**
 *  Decrypt a message using a public key scheme.
 *
 *  @param id ID of the encryption scheme to use
 *  @param hash_id ID of the hash scheme to use (if applicable)
 *  @param ciphertext the ciphertext to be encrypted
 *  @param ciphertext_len the length of ciphertext
 *  @param result buffer to store the plaintext
 *  @param result_len must point to a variable representing the length available in result, after function return it will hold the
 *  number of bytes written to result
 *  @param key the private key to use for the decryption
 *  @param key_len the length of key
 *  @param params public parameters associated with the key
 *  @param params_len the length of params
 */
flea_err_t THR_flea_pk_api__decrypt_message( flea_pk_scheme_id_t id, flea_hash_id_t hash_id, const flea_u8_t* ciphertext, flea_al_u16_t ciphertext_len, flea_u8_t* result, flea_al_u16_t* result_len, const flea_u8_t* key, flea_al_u16_t key_len, const flea_u8_t* params, flea_al_u16_t params_len);

#ifdef __cplusplus
}
#endif

#endif /* h-guard */
