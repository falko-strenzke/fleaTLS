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


#include "internal/common/default.h"
#include "flea/error_handling.h"
#include "flea/error.h"
#include "flea/alloc.h"
#include "flea/rsa.h"
#include "flea/block_cipher.h"
#include "self_test.h"
#include <string.h>

flea_err_e THR_flea_test_cbc_mode_aes()
{
  // from https://tools.ietf.org/html/rfc3602
  const flea_u8_t aes128_cbc_key[] = {0x56, 0xe4, 0x7a, 0x38, 0xc5, 0x59, 0x89, 0x74, 0xbc, 0x46, 0x90, 0x3d, 0xba, 0x29, 0x03, 0x49};
  const flea_u8_t aes128_cbc_iv[]  = {0x8c, 0xe8, 0x2e, 0xef, 0xbe, 0xa0, 0xda, 0x3c, 0x44, 0x69, 0x9e, 0xd7, 0xdb, 0x51, 0xb7, 0xd9};


  const flea_u8_t aes128_cbc_pt[] = {
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf
  };

  const flea_u8_t aes128_cbc_exp_ct[] = {
    0xc3, 0x0e, 0x32, 0xff, 0xed, 0xc0, 0x77, 0x4e, 0x6a, 0xff, 0x6a, 0xf0, 0x86, 0x9f, 0x71, 0xaa,
    0x0f, 0x3a, 0xf0, 0x7a, 0x9a, 0x31, 0xa9, 0xc6, 0x84, 0xdb, 0x20, 0x7e, 0xb0, 0xef, 0x8e, 0x4e,
    0x35, 0x90, 0x7a, 0xa6, 0x32, 0xc3, 0xff, 0xdf, 0x86, 0x8b, 0xb7, 0xb2, 0x9d, 0x3d, 0x46, 0xad,
    0x83, 0xce, 0x9f, 0x9a, 0x10, 0x2e, 0xe9, 0x9d, 0x49, 0xa5, 0x3e, 0x87, 0xf4, 0xc3, 0xda, 0x55
  };

  const flea_u8_t* in_ptr__pcu8;
  flea_u8_t* out_ptr__pu8;

  flea_cbc_mode_ctx_t encr_ctx__t;

  flea_cbc_mode_ctx_t__INIT(&encr_ctx__t);
#ifdef FLEA_HAVE_AES_BLOCK_DECR
  flea_cbc_mode_ctx_t decr_ctx__t;
  flea_cbc_mode_ctx_t__INIT(&decr_ctx__t);
#endif
  FLEA_DECL_BUF(encr__bu8, flea_u8_t, sizeof(aes128_cbc_pt));
  FLEA_DECL_BUF(decr__bu8, flea_u8_t, sizeof(aes128_cbc_pt));

  flea_u8_t block_len__u8 = 16; // AES
  FLEA_THR_BEG_FUNC();

  FLEA_ALLOC_BUF(encr__bu8, sizeof(aes128_cbc_pt));
  FLEA_ALLOC_BUF(decr__bu8, sizeof(aes128_cbc_pt));

  FLEA_CCALL(THR_flea_cbc_mode__encrypt_data(flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), aes128_cbc_pt, encr__bu8, sizeof(aes128_cbc_pt)));
  if(memcmp(encr__bu8, aes128_cbc_exp_ct, sizeof(aes128_cbc_exp_ct)))
  {
    FLEA_THROW("error with CBC encrypted result (1)", FLEA_ERR_FAILED_TEST);
  }
#ifdef FLEA_HAVE_AES_BLOCK_DECR
  FLEA_CCALL(THR_flea_cbc_mode__decrypt_data(flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), encr__bu8, decr__bu8, sizeof(aes128_cbc_pt)));

  if(memcmp(decr__bu8, aes128_cbc_pt, sizeof(aes128_cbc_pt)))
  {
    FLEA_THROW("error with CBC decrypted result (1)", FLEA_ERR_FAILED_TEST);
  }
