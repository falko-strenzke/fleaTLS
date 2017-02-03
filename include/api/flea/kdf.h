/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_kdf__H_
#define _flea_kdf__H_

#include "flea/hash.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Peform the ANSI X9.63 key derivation function.
 *
 * @param hash_id id of the hash algorithm to use in the key derivation function
 * @param input pointer to the input data
 * @param input_len length of input
 * @param shared_info shared info value to be used in the key derivation
 * function, may be NULL, then also its length must be 0
 * @param shared_info_len the length of shared_info
 * @param output pointer to the memory area where to store the computation
 * result
 * @param output_len the caller must provide a pointer to a value which contains
 * the available length of output. when the function returns, *output_len will
 * contain the length of the data set in output
 *
 * @return flea error code
 */
flea_err_t
THR_flea_kdf_X9_63(flea_hash_id_t id, const flea_u8_t *input, flea_al_u16_t input_len, const flea_u8_t *shared_info, flea_al_u16_t shared_info_len, flea_u8_t *output, flea_al_u16_t output_len);

#ifdef __cplusplus
}
#endif

#endif /* h-guard */
