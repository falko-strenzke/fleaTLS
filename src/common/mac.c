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
#include "flea/mac.h"
#include "flea/array_util.h"
#include "flea/error_handling.h"
#include "flea/alloc.h"
#include "flea/util.h"
#include "flea/bin_utils.h"
#include "flea/algo_config.h"
#include "internal/common/mac_int2.h"
#include <string.h>
#include "internal/common/block_cipher/block_cipher_int.h"

#ifdef FLEA_HAVE_MAC
struct mac_config_entry_struct
{
  flea_u8_t key_byte_size__u8;

  /**
   * the length of the output of the MAC
   */
  flea_u8_t          mac_output_len__u8;
  flea_mac_id_e      ext_id__t;
  flea_mac_mode_id_t mode_id__t;
  union
  {
    flea_hash_id_e         hash_id__t;
    flea_block_cipher_id_e cipher_id__t;
  } primitive_id__u;
};

static const mac_config_entry_t mac_config__at[] = {
# if defined FLEA_HAVE_MD5 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 64,
    .mac_output_len__u8 = 16,
    .ext_id__t  = flea_hmac_md5,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_md5
  },
# endif /* if defined FLEA_HAVE_MD5 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_SHA1 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 64,
    .mac_output_len__u8 = 20,
    .ext_id__t  = flea_hmac_sha1,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_sha1
  },
# endif /* if defined FLEA_HAVE_SHA1 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_SHA224_256 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 64,
    .mac_output_len__u8 = 224 / 8,
    .ext_id__t  = flea_hmac_sha224,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_sha224
  },
# endif /* if defined FLEA_HAVE_SHA224_256 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_SHA224_256 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 64,
    .mac_output_len__u8 = 256 / 8,
    .ext_id__t  = flea_hmac_sha256,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_sha256
  },
# endif /* if defined FLEA_HAVE_SHA224_256 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_SHA384_512 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 128,
    .mac_output_len__u8 = 384 / 8,
    .ext_id__t  = flea_hmac_sha384,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_sha384
  },
# endif /* if defined FLEA_HAVE_SHA384_512 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_SHA384_512 && defined FLEA_HAVE_HMAC
  {
    .key_byte_size__u8  = 128,
    .mac_output_len__u8 = 512 / 8,
    .ext_id__t  = flea_hmac_sha512,
    .mode_id__t = flea_hmac,
    .primitive_id__u.hash_id__t = flea_sha512
  },
# endif /* if defined FLEA_HAVE_SHA384_512 && defined FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 128 / 8,
    .mac_output_len__u8 = 128 / 8,
    .ext_id__t  = flea_cmac_aes128,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_aes128
  },
# endif /* if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC */
# if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 192 / 8,
    .mac_output_len__u8 = 128 / 8,
    .ext_id__t  = flea_cmac_aes192,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_aes192
  },
# endif /* if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC */
# if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 256 / 8,
    .mac_output_len__u8 = 128 / 8,
    .ext_id__t  = flea_cmac_aes256,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_aes256
  },
# endif /* if defined FLEA_HAVE_AES && defined FLEA_HAVE_CMAC */
# if defined FLEA_HAVE_DES && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 64 / 8,
    .mac_output_len__u8 = 64 / 8,
    .ext_id__t  = flea_cmac_des,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_des_single
  },
# endif /* if defined FLEA_HAVE_DES && defined FLEA_HAVE_CMAC */
# if defined FLEA_HAVE_TDES_2KEY && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 2 * 64 / 8,
    .mac_output_len__u8 = 64 / 8,
    .ext_id__t  = flea_cmac_tdes_2key,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_tdes_2key
  },
# endif /* if defined FLEA_HAVE_TDES_2KEY && defined FLEA_HAVE_CMAC */
# if defined FLEA_HAVE_TDES_3KEY && defined FLEA_HAVE_CMAC
  {
    .key_byte_size__u8  = 3 * 64 / 8,
    .mac_output_len__u8 = 64 / 8,
    .ext_id__t  = flea_cmac_tdes_3key,
    .mode_id__t = flea_cmac,
    .primitive_id__u.cipher_id__t = flea_tdes_3key
  }
# endif /* if defined FLEA_HAVE_TDES_3KEY && defined FLEA_HAVE_CMAC */
};

