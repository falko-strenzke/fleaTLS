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
#include "flea/rsa.h"
#include "flea/alloc.h"
#include "self_test.h"
#include "internal/common/block_cipher/aes.h"
#include <string.h>


static flea_err_e THR_flea_test_block_cipher_init_dtor()
{
  flea_ecb_mode_ctx_t ecb;

  flea_cbc_mode_ctx_t cbc;
  flea_ctr_mode_ctx_t ctr;

  FLEA_THR_BEG_FUNC();
  flea_ecb_mode_ctx_t__INIT(&ecb);
  flea_cbc_mode_ctx_t__INIT(&cbc);
  flea_ctr_mode_ctx_t__INIT(&ctr);

  FLEA_THR_FIN_SEC(
    flea_ecb_mode_ctx_t__dtor(&ecb);
    flea_cbc_mode_ctx_t__dtor(&cbc);
    flea_ctr_mode_ctx_t__dtor(&ctr);
  );
}

static flea_err_e THR_flea_test_cipher_block_encr_decr_data(
  flea_block_cipher_id_e id__t,
  const flea_u8_t*       key__pc_u8,
  flea_al_u16_t          key_len__al_u16,
  const flea_u8_t*       exp_ct__pc_u8,
  const flea_u8_t*       pt__pc_u8
)
{
  flea_ecb_mode_ctx_t encr_ctx;

  flea_ecb_mode_ctx_t__INIT(&encr_ctx);
#ifdef FLEA_HAVE_AES_BLOCK_DECR
  flea_ecb_mode_ctx_t decr_ctx;
  flea_ecb_mode_ctx_t__INIT(&decr_ctx);
#endif

  const flea_al_u8_t const_block_size = 16;
  flea_u8_t encr[const_block_size];

#ifdef FLEA_HAVE_AES_BLOCK_DECR
  flea_u8_t decr[const_block_size];
#endif

  flea_al_u8_t block_size;
  FLEA_THR_BEG_FUNC();

#ifdef FLEA_HAVE_AES_BLOCK_DECR
  flea_ecb_mode_ctx_t__INIT(&decr_ctx);
#endif
  flea_ecb_mode_ctx_t__INIT(&encr_ctx);

  FLEA_CCALL(THR_flea_ecb_mode_ctx_t__ctor(&encr_ctx, id__t, key__pc_u8, key_len__al_u16, flea_encrypt));
#ifdef FLEA_HAVE_AES_BLOCK_DECR
  FLEA_CCALL(THR_flea_ecb_mode_ctx_t__ctor(&decr_ctx, id__t, key__pc_u8, key_len__al_u16, flea_decrypt));
#endif // #ifndef FLEA_HAVE_AES_BLOCK_DECR
  block_size = encr_ctx.block_length__u8;
  FLEA_CCALL(THR_flea_ecb_ctx_t__crypt_data(&encr_ctx, pt__pc_u8, encr, block_size));
  if(memcmp(encr, exp_ct__pc_u8, block_size))
  {
    FLEA_THROW("encrypted block incorrect", FLEA_ERR_FAILED_TEST);
  }

#ifdef FLEA_HAVE_AES_BLOCK_DECR
  FLEA_CCALL(THR_flea_ecb_ctx_t__crypt_data(&decr_ctx, encr, decr, block_size));
  if(memcmp(decr, pt__pc_u8, block_size))
  {
    FLEA_THROW("decrypted block incorrect", FLEA_ERR_FAILED_TEST);
  }
#endif // #ifdef FLEA_HAVE_AES_BLOCK_DECR
  FLEA_THR_FIN_SEC(
    flea_ecb_mode_ctx_t__dtor(&encr_ctx);
    FLEA_DO_IF_HAVE_AES_BLOCK_DECR(
      flea_ecb_mode_ctx_t__dtor(&decr_ctx);
    );
  );
} /* THR_flea_test_cipher_block_encr_decr_data */

