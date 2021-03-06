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
#include <stdlib.h>
#include <string.h>
#include "internal/common/math/mpi.h"
#include "flea/error.h"
#include "flea/alloc.h"
#include "flea/array_util.h"
#include "flea/util.h"
#include "flea/rng.h"
#include "flea/ec_dom_par.h"
#include "internal/common/math/curve_gfp.h"
#include "internal/common/math/point_gfp.h"
#include "flea/ecdsa.h"
#include "flea/hash.h"
#include "flea/ctr_mode_prng.h"
#include "flea/algo_config.h"
#include "internal/common/ecc_int.h"

#ifdef FLEA_HAVE_ECDSA
flea_err_e THR_flea_ecdsa__raw_verify(
  const flea_u8_t*             enc_r,
  flea_al_u8_t                 enc_r_len,
  const flea_u8_t*             enc_s,
  flea_al_u8_t                 enc_s_len,
  const flea_u8_t*             message,
  flea_al_u8_t                 message_len,
  const flea_u8_t*             pub_point_enc,
  flea_al_u8_t                 pub_point_enc_len,
  const flea_ec_dom_par_ref_t* dom_par__pt
)
{
  flea_mpi_t n, s, s_inv, double_sized_field_elem;

  # define ECDSA_VERIFY_MPI_WS_COUNT 4

  flea_mpi_t mpi_worksp_arr[ECDSA_VERIFY_MPI_WS_COUNT];
  flea_curve_gfp_t curve;
  flea_point_gfp_t G, P;

# ifdef FLEA_STACK_MODE
  flea_uword_t ecc_ws_mpi_arrs [ECDSA_VERIFY_MPI_WS_COUNT][FLEA_ECC_MAX_ORDER_WORD_SIZE];
# else
  flea_al_u8_t enc_order_len;
  flea_al_u8_t enc_field_len;
  flea_uword_t* ecc_ws_mpi_arrs [ECDSA_VERIFY_MPI_WS_COUNT];
# endif /* ifdef FLEA_STACK_MODE */
  FLEA_DECL_BUF(vn, flea_hlf_uword_t, FLEA_MPI_DIV_VN_HLFW_LEN_FROM_DIVISOR_W_LEN(FLEA_ECC_MAX_ORDER_WORD_SIZE));
  FLEA_DECL_BUF(
    un,
    flea_hlf_uword_t,
    FLEA_MPI_DIV_UN_HLFW_LEN_FROM_DIVIDENT_W_LEN(2 * (FLEA_ECC_MAX_MOD_WORD_SIZE + 1))
  );
  FLEA_DECL_BUF(s_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(s_inv_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(n_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(double_sized_field_elem_arr, flea_uword_t, 2 * FLEA_ECC_MAX_MOD_WORD_SIZE + 1);  /* +1 one since n can be one bit larger than n */
  FLEA_DECL_BUF(G_arr, flea_uword_t, 2 * FLEA_ECC_MAX_MOD_WORD_SIZE);
  FLEA_DECL_BUF(P_arr, flea_uword_t, 2 * FLEA_ECC_MAX_MOD_WORD_SIZE);
  FLEA_DECL_BUF(curve_word_arr, flea_uword_t, 3 * FLEA_ECC_MAX_MOD_WORD_SIZE);


  flea_mpi_div_ctx_t div_ctx;
  flea_al_u8_t i;
# ifdef FLEA_HEAP_MODE
  flea_mpi_ulen_t vn_len, un_len, prime_word_len, order_word_len, double_sized_field_elem_arr_len, G_arr_word_len;
  flea_al_u16_t curve_word_arr_word_len, ecc_ws_mpi_arrs_word_len;
# endif
  FLEA_THR_BEG_FUNC();

  memset(ecc_ws_mpi_arrs, 0, sizeof(ecc_ws_mpi_arrs));
# ifdef FLEA_HEAP_MODE
  enc_field_len  = dom_par__pt->p__ru8.len__dtl;
  enc_order_len  = dom_par__pt->n__ru8.len__dtl;
  prime_word_len = FLEA_CEIL_WORD_LEN_FROM_BYTE_LEN(enc_field_len);
  order_word_len = FLEA_CEIL_WORD_LEN_FROM_BYTE_LEN(enc_order_len) + 4 / sizeof(flea_uword_t);

  G_arr_word_len = 2 * prime_word_len;
  curve_word_arr_word_len         = 3 * prime_word_len;
  double_sized_field_elem_arr_len = 2 * prime_word_len + 1; /* +1 one since n can be one bit larger than n */
  ecc_ws_mpi_arrs_word_len        = order_word_len;
  vn_len = FLEA_MPI_DIV_VN_HLFW_LEN_FROM_DIVISOR_W_LEN(order_word_len);
  un_len = FLEA_MPI_DIV_UN_HLFW_LEN_FROM_DIVIDENT_W_LEN(2 * (prime_word_len + 1)); // + 1 due to reducing R^2
# endif /* ifdef FLEA_HEAP_MODE */

# ifdef FLEA_HEAP_MODE
  for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
  {
    FLEA_ALLOC_MEM_ARR(ecc_ws_mpi_arrs[i], ecc_ws_mpi_arrs_word_len);
  }
# endif // #ifdef FLEA_HEAP_MODE

  FLEA_ALLOC_BUF(s_inv_arr, order_word_len);
  FLEA_ALLOC_BUF(s_arr, order_word_len);
  FLEA_ALLOC_BUF(n_arr, order_word_len);
  FLEA_ALLOC_BUF(G_arr, G_arr_word_len);
  FLEA_ALLOC_BUF(P_arr, G_arr_word_len);
  FLEA_ALLOC_BUF(curve_word_arr, curve_word_arr_word_len);

  FLEA_ALLOC_BUF(double_sized_field_elem_arr, double_sized_field_elem_arr_len);


  FLEA_CCALL(
    THR_flea_curve_gfp_t__init_dp_array(
      &curve,
      dom_par__pt,
      curve_word_arr,
      FLEA_HEAP_OR_STACK_CODE(curve_word_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(curve_word_arr))
    )
  );

  FLEA_CCALL(
    THR_flea_point_gfp_t__init(
      &G,
      dom_par__pt->gx__ru8.data__pcu8,
      dom_par__pt->gx__ru8.len__dtl,
      dom_par__pt->gy__ru8.data__pcu8,
      dom_par__pt->gy__ru8.len__dtl,
      G_arr,
      FLEA_HEAP_OR_STACK_CODE(G_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(G_arr))
    )
  );
  FLEA_CCALL(
    THR_flea_point_gfp_t__init_decode(
      &P,
      pub_point_enc,
      pub_point_enc_len,
      P_arr,
      FLEA_HEAP_OR_STACK_CODE(G_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(P_arr))
    )
  );
  flea_mpi_t__init(&s_inv, s_inv_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(s_inv_arr)));
  flea_mpi_t__init(&s, s_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(s_arr)));
  flea_mpi_t__init(&n, n_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(n_arr)));
  flea_mpi_t__init(
    &double_sized_field_elem,
    double_sized_field_elem_arr,
    FLEA_HEAP_OR_STACK_CODE(double_sized_field_elem_arr_len, FLEA_STACK_BUF_NB_ENTRIES(double_sized_field_elem_arr))
  );

  FLEA_CCALL(THR_flea_mpi_t__decode(&n, dom_par__pt->n__ru8.data__pcu8, dom_par__pt->n__ru8.len__dtl));
  FLEA_CCALL(THR_flea_mpi_t__decode(&s, enc_s, enc_s_len));

  for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
  {
    flea_mpi_t__init(
      &mpi_worksp_arr[i],
      ecc_ws_mpi_arrs[i],
      FLEA_HEAP_OR_STACK_CODE(ecc_ws_mpi_arrs_word_len, FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs[i]))
    );
  }
  // k_inv = k^(-1)
  FLEA_CCALL(THR_flea_mpi_t__invert_odd_mod(&s_inv, &s, &n, mpi_worksp_arr));
  FLEA_FREE_BUF(s_arr);
