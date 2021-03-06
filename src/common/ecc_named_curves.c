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
#include "flea/ecc_named_curves.h"

#ifdef FLEA_HAVE_ECC
const flea_u8_t brainpool_version_one_oid_prefix[] = {0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01};

const flea_u8_t nist_secp_curve_prefix[] = {0x2B, 0x81, 0x04, 0x00};
const flea_u8_t P256_P192_curve_prefix[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01};

flea_err_e THR_flea_ecc_gfp_dom_par_t__set_by_named_curve_oid(
  flea_ec_dom_par_ref_t* dp_to_set__pt,
  const flea_u8_t*       oid__pcu8,
  flea_al_u8_t           oid_len__alu8
)
{
  flea_ec_dom_par_id_e result_id;

  FLEA_THR_BEG_FUNC();
  if((oid_len__alu8 == sizeof(brainpool_version_one_oid_prefix) + 1) &&
    !memcmp(oid__pcu8, brainpool_version_one_oid_prefix, sizeof(brainpool_version_one_oid_prefix)))
  {
    /* brainpoolP160r1 OBJECT IDENTIFIER ::= {versionOne 1}
     * brainpoolP192r1 OBJECT IDENTIFIER ::= {versionOne 3}
     * brainpoolP224r1 OBJECT IDENTIFIER ::= {versionOne 5}
     * brainpoolP256r1 OBJECT IDENTIFIER ::= {versionOne 7}
     * brainpoolP320r1 OBJECT IDENTIFIER ::= {versionOne 9}
     * brainpoolP384r1 OBJECT IDENTIFIER ::= {versionOne 11}
     * brainpoolP512r1 OBJECT IDENTIFIER ::= {versionOne 13}
     */
    flea_al_u8_t last_byte = oid__pcu8[sizeof(brainpool_version_one_oid_prefix)];
    if(last_byte % 2 == 0 || last_byte > 13)
    {
      FLEA_THROW("invalid/unkonwn named curve OID", FLEA_ERR_ECC_INV_BUILTIN_DP_ID);
    }
    last_byte /= 2;
    result_id  = (flea_ec_dom_par_id_e) (flea_brainpoolP160r1 + last_byte);
  }
  else if((oid_len__alu8 == sizeof(nist_secp_curve_prefix) + 1) &&
    !memcmp(oid__pcu8, nist_secp_curve_prefix, sizeof(nist_secp_curve_prefix)))
  {
    /*secp224r1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3) certicom(132) curve(0) 33 }
     * secp384r1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3) certicom(132) curve(0) 34 }
     * secp521r1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3) certicom(132) curve(0) 35 }
     */
    switch(oid__pcu8[sizeof(nist_secp_curve_prefix)])
    {
        case 8:
          result_id = flea_secp160r1;
          break;
        case 30:
          result_id = flea_secp160r2;
          break;
        case 33:
          result_id = flea_secp224r1;
          break;
        case 34:
          result_id = flea_secp384r1;
          break;
        case 35:
          result_id = flea_secp521r1;
          break;
        default:
          FLEA_THROW("invalid/unkonwn named curve OID", FLEA_ERR_ECC_INV_BUILTIN_DP_ID);
    }
  }
  else if(!flea_memcmp_wsize(P256_P192_curve_prefix, sizeof(P256_P192_curve_prefix), oid__pcu8, oid_len__alu8 - 1))
  {
    /*     secp256r1 OBJECT IDENTIFIER ::= {
     *     iso(1) member-body(2) us(840) ansi-X9-62(10045) curves(3) prime(1) 7 }*/
    // flea_secp192r1: 1.2.840.10045.3.1.1
    switch(oid__pcu8[sizeof(P256_P192_curve_prefix)])
    {
        case 1:
          result_id = flea_secp192r1;
          break;
        case 7:
          result_id = flea_secp256r1;
          break;
        default:
          FLEA_THROW("invalid/unkonwn named curve OID", FLEA_ERR_ECC_INV_BUILTIN_DP_ID);
    }
  }
  else
  {
    FLEA_THROW("invalid/unkonwn named curve OID", FLEA_ERR_ECC_INV_BUILTIN_DP_ID);
  }
  FLEA_CCALL(THR_flea_ec_dom_par_ref_t__set_by_builtin_id(dp_to_set__pt, result_id));
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_ecc_gfp_dom_par_t__set_by_named_curve_oid */

#endif /* #ifdef FLEA_HAVE_ECC */