const mac_config_entry_t* flea_mac__find_mac_config(flea_mac_id_e id__t)
{
  flea_al_u8_t i;

  for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(mac_config__at); i++)
  {
    if(id__t == mac_config__at[i].ext_id__t)
    {
      return &mac_config__at[i];
    }
  }
  return NULL;
}

static flea_err_e THR_flea_mac_ctx_t__ctor_hmac(
  flea_mac_ctx_t*           ctx__pt,
  const mac_config_entry_t* config_entry__pt,
  const flea_u8_t*          key__pcu8,
  flea_al_u16_t             key_len__alu16
)
{
  flea_u8_t* alias_key__pu8;
  flea_u8_t key_byte_len__u8;
  flea_al_u8_t i;

  FLEA_THR_BEG_FUNC();
  flea_hash_ctx_t__INIT(&ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t);
# ifdef FLEA_HEAP_MODE
  ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key__bu8 = NULL;
# endif
  ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key_byte_len__u8 = config_entry__pt->key_byte_size__u8;


  ctx__pt->output_len__u8 = config_entry__pt->mac_output_len__u8; // setting this tells the dtor that all other fields are initialized
# ifdef FLEA_HEAP_MODE
  FLEA_ALLOC_MEM_ARR(
    ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key__bu8,
    ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key_byte_len__u8
  );
# endif /* ifdef FLEA_HEAP_MODE */
  alias_key__pu8      = ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key__bu8;
  ctx__pt->mode_id__t = config_entry__pt->mode_id__t;

  key_byte_len__u8 = ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key_byte_len__u8;
  if(key_len__alu16 > config_entry__pt->key_byte_size__u8)
  {
    flea_u8_t hash_output_len__u8 = flea_hash__get_output_length_by_id(config_entry__pt->primitive_id__u.hash_id__t);
    FLEA_CCALL(
      THR_flea_compute_hash(
        config_entry__pt->primitive_id__u.hash_id__t,
        key__pcu8,
        key_len__alu16,
        alias_key__pu8,
        hash_output_len__u8
      )
    );
    memset(alias_key__pu8 + hash_output_len__u8, 0, key_byte_len__u8 - hash_output_len__u8);
  }
  else// if(key_len__alu16 < key_byte_len__u8)
  {
    memcpy(alias_key__pu8, key__pcu8, key_len__alu16);
    memset(alias_key__pu8 + key_len__alu16, 0, key_byte_len__u8 - key_len__alu16);
  }
  FLEA_CCALL(
    THR_flea_hash_ctx_t__ctor(
      &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t,
      config_entry__pt->primitive_id__u.hash_id__t
    )
  );

  ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_output_len__u8 = flea_hash_ctx_t__get_output_length(
    &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t
    );
  // now build k ^ ipad and process it
  for(i = 0; i < key_byte_len__u8; i++)
  {
    flea_u8_t byte = alias_key__pu8[i] ^ 0x36;
    FLEA_CCALL(THR_flea_hash_ctx_t__update(&ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t, &byte, 1));
  }

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_mac_ctx_t__ctor_hmac */

flea_err_e THR_flea_mac_ctx_t__ctor_cmac(
  flea_mac_ctx_t*           ctx__pt,
  const mac_config_entry_t* config_entry__pt,
  const flea_u8_t*          key__pcu8,
  flea_al_u16_t             key_len__alu16,
  flea_ecb_mode_ctx_t*      ciph_ctx_ref__pt
)
{
  FLEA_THR_BEG_FUNC();
  // lookup config
  // first init the union member:
  // (afterwards, dtor will be able to deal with them; before the ctor call, dtor
  // will do nothin if the key_ptr = NULL)
# ifdef FLEA_HEAP_MODE
  ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8 = NULL;
# endif
  flea_ecb_mode_ctx_t__INIT(&ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t);

  // setting this tells the dtor that all other fields are initialized:
  ctx__pt->output_len__u8 = config_entry__pt->mac_output_len__u8;
  // now allocate the key buffer (indicates to the dtor that init of the remaining
  // members has been performed)
  flea_u8_t block_length__u8 = flea_block_cipher__get_block_size(config_entry__pt->primitive_id__u.cipher_id__t);
# ifdef FLEA_HEAP_MODE
  FLEA_ALLOC_MEM_ARR(ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8, block_length__u8);
# endif
  memset(ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8, 0, block_length__u8);
  ctx__pt->mode_id__t = config_entry__pt->mode_id__t;
  if(ciph_ctx_ref__pt == NULL)
  {
    FLEA_CCALL(
      THR_flea_ecb_mode_ctx_t__ctor(
        &ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t,
        config_entry__pt->primitive_id__u.cipher_id__t,
        key__pcu8,
        key_len__alu16,
        flea_encrypt
      )
    );
  }
  else
  {
    ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t = *ciph_ctx_ref__pt;
  }
  ctx__pt->primitive_specific_ctx__u.cmac_specific__t.pending__u8 = 0;

  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_mac_ctx_t__ctor_cmac */

flea_err_e THR_flea_mac_ctx_t__ctor(
  flea_mac_ctx_t*  ctx__pt,
  flea_mac_id_e    id__t,
  const flea_u8_t* key__pcu8,
  flea_al_u16_t    key_len__alu16
)
{
  const mac_config_entry_t* config_entry__pt;

  FLEA_THR_BEG_FUNC();

  config_entry__pt = flea_mac__find_mac_config(id__t);
  if(config_entry__pt == NULL)
  {
    FLEA_THROW("MAC config not found", FLEA_ERR_INV_ALGORITHM);
  }
  if(config_entry__pt->mode_id__t == flea_hmac)
  {
    FLEA_CCALL(THR_flea_mac_ctx_t__ctor_hmac(ctx__pt, config_entry__pt, key__pcu8, key_len__alu16));
  }
  else
  {
    FLEA_CCALL(THR_flea_mac_ctx_t__ctor_cmac(ctx__pt, config_entry__pt, key__pcu8, key_len__alu16, NULL));
  }


  FLEA_THR_FIN_SEC_empty();
}

void flea_mac_ctx_t__dtor(flea_mac_ctx_t* ctx__pt)
{
  if(ctx__pt->output_len__u8 == 0)
  {
    // indicates nothing has to be done
    return;
  }

  flea_mac_ctx_t__dtor_cipher_ctx_ref(ctx__pt);
  if(ctx__pt->mode_id__t == flea_cmac)
  {
    flea_ecb_mode_ctx_t__dtor(&ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t);
  }
  flea_mac_ctx_t__INIT(ctx__pt);
}

void flea_mac_ctx_t__dtor_cipher_ctx_ref(flea_mac_ctx_t* ctx__pt)
{
  if(ctx__pt->output_len__u8 == 0)
  {
    // indicates nothing has to be done
    return;
  }
  if(ctx__pt->mode_id__t == flea_hmac)
  {
    FLEA_FREE_MEM_CHECK_SET_NULL_SECRET_ARR(
      ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key__bu8,
      ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key_byte_len__u8
    );
    flea_hash_ctx_t__dtor(&ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t);
  }
  else // cmac
  {
    flea_al_u8_t block_length__alu8 =
      ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.config__pt->block_length__u8;
    FLEA_FREE_MEM_CHECK_SET_NULL_SECRET_ARR(
      ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8,
      block_length__alu8
    );
  }
}

flea_err_e THR_flea_mac_ctx_t__update(
  flea_mac_ctx_t*  ctx__pt,
  const flea_u8_t* data__pcu8,
  flea_dtl_t       data_len__dtl
)
{
  FLEA_THR_BEG_FUNC();


  if(ctx__pt->mode_id__t == flea_hmac)
  {
    FLEA_CCALL(
      THR_flea_hash_ctx_t__update(
        &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t,
        data__pcu8,
        data_len__dtl
      )
    );
  }
  else
  {
    // cmac.
    // fill up pending block
    flea_u8_t* block__pu8 = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8;
    flea_al_u8_t block_length__alu8 =
      ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_length__u8;
    flea_al_u8_t left__alu8, to_copy__alu8, tail_len__alu8;
    flea_dtl_t nb_full_blocks__alu16, i;
    flea_al_u8_t pending__alu8 = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.pending__u8;
    left__alu8    = block_length__alu8 - pending__alu8;
    to_copy__alu8 = FLEA_MIN(data_len__dtl, left__alu8);
    flea__xor_bytes_in_place(block__pu8 + pending__alu8, data__pcu8, to_copy__alu8);
    data__pcu8    += to_copy__alu8;
    data_len__dtl -= to_copy__alu8;
    pending__alu8 += to_copy__alu8;

    nb_full_blocks__alu16 = data_len__dtl / block_length__alu8;
    tail_len__alu8        = data_len__dtl % block_length__alu8;

    if((pending__alu8 == block_length__alu8) && (nb_full_blocks__alu16 || tail_len__alu8))
    {
      ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_crypt_f(
        &ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t,
        block__pu8,
        block__pu8
      );
      pending__alu8 = 0;
    }
    // we have to keep the last block for potential finalize
    if(!tail_len__alu8 && nb_full_blocks__alu16 >= 1)
    {
      nb_full_blocks__alu16 -= 1;
      tail_len__alu8         = block_length__alu8;
    }
    for(i = 0; i < nb_full_blocks__alu16; i++)
    {
      flea__xor_bytes_in_place(block__pu8, data__pcu8, block_length__alu8);
      ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_crypt_f(
        &ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t,
        block__pu8,
        block__pu8
      );
      data__pcu8 += block_length__alu8;
    }
    if(tail_len__alu8 != 0)
    {
      flea__xor_bytes_in_place(block__pu8, data__pcu8, tail_len__alu8);
      pending__alu8 = tail_len__alu8;
    }
    ctx__pt->primitive_specific_ctx__u.cmac_specific__t.pending__u8 = pending__alu8;
  }
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_mac_ctx_t__update */

// internal function
void flea_mac_ctx_t__reset_cmac(flea_mac_ctx_t* ctx__pt)
{
  flea_u8_t block_length__u8 = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_length__u8;

  ctx__pt->primitive_specific_ctx__u.cmac_specific__t.pending__u8 = 0;
  memset(ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8, 0, block_length__u8);
}

# if defined FLEA_HAVE_CMAC
static void flea_mac__polyn_double(
  flea_u8_t*   block__pu8,
  flea_al_u8_t block_length__alu8
)
{
  flea_al_u8_t i;
  flea_al_u8_t carry__alu8 = 0;
  flea_u8_t poly__u8;

  if(block_length__alu8 == 16)
  {
    poly__u8 = 0x87;
  }
  else
  {
    // block size == 8
    poly__u8 = 0x1B;
  }
  if((block__pu8[0] & 0x80) == 0)
  {
    poly__u8 = 0;
  }
  for(i = block_length__alu8; i > 0; i--)
  {
    flea_u8_t byte = block__pu8[i - 1];
    block__pu8[i - 1] = (byte << 1) | carry__alu8;
    carry__alu8       = byte >> 7;
  }
  block__pu8[block_length__alu8 - 1] ^= poly__u8;
}

# endif // #if defined FLEA_HAVE_CMAC

# ifdef FLEA_HAVE_HMAC
static flea_err_e THR_flea_mac_ctx_t__final_hmac_compute(
  flea_mac_ctx_t* ctx__pt,
  flea_u8_t*      result__pu8,
  flea_al_u8_t*   result_len__palu8
)
{
  FLEA_DECL_BUF(hash_out__bu8, flea_u8_t, FLEA_MAX_HASH_OUT_LEN);
  flea_al_u8_t i;
  flea_u8_t* tmp_alias__pu8     = result__pu8;
  flea_u8_t key_byte_len__u8    = ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key_byte_len__u8;
  flea_u8_t* alias_key__pu8     = ctx__pt->primitive_specific_ctx__u.hmac_specific__t.key__bu8;
  flea_u8_t hash_output_len__u8 = ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_output_len__u8;
  flea_al_u8_t demanded_output_len__alu8 = *result_len__palu8;
  FLEA_THR_BEG_FUNC();
  if(*result_len__palu8 < ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_output_len__u8)
  {
    FLEA_ALLOC_BUF(hash_out__bu8, ctx__pt->output_len__u8);
    tmp_alias__pu8 = hash_out__bu8;
  }
  FLEA_CCALL(
    THR_flea_hash_ctx_t__final(
      &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t,
      tmp_alias__pu8
    )
  );
  // reuse the hash-ctx for the outer hash application:
  flea_hash_ctx_t__reset(&ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t);
  for(i = 0; i < key_byte_len__u8; i++)
  {
    flea_u8_t byte = alias_key__pu8[i] ^ 0x5c;
    FLEA_CCALL(THR_flea_hash_ctx_t__update(&ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t, &byte, 1));
  }
  FLEA_CCALL(
    THR_flea_hash_ctx_t__update(
      &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.hash_ctx__t,
      tmp_alias__pu8,
      hash_output_len__u8
    )
  );
  if(demanded_output_len__alu8 > ctx__pt->output_len__u8)
  {
    demanded_output_len__alu8 = ctx__pt->output_len__u8;
  }
  FLEA_CCALL(
    THR_flea_hash_ctx_t__final_with_length_limit(
      &ctx__pt->primitive_specific_ctx__u.hmac_specific__t.
      hash_ctx__t,
      result__pu8,
      demanded_output_len__alu8
    )
  );
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(hash_out__bu8);
  );
} /* THR_flea_mac_ctx_t__final_hmac_compute */

# endif // #ifdef FLEA_HAVE_HMAC
# ifdef FLEA_HAVE_CMAC
static flea_err_e THR_flea_mac_ctx_t__final_cmac_compute(
  flea_mac_ctx_t* ctx__pt,
  flea_u8_t*      result__pu8,
  flea_al_u8_t*   result_len__palu8
)
{
  FLEA_DECL_BUF(block__bu8, flea_u8_t, FLEA_BLOCK_CIPHER_MAX_BLOCK_LENGTH);
  FLEA_THR_BEG_FUNC();
  flea_al_u8_t demanded_output_len__alu8;
  flea_u8_t* pending_block__pu8   = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.prev_ct__bu8;
  flea_al_u8_t block_length__alu8 = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_length__u8;
  flea_al_u8_t pending__alu8      = ctx__pt->primitive_specific_ctx__u.cmac_specific__t.pending__u8;
  // determine whether the last block is a complete block:
  FLEA_ALLOC_BUF(block__bu8, block_length__alu8);
  memset(block__bu8, 0, block_length__alu8);

  ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_crypt_f(
    &ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t,
    block__bu8,
    block__bu8
  );
  // K1 ^ M*_n
  // compute K1
  flea_mac__polyn_double(block__bu8, block_length__alu8);
  // block__bu8 now contains K1
  if(pending__alu8 == block_length__alu8)
  {
    // make use of K1
    flea__xor_bytes_in_place(block__bu8, pending_block__pu8, block_length__alu8);
    // block__bu8 now contains K1 ^ M*_n
  }
  else
  {
    // incomplete final block, use K2
    flea_mac__polyn_double(block__bu8, block_length__alu8); // block__bu8 now contains K2
    // pad the pending block:
    pending_block__pu8[pending__alu8] ^= 0x80;
    flea__xor_bytes_in_place(block__bu8, pending_block__pu8, block_length__alu8);
  }
  ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t.block_crypt_f(
    &ctx__pt->primitive_specific_ctx__u.cmac_specific__t.cipher_ctx__t,
    block__bu8,
    block__bu8
  );
  if(*result_len__palu8 < ctx__pt->output_len__u8)
  {
    // FLEA_THROW("MAC result buffer too small", FLEA_ERR_BUFF_TOO_SMALL);
    demanded_output_len__alu8 = *result_len__palu8;
  }
  else
  {
    demanded_output_len__alu8 = ctx__pt->output_len__u8;
  }
  memcpy(result__pu8, block__bu8, demanded_output_len__alu8);
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(block__bu8);
  );
} /* THR_flea_mac_ctx_t__final_cmac_compute */

# endif // #ifdef FLEA_HAVE_CMAC
flea_err_e THR_flea_mac_ctx_t__final_compute(
  flea_mac_ctx_t* ctx__pt,
  flea_u8_t*      result__pu8,
  flea_al_u8_t*   result_len__palu8
)
{
  FLEA_THR_BEG_FUNC();
# ifdef FLEA_HAVE_HMAC
  if(ctx__pt->mode_id__t == flea_hmac)
  {
    FLEA_CCALL(THR_flea_mac_ctx_t__final_hmac_compute(ctx__pt, result__pu8, result_len__palu8));
  }
# endif /* ifdef FLEA_HAVE_HMAC */
# if defined FLEA_HAVE_CMAC && defined FLEA_HAVE_HMAC
  else // cmac
# endif
# if defined FLEA_HAVE_CMAC
  {
    FLEA_CCALL(THR_flea_mac_ctx_t__final_cmac_compute(ctx__pt, result__pu8, result_len__palu8));
  }
# endif
  if(*result_len__palu8 > ctx__pt->output_len__u8)
  {
    *result_len__palu8 = ctx__pt->output_len__u8;
  }
  FLEA_THR_FIN_SEC_empty();
}

flea_al_u8_t flea_mac__get_output_length_by_id(flea_mac_id_e mac_id__e)
{
  const mac_config_entry_t* config_entry__pt = flea_mac__find_mac_config(mac_id__e);

  if(config_entry__pt == NULL)
  {
    return 0;
  }
  return config_entry__pt->mac_output_len__u8;
}

/**
 * Timing neutral function for memory comparison.
 */

flea_err_e THR_flea_mac_ctx_t__final_verify(
  flea_mac_ctx_t*  ctx__pt,
  const flea_u8_t* exp_result__pcu8,
  flea_al_u8_t     exp_result_len__alu8
)
{
  flea_al_u8_t mac_res_len__alu8 = exp_result_len__alu8;

  FLEA_DECL_BUF(mac_out__bu8, flea_u8_t, FLEA_MAC_MAX_OUTPUT_LENGTH);
  FLEA_THR_BEG_FUNC();
  if(exp_result_len__alu8 != ctx__pt->output_len__u8)
  {
    FLEA_THROW("MAC length wrong", FLEA_ERR_INV_MAC);
  }
  FLEA_ALLOC_BUF(mac_out__bu8, exp_result_len__alu8);
  FLEA_CCALL(THR_flea_mac_ctx_t__final_compute(ctx__pt, mac_out__bu8, &mac_res_len__alu8));
  // secure comparison
  if(!flea_sec_mem_equal(mac_out__bu8, exp_result__pcu8, exp_result_len__alu8))
  {
    FLEA_THROW("MAC verification failed", FLEA_ERR_INV_MAC);
  }
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(mac_out__bu8);
  );
}