# ifdef FLEA_HEAP_MODE
  FLEA_FREE_MEM_SET_NULL(ecc_ws_mpi_arrs[2]);
  FLEA_FREE_MEM_SET_NULL(ecc_ws_mpi_arrs[3]);
# endif

  FLEA_CCALL(THR_flea_mpi_t__decode(&mpi_worksp_arr[0], message, message_len));

  FLEA_CCALL(THR_flea_mpi_t__mul(&double_sized_field_elem, &s_inv, &mpi_worksp_arr[0]));

  // allocated after point mul, to reduce peak allocation size
  FLEA_ALLOC_BUF(vn, vn_len);
  FLEA_ALLOC_BUF(un, un_len);
  div_ctx.vn     = vn;
  div_ctx.un     = un;
  div_ctx.vn_len = FLEA_HEAP_OR_STACK_CODE(vn_len, FLEA_STACK_BUF_NB_ENTRIES(vn));
  ;
  div_ctx.un_len = FLEA_HEAP_OR_STACK_CODE(un_len, FLEA_STACK_BUF_NB_ENTRIES(un));

  FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &mpi_worksp_arr[0], &double_sized_field_elem, &n, &div_ctx)); // reduced u1

  FLEA_CCALL(THR_flea_mpi_t__decode(&mpi_worksp_arr[1], enc_r, enc_r_len));

  FLEA_CCALL(THR_flea_mpi_t__mul(&double_sized_field_elem, &s_inv, &mpi_worksp_arr[1]));
  FLEA_FREE_BUF(s_inv_arr);

  FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &mpi_worksp_arr[1], &double_sized_field_elem, &n, &div_ctx)); // reduced u2

  // Q = 0 is detected by this function
