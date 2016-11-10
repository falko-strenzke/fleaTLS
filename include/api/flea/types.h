/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */


#ifndef __types_H_
#define __types_H_

#include "flea/error.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned char flea_u8_t;
typedef signed char flea_s8_t;
typedef unsigned short flea_u16_t;
typedef short flea_s16_t;
typedef unsigned int flea_u32_t;
typedef int flea_s32_t;
typedef unsigned long long flea_u64_t;
typedef long long flea_s64_t;

// "at least" width types
typedef flea_u32_t flea_al_u8_t;
typedef flea_s32_t flea_al_s8_t;
typedef flea_u32_t flea_al_u16_t;
typedef flea_s32_t flea_al_s16_t;

typedef flea_u16_t flea_hlf_uword_t;
typedef flea_s16_t flea_hlf_sword_t;
typedef flea_u32_t flea_uword_t;
typedef flea_s32_t flea_sword_t;
typedef flea_u64_t flea_dbl_uword_t;
typedef flea_s64_t flea_dbl_sword_t;

typedef flea_u32_t flea_cycles_t;

#define FLEA_LOG2_WORD_BIT_SIZE 5
#define FLEA_UWORD_MAX ((flea_uword_t)(-1))
#define FLEA_HLF_UWORD_MAX ((flea_hlf_uword_t)(-1))

typedef flea_al_u8_t flea_bool_t;

/**
 * byte lengths of mpis
 */
typedef flea_u16_t flea_mpi_ulen_t;
typedef flea_s16_t flea_mpi_slen_t;

/**
 * bit lengths of mpis
 */
typedef flea_u16_t flea_mpi_ubil_t;
typedef flea_s16_t flea_mpi_sbil_t;

/**
 * type indicating possible data lengths
 */
#ifdef FLEA_HAVE_DTL_32BIT
typedef flea_u32_t flea_dtl_t;
#else
typedef flea_al_u16_t flea_dtl_t;
#endif

typedef flea_u16_t flea_x509_len_t;

#define FLEA_FALSE 0
#define FLEA_TRUE 1

#ifdef __cplusplus
}
#endif

#endif /* h-guard */
