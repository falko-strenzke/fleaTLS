/* fleaTLS cryptographic library
Copyright (C) 2015-2019 cryptosource GmbH

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#ifndef _flea_rsa__H_
#define _flea_rsa__H_

#ifdef __cplusplus
extern "C" {
#endif


#include "internal/common/default.h"
#include  "flea/types.h"


/**
 *  RSA raw private operation using chinese remainder theorem.
 *  @param result_enc array to receive the big endian encoded exponentiation
 *  result. Must have length of the modulus (modulus_length), may be equal to
 *  base (then base_length must be equal to modulus_length).
 *  @param base_enc big endian encoded base used in the exponentiation
 *  @param base_length length of the base_enc array
 *  @param modulus_length byte length of the modulus
 *  @param p_enc big endian encoded prime p of length modulus_length/2
 *  @param p_enc_len length of p_enc
 *  @param q_enc big endian encoded prime q of length modulus_length/2
 *  @param q_enc_len length of q_enc
 *  @param d1_enc big endian encoded exponent d mod (p-1) of length modulus_length/2
 *  @param d1_enc_len length of d1_enc
 *  @param d2_enc big endian encoded exponent d mod (q-1) of length modulus_length/2
 *  @param d2_enc_len length of d2_enc
 *  @param c_enc big endian encoded q^(-1) mod p of length modulus_length/2
 *  @param c_enc_len length of c_enc
 *
 *  @return error code
 *
 */
flea_err_e THR_flea_rsa_raw_operation_crt(
  flea_u8_t*       result_enc,
  const flea_u8_t* base_enc,
  flea_al_u16_t    base_length,
  flea_al_u16_t    modulus_length,
  const flea_u8_t* p_enc,
  flea_mpi_ulen_t  p_enc_len,
  const flea_u8_t* q_enc,
  flea_mpi_ulen_t  q_enc_len,
  const flea_u8_t* d1_enc,
  flea_mpi_ulen_t  d1_enc_len,
  const flea_u8_t* d2_enc,
  flea_mpi_ulen_t  d2_enc_len,
  const flea_u8_t* c_enc,
  flea_mpi_ulen_t  c_enc_len
);


#ifdef __cplusplus
}
#endif

#endif /* h-guard */