# ifdef FLEA_SCCM_USE_PUBKEY_INPUT_BASED_DELAY
  FLEA_CCALL(THR_flea_point_gfp_t__mul_multi(&P, &mpi_worksp_arr[1], &G, &mpi_worksp_arr[0], &curve, FLEA_FALSE, NULL));
# else

  FLEA_CCALL(THR_flea_point_gfp_t__mul_multi(&P, &mpi_worksp_arr[1], &G, &mpi_worksp_arr[0], &curve, FLEA_FALSE));
# endif


  FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &mpi_worksp_arr[1], &P.m_x, &n, &div_ctx));

  FLEA_CCALL(THR_flea_mpi_t__decode(&mpi_worksp_arr[0], enc_r, enc_r_len));
  if(flea_mpi_t__compare(&mpi_worksp_arr[1], &mpi_worksp_arr[0]))
  {
    FLEA_THROW("ecdsa signature not verified correctly", FLEA_ERR_INV_SIGNATURE);
  }


  FLEA_THR_FIN_SEC(
    FLEA_DO_IF_USE_HEAP_BUF(
      for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
  {
    FLEA_FREE_MEM_CHK_NULL(ecc_ws_mpi_arrs[i]);
  }
    );
    FLEA_FREE_BUF_FINAL(s_arr);
    FLEA_FREE_BUF_FINAL(s_inv_arr);
    FLEA_FREE_BUF_FINAL(double_sized_field_elem_arr);
    FLEA_FREE_BUF_FINAL(curve_word_arr);
    FLEA_FREE_BUF_FINAL(n_arr);
    FLEA_FREE_BUF_FINAL(G_arr);
    FLEA_FREE_BUF_FINAL(P_arr);
    FLEA_FREE_BUF_FINAL(vn);
    FLEA_FREE_BUF_FINAL(un);
  );
} /* THR_flea_ecdsa__raw_verify */

