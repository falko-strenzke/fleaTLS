/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "internal/common/default.h"
#include "flea/hash.h"
#include "self_test.h"
#include "flea/error_handling.h"
#include "flea/alloc.h"
#include "flea/algo_config.h"
#include <string.h>
static flea_err_t THR_flea_test_hash_init_dtor()
{
  FLEA_DECL_OBJ(ctx, flea_hash_ctx_t);
  flea_hash_ctx_t ctx2;
  FLEA_THR_BEG_FUNC();
  flea_hash_ctx_t__INIT(&ctx2);

  FLEA_THR_FIN_SEC(
    flea_hash_ctx_t__dtor(&ctx);
    flea_hash_ctx_t__dtor(&ctx2);
  );
}

static flea_err_t THR_flea_test_hash_mess_result(flea_hash_id_t id__t, const flea_u8_t *m__pcu8, flea_al_u16_t m_len__al_u16, const flea_u8_t *exp_res__pcu8, flea_al_u16_t exp_res_len__alu16)
{
  FLEA_DECL_BUF(digest__b_u8, flea_u8_t, FLEA_MAX_HASH_OUT_LEN);
  FLEA_DECL_BUF(digest_copy__b_u8, flea_u8_t, FLEA_MAX_HASH_OUT_LEN);

  FLEA_DECL_OBJ(ctx, flea_hash_ctx_t);
  FLEA_DECL_OBJ(ctx_copy, flea_hash_ctx_t);
  FLEA_THR_BEG_FUNC();

  FLEA_ALLOC_BUF(digest__b_u8, flea_hash__get_output_length_by_id(id__t));
  FLEA_ALLOC_BUF(digest_copy__b_u8, flea_hash__get_output_length_by_id(id__t));


  FLEA_CCALL(THR_flea_hash_ctx_t__ctor(&ctx, id__t));

  FLEA_CCALL(THR_flea_hash_ctx_t__update(&ctx, m__pcu8, m_len__al_u16));

  FLEA_CCALL(THR_flea_hash_ctx_t__ctor_copy(&ctx_copy, &ctx));

  FLEA_CCALL(THR_flea_hash_ctx_t__final(&ctx, digest__b_u8));
  if(exp_res_len__alu16 != flea_hash_ctx_t__get_output_length(&ctx))
  {
    FLEA_THROW("error with length of hash output in test", FLEA_ERR_FAILED_TEST);
  }

  FLEA_CCALL(THR_flea_hash_ctx_t__final(&ctx_copy, digest_copy__b_u8));
  if(exp_res_len__alu16 != flea_hash_ctx_t__get_output_length(&ctx_copy))
  {
    FLEA_THROW("error with length of hash output in test (copied ctx object)", FLEA_ERR_FAILED_TEST);
  }

  if(memcmp(digest__b_u8, exp_res__pcu8, flea_hash_ctx_t__get_output_length(&ctx)))
  {
    FLEA_THROW("error with hash result value", FLEA_ERR_FAILED_TEST);
  }
  if(memcmp(digest__b_u8, digest_copy__b_u8, flea_hash_ctx_t__get_output_length(&ctx)))
  {
    FLEA_THROW("error with hash result value (copied ctx object)", FLEA_ERR_FAILED_TEST);
  }
  FLEA_THR_FIN_SEC(
    FLEA_FREE_BUF_FINAL(digest__b_u8);
    FLEA_FREE_BUF_FINAL(digest_copy__b_u8);
    flea_hash_ctx_t__dtor(&ctx);
    flea_hash_ctx_t__dtor(&ctx_copy);
  );
} /* THR_flea_test_hash_mess_result */

static flea_err_t THR_flea_test_hash_abc(flea_hash_id_t id__t, const flea_u8_t *exp_res__pcu8, flea_al_u16_t exp_res_len__alu16)
{
  flea_u8_t m__a_u8[] = { 0x61, 0x62, 0x63 };

  return THR_flea_test_hash_mess_result(id__t, m__a_u8, sizeof(m__a_u8), exp_res__pcu8, exp_res_len__alu16);
}