#else // #ifdef FLEA_HAVE_AES_BLOCK_DECR
  if(FLEA_ERR_INV_ALGORITHM != THR_flea_cbc_mode__decrypt_data(flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), encr__bu8, decr__bu8, sizeof(aes128_cbc_pt)))
  {
    FLEA_THROW("error with unsupported decryption", FLEA_ERR_FAILED_TEST);
  }
#endif // #else of #ifdef FLEA_HAVE_AES_BLOCK_DECR

  // now try update functionality

  memset(encr__bu8, 0, sizeof(aes128_cbc_pt));
  memset(decr__bu8, 0, sizeof(aes128_cbc_pt));
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__ctor(&encr_ctx__t, flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), flea_encrypt));
#ifdef FLEA_HAVE_AES_BLOCK_DECR
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__ctor(&decr_ctx__t, flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), flea_decrypt));
#endif // #ifdef FLEA_HAVE_AES_BLOCK_DECR

  in_ptr__pcu8 = aes128_cbc_pt;
  out_ptr__pu8 = encr__bu8;
  ;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  in_ptr__pcu8 += block_len__u8;
  out_ptr__pu8 += block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, 2 * block_len__u8));
  in_ptr__pcu8 += 2 * block_len__u8;
  out_ptr__pu8 += 2 * block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  if(memcmp(encr__bu8, aes128_cbc_exp_ct, sizeof(aes128_cbc_exp_ct)))
  {
    FLEA_THROW("error with CBC encrypted result (2)", FLEA_ERR_FAILED_TEST);
  }

#ifdef FLEA_HAVE_AES_BLOCK_DECR
  in_ptr__pcu8 = encr__bu8;
  out_ptr__pu8 = encr__bu8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&decr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  in_ptr__pcu8 += block_len__u8;
  out_ptr__pu8 += block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&decr_ctx__t, in_ptr__pcu8, out_ptr__pu8, 2 * block_len__u8));
  in_ptr__pcu8 += 2 * block_len__u8;
  out_ptr__pu8 += 2 * block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&decr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));

  if(memcmp(encr__bu8, aes128_cbc_pt, sizeof(aes128_cbc_pt)))
  {
    FLEA_THROW("error with CBC decrypted result (3)", FLEA_ERR_FAILED_TEST);
  }
#endif // #ifdef FLEA_HAVE_AES_BLOCK_DECR

  // set up encryptor again
  flea_cbc_mode_ctx_t__dtor(&encr_ctx__t);
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__ctor(&encr_ctx__t, flea_aes128, aes128_cbc_key, sizeof(aes128_cbc_key), aes128_cbc_iv, sizeof(aes128_cbc_iv), flea_encrypt));
  // encrypt in place
  in_ptr__pcu8 = aes128_cbc_pt;
  out_ptr__pu8 = encr__bu8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  in_ptr__pcu8 += block_len__u8;
  out_ptr__pu8 += block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, 2 * block_len__u8));
  in_ptr__pcu8 += 2 * block_len__u8;
  out_ptr__pu8 += 2 * block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  if(memcmp(encr__bu8, aes128_cbc_exp_ct, sizeof(aes128_cbc_exp_ct)))
  {
    FLEA_THROW("error with CBC encrypted result (4)", FLEA_ERR_FAILED_TEST);
  }

  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(encr__bu8);
    FLEA_FREE_BUF_FINAL(decr__bu8);
    flea_cbc_mode_ctx_t__dtor(&encr_ctx__t);
    FLEA_DO_IF_HAVE_AES_BLOCK_DECR(
      flea_cbc_mode_ctx_t__dtor(&decr_ctx__t);
    );
  );
} /* THR_flea_test_cbc_mode */