flea_err_e THR_flea_ecdsa__raw_sign(
  flea_u8_t*                   res_r_arr,
  flea_al_u8_t*                res_r_arr_len,
  flea_u8_t*                   res_s_arr,
  flea_al_u8_t*                res_s_arr_len,
  const flea_u8_t*             message,
  flea_al_u8_t                 message_len,
  const flea_u8_t*             priv_key_enc_arr,
  flea_al_u8_t                 priv_key_enc_arr_len,
  const flea_ec_dom_par_ref_t* dom_par__pt
)
{
  flea_mpi_t n, k, k_inv, r, s;

  # define ECDSA_SIGN_MPI_WS_COUNT 4

  flea_mpi_t mpi_worksp_arr[ECDSA_SIGN_MPI_WS_COUNT];
  flea_curve_gfp_t curve;
  flea_point_gfp_t G;

# ifdef FLEA_SCCM_USE_ECC_ADD_ALWAYS
  const flea_bool_t do_use_add_always__b = FLEA_TRUE;
# else
  const flea_bool_t do_use_add_always__b = FLEA_FALSE;
# endif
# ifdef FLEA_STACK_MODE
  flea_uword_t ecc_ws_mpi_arrs [ECDSA_SIGN_MPI_WS_COUNT][FLEA_ECC_MAX_ORDER_WORD_SIZE];
# else
  flea_uword_t* ecc_ws_mpi_arrs [ECDSA_SIGN_MPI_WS_COUNT];
  flea_al_u8_t enc_field_len, enc_order_len;
# endif
  FLEA_DECL_BUF(vn, flea_hlf_uword_t, FLEA_MPI_DIV_VN_HLFW_LEN_FROM_DIVISOR_W_LEN(FLEA_ECC_MAX_ORDER_WORD_SIZE));
  FLEA_DECL_BUF(
    un,
    flea_hlf_uword_t,
    FLEA_MPI_DIV_UN_HLFW_LEN_FROM_DIVIDENT_W_LEN(2 * (FLEA_ECC_MAX_MOD_WORD_SIZE + 1))
  );
  FLEA_DECL_BUF(k_arr, flea_uword_t, 2 * FLEA_ECC_MAX_ORDER_WORD_SIZE + 1);
  FLEA_DECL_BUF(k_inv_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(n_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(s_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(r_arr, flea_uword_t, FLEA_ECC_MAX_ORDER_WORD_SIZE);
  FLEA_DECL_BUF(G_arr, flea_uword_t, 2 * FLEA_ECC_MAX_MOD_WORD_SIZE);
  FLEA_DECL_BUF(curve_word_arr, flea_uword_t, 3 * FLEA_ECC_MAX_MOD_WORD_SIZE);

# ifdef FLEA_HEAP_MODE
  flea_mpi_ulen_t vn_len, un_len, k_arr_word_len, prime_word_len, order_word_len, G_arr_word_len;
  flea_al_u16_t curve_word_arr_word_len, ecc_ws_mpi_arrs_word_len;
# endif

# ifdef FLEA_SCCM_USE_PUBKEY_INPUT_BASED_DELAY
  flea_ctr_mode_prng_t delay_prng__t;
# endif

  flea_mpi_div_ctx_t div_ctx;
  flea_al_u8_t i;
  FLEA_THR_BEG_FUNC();

# ifdef FLEA_SCCM_USE_PUBKEY_INPUT_BASED_DELAY
  flea_ctr_mode_prng_t__INIT(&delay_prng__t);
# endif
# ifdef FLEA_HEAP_MODE
  enc_field_len  = dom_par__pt->p__ru8.len__dtl;
  enc_order_len  = dom_par__pt->n__ru8.len__dtl;
  prime_word_len = FLEA_CEIL_WORD_LEN_FROM_BYTE_LEN(enc_field_len);
  order_word_len = FLEA_CEIL_WORD_LEN_FROM_BYTE_LEN(enc_order_len) + 1;

  k_arr_word_len = 2 * order_word_len + 32 / sizeof(flea_uword_t);
  G_arr_word_len = 2 * prime_word_len;
  curve_word_arr_word_len  = 3 * prime_word_len;
  ecc_ws_mpi_arrs_word_len = order_word_len;
  vn_len = FLEA_MPI_DIV_VN_HLFW_LEN_FROM_DIVISOR_W_LEN(order_word_len);
  un_len = FLEA_MPI_DIV_UN_HLFW_LEN_FROM_DIVIDENT_W_LEN(2 * (prime_word_len + 1)); // + 1 due to reducing R^2 !
  memset(ecc_ws_mpi_arrs, 0, sizeof(ecc_ws_mpi_arrs));
# endif /* ifdef FLEA_HEAP_MODE */
  // reseed PRNG with message and private key:
  FLEA_CCALL(THR_flea_rng__reseed_volatile(message, message_len));
  FLEA_CCALL(THR_flea_rng__reseed_volatile(priv_key_enc_arr, priv_key_enc_arr_len));
  FLEA_ALLOC_BUF(n_arr, order_word_len);
  FLEA_ALLOC_BUF(curve_word_arr, curve_word_arr_word_len);

# ifdef FLEA_SCCM_USE_PUBKEY_INPUT_BASED_DELAY
  FLEA_CCALL(THR_flea_ctr_mode_prng_t__ctor(&delay_prng__t, message, message_len));
  FLEA_CCALL(THR_flea_ctr_mode_prng_t__reseed(&delay_prng__t, priv_key_enc_arr, priv_key_enc_arr_len));
# endif
  FLEA_CCALL(
    THR_flea_curve_gfp_t__init_dp_array(
      &curve,
      // p_dp,
      dom_par__pt,
      curve_word_arr,
      FLEA_HEAP_OR_STACK_CODE(curve_word_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(curve_word_arr))
    )
  );
  FLEA_ALLOC_BUF(G_arr, G_arr_word_len);
  FLEA_CCALL(
    THR_flea_point_gfp_t__init(
      &G,
      dom_par__pt->gx__ru8.data__pcu8,
      dom_par__pt->gx__ru8.len__dtl,
      dom_par__pt->gy__ru8.data__pcu8,
      dom_par__pt->gy__ru8.len__dtl,
      G_arr,
      FLEA_HEAP_OR_STACK_CODE(G_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(G_arr))
    )
  );

  FLEA_ALLOC_BUF(vn, vn_len);
  FLEA_ALLOC_BUF(un, un_len);
  div_ctx.vn     = vn;
  div_ctx.un     = un;
  div_ctx.vn_len = FLEA_HEAP_OR_STACK_CODE(vn_len, FLEA_STACK_BUF_NB_ENTRIES(vn));
  div_ctx.un_len = FLEA_HEAP_OR_STACK_CODE(un_len, FLEA_STACK_BUF_NB_ENTRIES(un));

  flea_mpi_t__init(&n, n_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(n_arr)));
  // store cofactor in n:
  FLEA_CCALL(THR_flea_mpi_t__decode(&n, dom_par__pt->h__ru8.data__pcu8, dom_par__pt->h__ru8.len__dtl));
  FLEA_CCALL(THR_flea_point_gfp_t__validate_point(&G, &curve, &n, &div_ctx));

  FLEA_ALLOC_BUF(k_arr, k_arr_word_len);
  FLEA_ALLOC_BUF(k_inv_arr, order_word_len);
  FLEA_ALLOC_BUF(r_arr, order_word_len);
  FLEA_ALLOC_BUF(s_arr, order_word_len);
  flea_mpi_t__init(&k, k_arr, FLEA_HEAP_OR_STACK_CODE(k_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(k_arr)));
  flea_mpi_t__init(&r, r_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(r_arr)));
  flea_mpi_t__init(&s, s_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(s_arr)));
  flea_mpi_t__init(&k_inv, k_inv_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(k_inv_arr)));

  FLEA_CCALL(THR_flea_mpi_t__decode(&n, dom_par__pt->n__ru8.data__pcu8, dom_par__pt->n__ru8.len__dtl));

  while(flea_mpi_t__is_zero(&r) || flea_mpi_t__is_zero(&s)) // while r == 0 && s == 0
  {
    // generate k randomly
    do
    {
      FLEA_CCALL(THR_flea_mpi_t__random_integer(&k, &n));
    } while(flea_mpi_t__is_zero(&k));

    // mul Q=k*G
# ifdef FLEA_SCCM_USE_PUBKEY_INPUT_BASED_DELAY
    FLEA_CCALL(THR_flea_point_gfp_t__mul(&G, &k, &curve, do_use_add_always__b, &delay_prng__t));
# else
    FLEA_CCALL(THR_flea_point_gfp_t__mul(&G, &k, &curve, do_use_add_always__b));
# endif

    FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &r, &G.m_x, &n, &div_ctx));
    if(flea_mpi_t__is_zero(&r))
    {
      continue;
    }