flea_err_t THR_flea_test_hash()
{
  FLEA_THR_BEG_FUNC();
#ifdef FLEA_HAVE_SHA1
  const flea_u8_t exp_res_abc_sha1__a_u8 [] = {
    0xa9, 0x99, 0x3e, 0x36,
    0x47, 0x06, 0x81, 0x6a,
    0xba, 0x3e, 0x25, 0x71,
    0x78, 0x50, 0xc2, 0x6c,
    0x9c, 0xd0, 0xd8, 0x9d
  };
#endif
#ifdef FLEA_HAVE_SHA224_256
  const flea_u8_t exp_res_abc_sha224__a_u8[] = { 0x23, 0x09, 0x7d, 0x22, 0x34, 0x05, 0xd8, 0x22, 0x86, 0x42, 0xa4, 0x77, 0xbd, 0xa2, 0x55, 0xb3, 0x2a, 0xad, 0xbc, 0xe4, 0xbd, 0xa0, 0xb3, 0xf7, 0xe3, 0x6c, 0x9d, 0xa7 };

  const flea_u8_t longer_message_sha256__cau8[] = {
    0x30, 0x82, 0x04, 0x6b, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x09, 0x00, 0x83, 0x01, 0xc0, 0x9d, 0x7d, 0xf7, 0xc9, 0x7e, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x6f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x44, 0x45, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03,
    0x55, 0x04, 0x08, 0x13, 0x06, 0x48, 0x65, 0x73, 0x73, 0x65, 0x6e, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13, 0x09, 0x44, 0x61, 0x72, 0x6d, 0x73, 0x74, 0x61, 0x64,
    0x74, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x11, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x20, 0x47, 0x6d, 0x62, 0x48, 0x31,
    0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x16, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x20, 0x43, 0x41, 0x20, 0x32, 0x30, 0x31, 0x36,
    0x2d, 0x31, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x36, 0x30, 0x35, 0x33, 0x30, 0x31, 0x35, 0x30, 0x35, 0x34, 0x34, 0x5a, 0x17, 0x0d, 0x32, 0x31, 0x30, 0x35, 0x32, 0x39, 0x31, 0x35, 0x30,
    0x35, 0x34, 0x34, 0x5a, 0x30, 0x71, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x44, 0x45, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x06,
    0x48, 0x65, 0x73, 0x73, 0x65, 0x6e, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13, 0x09, 0x44, 0x61, 0x72, 0x6d, 0x73, 0x74, 0x61, 0x64, 0x74, 0x31, 0x1a, 0x30, 0x18,
    0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x11, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x20, 0x47, 0x6d, 0x62, 0x48, 0x31, 0x21, 0x30, 0x1f, 0x06, 0x03,
    0x55, 0x04, 0x03, 0x13, 0x18, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x61, 0x6c, 0x2e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x2e, 0x64, 0x65, 0x30,
    0x82, 0x02, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00, 0x30, 0x82, 0x02, 0x0a, 0x02, 0x82, 0x02,
    0x01, 0x00, 0xb2, 0x95, 0x08, 0x87, 0x31, 0x99, 0x38, 0xd6, 0xea, 0x61, 0x7f, 0xbf, 0xb0, 0x84, 0xf4, 0xc6, 0x34, 0x2d, 0x65, 0x89, 0x88, 0x47, 0xed, 0x81, 0xaf, 0x05, 0x09, 0x36,
    0x25, 0x97, 0x6d, 0x51, 0xa8, 0x3e, 0xfe, 0x76, 0x16, 0xe4, 0x22, 0x86, 0x55, 0x9a, 0xb1, 0x8a, 0x56, 0x24, 0x01, 0x32, 0x6b, 0xbd, 0x46, 0xeb, 0x35, 0xa6, 0xdd, 0xee, 0x89, 0xce,
    0xd3, 0xb9, 0x04, 0x57, 0x50, 0x20, 0xdc, 0x02, 0xfb, 0x06, 0x81, 0x98, 0xaa, 0xaa, 0x5a, 0x41, 0xe2, 0x1a, 0x33, 0x24, 0x2d, 0x19, 0xd0, 0xa1, 0x7c, 0xe1, 0x6d, 0x67, 0xb8, 0xf7,
    0x84, 0x5e, 0xb0, 0x3a, 0xec, 0x57, 0xe0, 0x66, 0x6f, 0x08, 0xa7, 0x6e, 0x4e, 0x33, 0x01, 0x3a, 0x82, 0x07, 0x6e, 0xc0, 0xb0, 0x67, 0x30, 0x8c, 0xe8, 0xbb, 0x21, 0xdd, 0x2e, 0xef,
    0x0d, 0x31, 0xbb, 0x9e, 0x66, 0x09, 0xe0, 0x43, 0x23, 0xfb, 0xff, 0x0c, 0x7a, 0x7d, 0x12, 0x09, 0xe9, 0x0b, 0x12, 0x07, 0xaa, 0x29, 0x87, 0x06, 0x4d, 0x78, 0x03, 0xf1, 0x29, 0xd8,
    0xa7, 0x14, 0x50, 0xcc, 0xdb, 0x14, 0x09, 0x09, 0xf6, 0x4d, 0xd8, 0xb2, 0xf6, 0x08, 0xfc, 0xcf, 0xab, 0x20, 0xc5, 0x29, 0x1c, 0xbb, 0x9d, 0x8f, 0x5a, 0x44, 0xeb, 0x38, 0x18, 0x8d,
    0xb5, 0x81, 0xc9, 0x19, 0x92, 0xfa, 0x3d, 0xaf, 0x03, 0x08, 0x7a, 0xe0, 0x6b, 0x2d, 0xe3, 0x1e, 0x02, 0xf2, 0xb5, 0x99, 0xb8, 0xe6, 0x18, 0x2e, 0x20, 0x48, 0x1e, 0x22, 0xe0, 0x86,
    0xe4, 0xed, 0xa1, 0xe6, 0x71, 0x29, 0x9e, 0xac, 0x95, 0xb8, 0x22, 0xb1, 0x5b, 0xe0, 0xc7, 0x45, 0x96, 0x65, 0x30, 0x3e, 0x37, 0x84, 0x27, 0xfb, 0xa2, 0x82, 0x8e, 0xca, 0x6a, 0x92,
    0x9f, 0x4e, 0xf5, 0x24, 0xef, 0xe3, 0xc7, 0x03, 0x2e, 0x64, 0xda, 0xfa, 0x2c, 0x66, 0xac, 0x54, 0x9b, 0x1b, 0x38, 0x42, 0xad, 0x34, 0x81, 0xb1, 0x3d, 0x75, 0x28, 0xb8, 0xaa, 0xad,
    0x82, 0xfd, 0xf8, 0xb5, 0x8c, 0x48, 0x68, 0xa1, 0x07, 0xff, 0x16, 0x05, 0xc5, 0x73, 0x09, 0x71, 0x48, 0xf8, 0x16, 0xc1, 0x68, 0xd7, 0x04, 0xdb, 0xcc, 0x3f, 0x0f, 0x03, 0x08, 0x08,
    0xf7, 0x18, 0xf9, 0x7f, 0xf2, 0x0f, 0x58, 0x7a, 0x85, 0x39, 0x84, 0x60, 0x15, 0xdb, 0xf2, 0x2d, 0x91, 0xf3, 0x4d, 0x99, 0x0e, 0xdc, 0x89, 0xed, 0x94, 0x12, 0x6f, 0xeb, 0xee, 0xe0,
    0x8f, 0x07, 0xb8, 0xd9, 0x0d, 0x88, 0xf4, 0x0b, 0x01, 0x16, 0xa9, 0xfa, 0x98, 0x49, 0xb1, 0xba, 0x52, 0x1b, 0x78, 0x0b, 0xdf, 0x59, 0xf6, 0xe1, 0x6f, 0x88, 0xdd, 0xe9, 0xb9, 0x12,
    0xd5, 0x3c, 0x4c, 0x9d, 0x98, 0x36, 0xb3, 0x97, 0xf0, 0xfb, 0xfc, 0x13, 0xae, 0x15, 0x7a, 0xa7, 0x50, 0x47, 0xbc, 0x18, 0xb1, 0x2e, 0xe8, 0xfc, 0x80, 0xd4, 0x0d, 0x6d, 0xb2, 0xfd,
    0x87, 0x47, 0x97, 0x18, 0x56, 0x78, 0x64, 0x05, 0x5d, 0x9b, 0xa1, 0xc8, 0x47, 0xdc, 0x0c, 0x85, 0x78, 0xa0, 0x3b, 0x4c, 0x3f, 0xf8, 0x95, 0x75, 0x23, 0xad, 0xb0, 0xd9, 0x06, 0x64,
    0x01, 0x69, 0x24, 0x2e, 0xcd, 0x74, 0xa1, 0x73, 0x4b, 0xba, 0x60, 0xef, 0x1d, 0x6e, 0xda, 0xc3, 0x2d, 0xac, 0x0e, 0x55, 0x98, 0x4d, 0x2a, 0xfa, 0x0a, 0xb3, 0x59, 0xcf, 0xda, 0x91,
    0x16, 0xb0, 0x55, 0x08, 0xc7, 0xae, 0xa1, 0x24, 0xe9, 0x06, 0xb1, 0x0f, 0x2b, 0xf0, 0x58, 0xf6, 0xf9, 0x0d, 0xc3, 0xb9, 0x4a, 0x24, 0xf1, 0x10, 0x82, 0xd5, 0xec, 0x1e, 0x9d, 0x2f,
    0xcc, 0xa6, 0xd9, 0xf6, 0x39, 0x6b, 0x15, 0x81, 0x6b, 0x90, 0x54, 0xa0, 0xf1, 0x92, 0xf7, 0xbb, 0x29, 0xec, 0x8e, 0x6f, 0xa3, 0x48, 0x75, 0x1d, 0xe3, 0x61, 0x81, 0x49, 0x1a, 0x4d,
    0x38, 0x63, 0xc2, 0xc7, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x82, 0x01, 0x1e, 0x30, 0x82, 0x01, 0x1a, 0x30, 0x09, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0b,
    0x06, 0x03, 0x55, 0x1d, 0x0f, 0x04, 0x04, 0x03, 0x02, 0x03, 0xe8, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x57, 0xd1, 0xae, 0x9a, 0x43, 0x92, 0xfc, 0x42,
    0x35, 0xc1, 0xed, 0xa9, 0x14, 0xb7, 0x21, 0xb3, 0xac, 0xb6, 0x01, 0xdb, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x34, 0xf8, 0x9a, 0x66, 0x00,
    0xd9, 0x85, 0x4c, 0xb2, 0xfb, 0x28, 0xed, 0x86, 0x5b, 0x58, 0x82, 0x70, 0x9f, 0x2d, 0xd5, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x25, 0x04, 0x16, 0x30, 0x14, 0x06, 0x08, 0x2b, 0x06,
    0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02, 0x30, 0x29, 0x06, 0x03, 0x55, 0x1d, 0x11, 0x04, 0x22, 0x30, 0x20, 0x82, 0x18, 0x69,
    0x6e, 0x74, 0x65, 0x72, 0x6e, 0x61, 0x6c, 0x2e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x2e, 0x64, 0x65, 0x87, 0x04, 0x5e, 0x10, 0x51, 0x0f, 0x30,
    0x42, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01, 0x04, 0x36, 0x30, 0x34, 0x30, 0x32, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0x86, 0x26, 0x68,
    0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x70, 0x6b, 0x69, 0x2e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x2e, 0x64, 0x65, 0x2f, 0x63, 0x73, 0x5f, 0x72,
    0x6f, 0x6f, 0x74, 0x2e, 0x63, 0x72, 0x74, 0x30, 0x32, 0x06, 0x03, 0x55, 0x1d, 0x1f, 0x04, 0x2b, 0x30, 0x29, 0x30, 0x27, 0xa0, 0x25, 0xa0, 0x23, 0x86, 0x21, 0x68, 0x74, 0x74, 0x70,
    0x3a, 0x2f, 0x2f, 0x70, 0x6b, 0x69, 0x2e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x2e, 0x64, 0x65, 0x2f, 0x63, 0x73, 0x2e, 0x63, 0x72, 0x6c
  };

  const flea_u8_t exp_result_sha256_of_longer_message__cau8 [] = { 0x35, 0xa7, 0xc9, 0x0b, 0xe4, 0x81, 0xc9, 0xaa, 0x6d, 0x4b, 0xfe, 0x47, 0xc8, 0xdf, 0xa4, 0xa7, 0x85, 0xb0, 0x77, 0x0f, 0x8e, 0x91, 0x69, 0x27, 0xe1, 0xd0, 0x2f, 0xe4, 0x10, 0xaf, 0x36, 0x05 };
#endif /* ifdef FLEA_HAVE_SHA224_256 */

#ifdef FLEA_HAVE_MD5
  flea_u8_t exp_res_abc_md5__a_u8[] = { 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72 };
#endif
#ifdef FLEA_HAVE_SHA1
  FLEA_CCALL(THR_flea_test_hash_abc(flea_sha1, exp_res_abc_sha1__a_u8, sizeof(exp_res_abc_sha1__a_u8)));
#endif
#ifdef FLEA_HAVE_SHA224_256
  FLEA_CCALL(THR_flea_test_hash_abc(flea_sha224, exp_res_abc_sha224__a_u8, sizeof(exp_res_abc_sha224__a_u8)));

  FLEA_CCALL(THR_flea_test_hash_mess_result(flea_sha256, longer_message_sha256__cau8, sizeof(longer_message_sha256__cau8), exp_result_sha256_of_longer_message__cau8, sizeof(exp_result_sha256_of_longer_message__cau8)));
#endif
#ifdef FLEA_HAVE_MD5
  FLEA_CCALL(THR_flea_test_hash_abc(flea_md5, exp_res_abc_md5__a_u8, sizeof(exp_res_abc_md5__a_u8)));
#endif
  FLEA_CCALL(THR_flea_test_hash_init_dtor());
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_test_hash */
