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
#include "flea/bin_utils.h"
#include "flea/types.h"

void flea__xor_bytes_in_place(
  flea_u8_t*       in_out,
  const flea_u8_t* in2,
  flea_dtl_t       len
)
{
  flea_dtl_t i;

  for(i = 0; i < len; i++)
  {
    in_out[i] ^= in2[i];
  }
}

void flea__xor_bytes(
  flea_u8_t*       out,
  const flea_u8_t* in1,
  const flea_u8_t* in2,
  flea_dtl_t       len__dtl
)
{
  flea_dtl_t i;

  for(i = 0; i < len__dtl; i++)
  {
    out[i] = in1[i] ^ in2[i];
  }
}

void flea__encode_U32_LE(
  flea_u32_t to_enc,
  flea_u8_t  res[4]
)
{
  res[3] = to_enc >> 24;
  res[2] = to_enc >> 16;
  res[1] = to_enc >> 8;
  res[0] = to_enc & 0xFF;
}

void flea__encode_U32_BE(
  flea_u32_t to_enc,
  flea_u8_t  res[4]
)
{
  res[0] = to_enc >> 24;
  res[1] = to_enc >> 16;
  res[2] = to_enc >> 8;
  res[3] = to_enc & 0xFF;
}

void flea__encode_U16_BE(
  flea_u16_t to_enc,
  flea_u8_t  res[2]
)
{
  res[0] = to_enc >> 8;
  res[1] = to_enc & 0xFF;
}

flea_u32_t flea__decode_U32_BE(const flea_u8_t enc[4])
{
  return ((flea_u32_t) enc[0] << 24)
         | ((flea_u32_t) enc[1] << 16)
         | ((flea_u32_t) enc[2] << 8)
         | ((flea_u32_t) (enc[3] & 0xFF));
}

flea_u16_t flea__decode_U16_BE(const flea_u8_t enc[2])
{
  return ((flea_u16_t) enc[0] << 8) | ((flea_u16_t) enc[1] & 0xFF);
}

flea_u16_t flea__decode_U16_LE(const flea_u8_t enc[2])
{
  return ((flea_u16_t) enc[1] << 8)
         | ((flea_u16_t) (enc[0] & 0xFF));
}

void flea__increment_encoded_BE_int(
  flea_u8_t*   ctr_block_pu8,
  flea_al_u8_t ctr_block_length_al_u8
)
{
  flea_al_s8_t i;

  for(i = ctr_block_length_al_u8 - 1; i >= 0; i--)
  {
    flea_u8_t old_u8 = ctr_block_pu8[i];
    ctr_block_pu8[i] += 1;
    if(ctr_block_pu8[i] > old_u8)
    {
      // no overflow
      break;
    }
  }
}

flea_mpi_ulen_t flea__get_BE_int_bit_len(
  const flea_u8_t* enc__pcu8,
  flea_mpi_ulen_t  int_len__mpl
)
{
  flea_mpi_ulen_t i;

  for(i = 0; i < int_len__mpl; i++)
  {
    if(enc__pcu8[i])
    {
      flea_al_s8_t j;
      for(j = 7; j >= 0; j--)
      {
        if(enc__pcu8[i] & (1 << j))
        {
          return j + 1 + (int_len__mpl - 1 - i) * 8;
        }
      }
    }
  }
  return 0;
}