# ifdef FLEA_HEAP_MODE
    for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
    {
      FLEA_ALLOC_MEM_ARR(ecc_ws_mpi_arrs[i], ecc_ws_mpi_arrs_word_len);
    }
# endif /* ifdef FLEA_HEAP_MODE */
    for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
    {
      flea_mpi_t__init(
        &mpi_worksp_arr[i],
        ecc_ws_mpi_arrs[i],
        FLEA_HEAP_OR_STACK_CODE(ecc_ws_mpi_arrs_word_len, FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs[i]))
      );
    }
    // k_inv = k^(-1)
    FLEA_CCALL(THR_flea_mpi_t__invert_odd_mod(&k_inv, &k, &n, mpi_worksp_arr));

    FLEA_CCALL(THR_flea_mpi_t__decode(&mpi_worksp_arr[0], message, message_len));
    // compute r*secret + OS2I(Hash)
    FLEA_CCALL(THR_flea_mpi_t__decode(&mpi_worksp_arr[2], priv_key_enc_arr, priv_key_enc_arr_len));
    FLEA_CCALL(THR_flea_mpi_t__mul(&k, &r, &mpi_worksp_arr[2]));
    FLEA_CCALL(THR_flea_mpi_t__add_in_place(&k, &mpi_worksp_arr[0], &mpi_worksp_arr[1]));
    FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &mpi_worksp_arr[0], &k, &n, &div_ctx)); // reduced k

    // compute s = k_inv * res

    FLEA_CCALL(THR_flea_mpi_t__mul(&k, &mpi_worksp_arr[0], &k_inv));
    FLEA_CCALL(THR_flea_mpi_t__divide(NULL, &s, &k, &n, &div_ctx)); // s in k

    // free early s.th. they never live together with the point mul