#ifdef FLEA_HAVE_DES
flea_err_e THR_flea_test_cbc_mode_3des()
{
  const flea_u8_t tdes_cbc_key[] = {
    0x81, 0xF8, 0xE1, 0xEC, 0xCD, 0xFB, 0xBC, 0xE1, 0xE2, 0xFB, 0x52, 0x3C, 0xDA,
    0xB3, 0x2B, 0x10, 0xB4, 0x79, 0xAB, 0x53, 0xD9, 0x81, 0x9D, 0xEF
  };
  const flea_u8_t tdes_cbc_iv[] = {
    0x07, 0xDD, 0x42, 0x78, 0x83, 0xB7, 0x49, 0x70
  };


  const flea_u8_t tdes_cbc_pt[16] = {
    'C', 'B', 'C', ' ', 'M', 'o', 'd', 'e', ' ', 'T', 'e', 's', 't', 3, 3, 3
  };

  const flea_u8_t tdes_cbc_exp_ct[16] = {
    0x6D, 0xF4, 0xAE, 0x5D, 0x60, 0xBC, 0x8F, 0xF1,
    0xAA, 0xE4, 0xCC, 0xFD, 0x11, 0xA0, 0x5F, 0xB5
  };

  const flea_u8_t* in_ptr__pcu8;
  flea_u8_t* out_ptr__pu8;

  flea_cbc_mode_ctx_t encr_ctx__t;
  flea_cbc_mode_ctx_t decr_ctx__t;

  FLEA_DECL_BUF(decr__bu8, flea_u8_t, sizeof(tdes_cbc_pt));
  FLEA_DECL_BUF(encr__bu8, flea_u8_t, sizeof(tdes_cbc_pt));
  flea_u8_t block_len__u8 = 8; // DES
  FLEA_THR_BEG_FUNC();
  flea_cbc_mode_ctx_t__INIT(&encr_ctx__t);
  flea_cbc_mode_ctx_t__INIT(&decr_ctx__t);

  FLEA_ALLOC_BUF(encr__bu8, sizeof(tdes_cbc_pt));
  FLEA_ALLOC_BUF(decr__bu8, sizeof(tdes_cbc_pt));

  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__ctor(&encr_ctx__t, flea_tdes_3key, tdes_cbc_key, sizeof(tdes_cbc_key), tdes_cbc_iv, sizeof(tdes_cbc_iv), flea_encrypt));
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__ctor(&decr_ctx__t, flea_tdes_3key, tdes_cbc_key, sizeof(tdes_cbc_key), tdes_cbc_iv, sizeof(tdes_cbc_iv), flea_decrypt));

  in_ptr__pcu8 = tdes_cbc_pt;
  out_ptr__pu8 = encr__bu8;
  ;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  in_ptr__pcu8 += block_len__u8;
  out_ptr__pu8 += block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&encr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  if(memcmp(encr__bu8, tdes_cbc_exp_ct, sizeof(tdes_cbc_exp_ct)))
  {
    FLEA_THROW("error with CBC encrypted result (2)", FLEA_ERR_FAILED_TEST);
  }

  in_ptr__pcu8 = encr__bu8;
  out_ptr__pu8 = encr__bu8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&decr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));
  in_ptr__pcu8 += block_len__u8;
  out_ptr__pu8 += block_len__u8;
  FLEA_CCALL(THR_flea_cbc_mode_ctx_t__crypt(&decr_ctx__t, in_ptr__pcu8, out_ptr__pu8, block_len__u8));

  if(memcmp(encr__bu8, tdes_cbc_pt, sizeof(tdes_cbc_pt)))
  {
    FLEA_THROW("error with CBC decrypted result (3)", FLEA_ERR_FAILED_TEST);
  }

  FLEA_CCALL(THR_flea_cbc_mode__decrypt_data(flea_tdes_3key, tdes_cbc_key, sizeof(tdes_cbc_key), tdes_cbc_iv, sizeof(tdes_cbc_iv), tdes_cbc_exp_ct, decr__bu8, sizeof(tdes_cbc_pt)));

  if(memcmp(decr__bu8, tdes_cbc_pt, sizeof(tdes_cbc_pt)))
  {
    FLEA_THROW("error with CBC decrypted result (1)", FLEA_ERR_FAILED_TEST);
  }
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(encr__bu8);
    flea_cbc_mode_ctx_t__dtor(&encr_ctx__t);
    flea_cbc_mode_ctx_t__dtor(&decr_ctx__t);
    FLEA_FREE_BUF_FINAL(decr__bu8);
  );
} /* THR_flea_test_cbc_mode_3des */

#endif /* ifdef FLEA_HAVE_DES */