flea_err_e THR_flea_test_cipher_block_encr_decr()
{
  flea_u8_t pt_aes128[]     = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  flea_u8_t exp_ct_aes128[] = {0x69, 0xC4, 0xE0, 0xD8, 0x6A, 0x7B, 0x04, 0x30, 0xD8, 0xCD, 0xB7, 0x80, 0x70, 0xB4, 0xC5, 0x5A};
  flea_u8_t key_aes128[]    = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

  flea_u8_t key_aes192[] = {
    0x04, 0x05, 0x06, 0x07, 0x09, 0x0A, 0x0B, 0x0C, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x1E, 0x1F, 0x20
  };
  flea_u8_t pt_aes192 [] = {
    0x76, 0x77, 0x74, 0x75, 0xF1, 0xF2, 0xF3, 0xF4, 0xF8, 0xF9, 0xE6, 0xE7, 0x77, 0x70, 0x71, 0x72
  };
  flea_u8_t exp_ct_aes192[] = {
    0x5d, 0x1e, 0xf2, 0x0d, 0xce, 0xd6, 0xbc, 0xbc, 0x12, 0x13, 0x1a, 0xc7, 0xc5, 0x47, 0x88, 0xaa
  };

  flea_u8_t key_aes256[] = {
    0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x10, 0x12, 0x13, 0x14, 0x15, 0x17, 0x18, 0x19, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x21, 0x22, 0x23, 0x24, 0x26, 0x27, 0x28, 0x29, 0x2B, 0x2C, 0x2D, 0x2E
  };
  flea_u8_t pt_aes256[] = {
    0x06, 0x9A, 0x00, 0x7F, 0xC7, 0x6A, 0x45, 0x9F, 0x98, 0xBA, 0xF9, 0x17, 0xFE, 0xDF, 0x95, 0x21
  };
  flea_u8_t exp_ct_aes256[] = {
    0x08, 0x0e, 0x95, 0x17, 0xeb, 0x16, 0x77, 0x71, 0x9a, 0xcf, 0x72, 0x80, 0x86, 0x04, 0x0a, 0xe3
  };

#ifdef FLEA_HAVE_DES
  flea_u8_t key_des[8] = {
    0x04, 0x9A, 0x5F, 0x09, 0x50, 0xCE, 0x15, 0x3B
  };
  const flea_u8_t pt_des[8] = {
    0x86, 0xA5, 0x60, 0xF1, 0x0E, 0xC6, 0xD8, 0x5B
  };

  const flea_u8_t exp_ct_des[8] = {
    0x8e, 0x0a, 0x6d, 0xcf, 0xfe, 0x71, 0x9c, 0x40
  };

  const flea_u8_t key_tdes_2key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

  const flea_u8_t pt_tdes_2key[8]     = {0xE4, 0xFC, 0x19, 0xD6, 0x94, 0x63, 0xB7, 0x83};
  const flea_u8_t exp_ct_tdes_2key[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

  const flea_u8_t key_tdes_3key[24] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
  };

  const flea_u8_t pt_tdes_3key[8]     = {0x73, 0x6F, 0x6D, 0x65, 0x64, 0x61, 0x74, 0x61};
  const flea_u8_t exp_ct_tdes_3key[8] = {0x18, 0xD7, 0x48, 0xE5, 0x63, 0x62, 0x05, 0x72};

  const flea_u8_t key_desx[24] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
  };
  const flea_u8_t pt_desx[8]     = {0x94, 0xDB, 0xE0, 0x82, 0x54, 0x9A, 0x14, 0xEF};
  const flea_u8_t exp_ct_desx[8] = {0x90, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

#endif // #ifdef FLEA_HAVE_DES

  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_aes128, key_aes128, sizeof(key_aes128), exp_ct_aes128, pt_aes128));
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_aes192, key_aes192, sizeof(key_aes192), exp_ct_aes192, pt_aes192));
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_aes256, key_aes256, sizeof(key_aes256), exp_ct_aes256, pt_aes256));

#ifdef FLEA_HAVE_DES
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_des_single, key_des, sizeof(key_des), exp_ct_des, pt_des));
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_tdes_2key, key_tdes_2key, sizeof(key_tdes_2key), exp_ct_tdes_2key, pt_tdes_2key));
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_tdes_3key, key_tdes_3key, sizeof(key_tdes_3key), exp_ct_tdes_3key, pt_tdes_3key));
  FLEA_CCALL(THR_flea_test_cipher_block_encr_decr_data(flea_desx, key_desx, sizeof(key_desx), exp_ct_desx, pt_desx));
#endif // #ifdef FLEA_HAVE_DES

  FLEA_CCALL(THR_flea_test_block_cipher_init_dtor());

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_test_cipher_block_encr_decr */