# ifdef FLEA_HEAP_MODE
    for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
    {
      FLEA_FREE_MEM_SET_NULL(ecc_ws_mpi_arrs[i]);
    }
# endif /* ifdef FLEA_HEAP_MODE */
  }
  *res_r_arr_len = flea_mpi_t__get_byte_size(&r);
  *res_s_arr_len = flea_mpi_t__get_byte_size(&s);
  FLEA_CCALL(THR_flea_mpi_t__encode(res_r_arr, *res_r_arr_len, &r));
  FLEA_CCALL(THR_flea_mpi_t__encode(res_s_arr, *res_s_arr_len, &s));

  FLEA_THR_FIN_SEC(
    FLEA_DO_IF_USE_HEAP_BUF(
      for(i = 0; i < FLEA_NB_ARRAY_ENTRIES(ecc_ws_mpi_arrs); i++)
  {
    FLEA_FREE_MEM_CHK_NULL(ecc_ws_mpi_arrs[i]);
  }
    );
    FLEA_FREE_BUF_SECRET_ARR(k_arr, FLEA_HEAP_OR_STACK_CODE(k_arr_word_len, FLEA_STACK_BUF_NB_ENTRIES(k_arr)));
    FLEA_FREE_BUF_FINAL(s_arr);
    FLEA_FREE_BUF_SECRET_ARR(k_inv_arr, FLEA_HEAP_OR_STACK_CODE(order_word_len, FLEA_STACK_BUF_NB_ENTRIES(k_inv_arr)));
    FLEA_FREE_BUF_FINAL(r_arr);
    FLEA_FREE_BUF_FINAL(curve_word_arr);
    FLEA_FREE_BUF_FINAL(n_arr);
    FLEA_FREE_BUF_FINAL(G_arr);
    FLEA_FREE_BUF_SECRET_ARR(vn, FLEA_HEAP_OR_STACK_CODE(vn_len, FLEA_STACK_BUF_NB_ENTRIES(vn)));
    FLEA_FREE_BUF_SECRET_ARR(un, FLEA_HEAP_OR_STACK_CODE(un_len, FLEA_STACK_BUF_NB_ENTRIES(un)));
    FLEA_DO_IF_USE_PUBKEY_INPUT_BASED_DELAY(
      flea_ctr_mode_prng_t__dtor(&delay_prng__t);
    )
  );
} /* THR_flea_ecdsa__raw_sign */

#endif // #ifdef FLEA_HAVE_ECDSA