flea_err_e THR_flea_mac__compute_mac(
  flea_mac_id_e    id__t,
  const flea_u8_t* key__pcu8,
  flea_al_u16_t    key_len__alu16,
  const flea_u8_t* data__pcu8,
  flea_dtl_t       data_len__dtl,
  flea_u8_t*       result__pu8,
  flea_al_u8_t*    result_len__palu8
)
{
  flea_mac_ctx_t ctx__t;

  FLEA_THR_BEG_FUNC();
  flea_mac_ctx_t__INIT(&ctx__t);
  FLEA_CCALL(THR_flea_mac_ctx_t__ctor(&ctx__t, id__t, key__pcu8, key_len__alu16));
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&ctx__t, data__pcu8, data_len__dtl));
  FLEA_CCALL(THR_flea_mac_ctx_t__final_compute(&ctx__t, result__pu8, result_len__palu8));
  FLEA_THR_FIN_SEC(
    flea_mac_ctx_t__dtor(&ctx__t);
  );
}

flea_err_e THR_flea_mac__verify_mac(
  flea_mac_id_e    id__t,
  const flea_u8_t* key__pcu8,
  flea_al_u16_t    key_len__alu16,
  const flea_u8_t* data__pcu8,
  flea_dtl_t       data_len__dtl,
  const flea_u8_t* exp_mac__pu8,
  flea_al_u8_t     exp_mac_len__alu8
)
{
  flea_mac_ctx_t ctx__t;

  FLEA_THR_BEG_FUNC();
  flea_mac_ctx_t__INIT(&ctx__t);
  FLEA_CCALL(THR_flea_mac_ctx_t__ctor(&ctx__t, id__t, key__pcu8, key_len__alu16));
  FLEA_CCALL(THR_flea_mac_ctx_t__update(&ctx__t, data__pcu8, data_len__dtl));
  FLEA_CCALL(THR_flea_mac_ctx_t__final_verify(&ctx__t, exp_mac__pu8, exp_mac_len__alu8));
  FLEA_THR_FIN_SEC(
    flea_mac_ctx_t__dtor(&ctx__t);
  );
}

#endif // #ifdef FLEA_HAVE_MAC
