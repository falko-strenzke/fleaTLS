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
#include "flea/types.h"
#include "flea/alloc.h"
#include "flea/cert_verify.h"
#include "flea/array_util.h"
#include "flea/asn1_date.h"
#include "flea/error_handling.h"
#include "internal/common/ber_dec.h"
#include "flea/x509.h"
#include "internal/common/crl_int.h"
#include "flea/hostn_ver.h"
#include "flea/cert_path.h"
#include "internal/common/cert_path_int.h"
#include "internal/common/cert_verify_int.h"
#include "internal/common/lib_int.h"
#include "internal/common/pubkey_int2.h"
#include "internal/common/tls/hostn_ver_int.h"

#ifdef FLEA_HAVE_ASYM_SIG

# define END_OF_COLL 0xFFFF


static flea_al_u16_t find_cert(
  const flea_x509_cert_info_t* cert_to_find__pt,
  const flea_x509_cert_info_t* cert_collection__bt,
  flea_al_u16_t                cert_collection_size__alu16,
  flea_al_u16_t                start_pos__alu16
)
{
  flea_al_u16_t i;

  for(i = start_pos__alu16; i < cert_collection_size__alu16; i++)
  {
    /* compare subject DN's */
    if(!flea_byte_vec_t__cmp(
        &cert_to_find__pt->cert_ref__t.subject__t.raw_dn_complete__t,
        &cert_collection__bt[i].cert_ref__t.subject__t.raw_dn_complete__t
      ))
    {
      /* compare tbs */

      if(!flea_byte_vec_t__cmp(&cert_to_find__pt->ref_to_tbs__rcu8, &cert_collection__bt[i].ref_to_tbs__rcu8))
      {
        if(FLEA_DER_REF_IS_ABSENT(&cert_collection__bt[i].cert_ref__t.cert_signature_as_bit_string__t) ||
          FLEA_DER_REF_IS_ABSENT(&cert_to_find__pt->cert_ref__t.cert_signature_as_bit_string__t) ||
          !flea_byte_vec_t__cmp(
            &cert_to_find__pt->cert_ref__t.cert_signature_as_bit_string__t,
            &cert_collection__bt[i].cert_ref__t.cert_signature_as_bit_string__t
          ))
          return i;
      }
    }
  }
  return END_OF_COLL;
} /* find_cert */

static flea_al_u16_t find_issuer(
  const flea_x509_cert_ref_t*  cert_to_find_issuer_to__pt,
  const flea_x509_cert_info_t* cert_collection__bt,
  flea_al_u16_t                cert_collection_size__alu16,
  flea_al_u16_t                start_pos__alu16,
  flea_u16_t*                  used_chain__pu16,
  flea_al_u16_t                used_chain_len__alu16
)
{
  //  search through collection for matching DN and AKIE
  flea_al_u16_t i;

  for(i = start_pos__alu16; i < cert_collection_size__alu16; i++)
  {
    /* compare subject DN's */
    if(!flea_byte_vec_t__cmp(
        &cert_to_find_issuer_to__pt->issuer__t.raw_dn_complete__t,
        &cert_collection__bt[i].cert_ref__t.subject__t.raw_dn_complete__t
      ))
    {
      flea_bool_t already_used__b = FLEA_FALSE;
      // check if that candidate is not yet part of the chain
      flea_al_u16_t j;
      for(j = 0; j < used_chain_len__alu16; j++)
      {
        if(used_chain__pu16[j] == i)
        {
          already_used__b = FLEA_TRUE;
          break;
        }
      }
      if(!already_used__b)
      {
        return i;
      }
    }
  }
  return END_OF_COLL;
}

static flea_bool_t is_cert_trusted(const flea_x509_cert_info_t* cert_ref__pt)
{
  return cert_ref__pt->is_trusted__b;
}

void flea_cpv_t__dtor(flea_cpv_t* cpv__pt)
{
# ifdef FLEA_HEAP_MODE
  FLEA_FREE_MEM_CHK_NULL(cpv__pt->chain__bu16);
  FLEA_FREE_MEM_CHK_NULL(cpv__pt->crl_collection__brcu8);
  FLEA_FREE_MEM_CHK_NULL(cpv__pt->cert_collection__bt);
# endif
  flea_cpv_t__INIT(cpv__pt);
}

static flea_err_e THR_validate_cert_path(
  flea_cpv_t*                  cert_cpv__pt,
  const flea_gmt_time_t*       arg_compare_time_mbn__pt,
  flea_pubkey_t*               key_to_construct_mbn__pt,
  flea_x509_validation_flags_e cert_ver_flags__e
)
{
  flea_s32_t i;

  flea_pubkey_t pubkey_for_crl_ver__t;
  flea_al_u16_t chain_len__alu16 = cert_cpv__pt->chain_pos__u16 + 1;
  flea_al_u16_t m_path__u16      = chain_len__alu16;

  FLEA_THR_BEG_FUNC();
  flea_pubkey_t__INIT(&pubkey_for_crl_ver__t);

  if(chain_len__alu16 == 0)
  {
    FLEA_THROW("attempted to verify an empty certificate path", FLEA_ERR_INV_ARG);
  }

  /* from TA to target cert */
  for(i = chain_len__alu16 - 1; i >= 0; i--)
  {
    flea_x509_cert_info_t* current__pt = &cert_cpv__pt->cert_collection__bt[cert_cpv__pt->chain__bu16[i]];
    flea_bool_t is_current_ta;
    flea_bool_t is_current_target;
    flea_basic_constraints_t* basic_constraints__pt;
    is_current_ta     = (i == (flea_s32_t) chain_len__alu16 - 1);
    is_current_target = (i == 0);

    basic_constraints__pt = &current__pt->cert_ref__t.extensions__t.basic_constraints__t;
    if(!flea_x509_is_cert_self_issued(&current__pt->cert_ref__t) && !is_current_target)
    {
      if(m_path__u16 == 0)
      {
        FLEA_THROW("path len constraint exceeded", FLEA_ERR_CERT_PATH_LEN_CONSTR_EXCEEDED);
      }

      m_path__u16 -= 1;
    }

    if(basic_constraints__pt->is_present__u8)
    {
      if(basic_constraints__pt->has_path_len__b)
      {
        m_path__u16 = FLEA_MIN(basic_constraints__pt->path_len__u16, m_path__u16);
      }
    }
    FLEA_CCALL(
      THR_flea_cert_path__validate_single_cert(
        &current__pt->cert_ref__t,
        is_current_ta,
        is_current_target,
        arg_compare_time_mbn__pt
      )
    );
  }

  /* from TA to target cert */
  for(i = (flea_s32_t) (chain_len__alu16 - 2); i >= 0; i--)
  {
    flea_bool_t is_ca_cert__b = (i != 0) ? FLEA_TRUE : FLEA_FALSE;
    flea_x509_cert_info_t* subject__pt = &cert_cpv__pt->cert_collection__bt[cert_cpv__pt->chain__bu16[i]];
    flea_x509_cert_info_t* issuer__pt  = &cert_cpv__pt->cert_collection__bt[cert_cpv__pt->chain__bu16[i + 1]];

    /* verify against subsequent certificate */
    FLEA_CCALL(THR_flea_x509_verify_cert_info_signature(subject__pt, issuer__pt, cert_ver_flags__e));

    if((((cert_cpv__pt->rev_chk_mode__e == flea_rev_chk_only_ee) && !is_ca_cert__b) ||
      (cert_cpv__pt->rev_chk_mode__e == flea_rev_chk_all)))
    {
      FLEA_CCALL(THR_flea_pubkey_t__ctor_cert(&pubkey_for_crl_ver__t, &issuer__pt->cert_ref__t));


      if(issuer__pt->cert_ref__t.extensions__t.key_usage__t.is_present__u8)
      {
        if(!(issuer__pt->cert_ref__t.extensions__t.key_usage__t.purposes__u16 & flea_ku_crl_sign))
        {
          FLEA_THROW(
            "CRL issuer has key usage extension without the CRLSign bit set",
            FLEA_ERR_X509_CRL_ISSUER_WO_CRL_SIGN
          );
        }
      }

      FLEA_CCALL(
        THR_flea_crl__check_revocation_status(
          cert_cpv__pt->crl_collection__brcu8,
          cert_cpv__pt->nb_crls__u16,
          arg_compare_time_mbn__pt,
          is_ca_cert__b,
          &subject__pt->cert_ref__t.issuer__t.raw_dn_complete__t,
          &subject__pt->cert_ref__t.serial_number__t,
          subject__pt->cert_ref__t.extensions__t.crl_distr_point__t.is_present__u8 ? &subject__pt->cert_ref__t.
          extensions__t.crl_distr_point__t.
          raw_ref__t : NULL,
          &pubkey_for_crl_ver__t,
          cert_ver_flags__e
        )
      );
      flea_pubkey_t__dtor(&pubkey_for_crl_ver__t);
    }
  }
  if(key_to_construct_mbn__pt)
  {
    FLEA_CCALL(
      THR_flea_pubkey_t__ctor_cert(
        key_to_construct_mbn__pt,
        &cert_cpv__pt->cert_collection__bt[cert_cpv__pt->chain__bu16[0]].cert_ref__t
      )
    );
    FLEA_CCALL(
      THR_flea_pubkey_t__ensure_key_strength(
        key_to_construct_mbn__pt,
        flea_pk_sec_lev_from_bit_mask(FLEA_PK_SEC_LEV_BIT_MASK_FROM_X509_FLAGS(cert_ver_flags__e))
      )
    );
  }

  FLEA_THR_FIN_SEC(
    flea_pubkey_t__dtor(&pubkey_for_crl_ver__t);
  );
} /* THR_validate_cert_path */

flea_err_e THR_flea_cpv_t__validate(
  flea_cpv_t*            cert_cpv__pt,
  const flea_gmt_time_t* time_mbn__pt
)
{
  return THR_flea_cpv_t__validate_and_create_pub_key(cert_cpv__pt, time_mbn__pt, NULL);
}

flea_err_e THR_flea_cpv_t__validate_and_hostid_and_create_pub_key(
  flea_cpv_t*            cert_cpv__pt,
  const flea_gmt_time_t* time_mbn__pt,
  const flea_byte_vec_t* host_id__pcrcu8,
  flea_host_id_type_e    host_id_type,
  flea_pubkey_t*         key_to_construct_mbn__pt
)
{
  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(
    THR_flea_x509__vrfy_tls_srv_id(
      host_id__pcrcu8,
      host_id_type,
      &cert_cpv__pt->cert_collection__bt[0].cert_ref__t
    )
  );
  FLEA_CCALL(
    THR_flea_cpv_t__validate_and_create_pub_key(
      cert_cpv__pt,
      time_mbn__pt,
      key_to_construct_mbn__pt
    )
  );
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_cpv_t__validate_and_create_pub_key(
  flea_cpv_t*            cert_cpv__pt,
  const flea_gmt_time_t* time_mbn__pt,
  flea_pubkey_t*         key_to_construct_mbn__pt
)
{
  flea_u16_t* chain_pos__pu16 = &cert_cpv__pt->chain_pos__u16;
  flea_x509_cert_info_t* cert_collection__bt = cert_cpv__pt->cert_collection__bt;
  flea_x509_cert_info_t* target_cert__pt     = &cert_collection__bt[0];
  flea_al_u16_t target_pos = 0;
  flea_u16_t* chain__bu16  = cert_cpv__pt->chain__bu16;
  flea_al_u16_t cert_collection_size__alu16 = cert_cpv__pt->cert_collection_size__u16;

  FLEA_THR_BEG_FUNC();
  *chain_pos__pu16 = 0;
  FLEA_SET_ARR(chain__bu16, 0, FLEA_MAX_CERT_CHAIN_DEPTH);
  if(cert_collection_size__alu16 == END_OF_COLL)
  {
    FLEA_THROW("exceeded maximal number of certificates in collection", FLEA_ERR_INV_ARG);
  }
  if(cert_collection_size__alu16 == 0)
  {
    FLEA_THROW("no certificate collection provided for path validation", FLEA_ERR_CERT_PATH_NO_TRUSTED_CERTS);
  }
  if(FLEA_DER_REF_IS_ABSENT(&target_cert__pt->cert_ref__t.cert_signature_as_bit_string__t))
  {
    FLEA_THROW("target certificate carries no signature", FLEA_ERR_INV_ARG);
  }
  /* check if the target cert is directly trusted */
# if FLEA_MAX_CERT_CHAIN_DEPTH < 2
#  error FLEA_MAX_CERT_CHAIN_DEPTH < 2
# endif
  // cover the target cert appearing as trusted in the coll by searching
  // through it, and setting current as trusted if it is found as trusted. at the start of the construction loop then
  // it is checked if the current is trusted
  while(target_pos != END_OF_COLL)
  {
    target_pos = find_cert(target_cert__pt, &cert_collection__bt[1], cert_collection_size__alu16 - 1, target_pos); // find by TBS match and possibly signature match (if both signatures are available)

    if((target_pos != END_OF_COLL))
    {
      if(is_cert_trusted(&cert_collection__bt[target_pos]))
      {
        target_cert__pt->is_trusted__b = FLEA_TRUE;
        break;
      }
      target_pos++;
    }
  }
  /* try to find a path */
  while(cert_cpv__pt->abort_cert_path_finding__vb == FLEA_FALSE)
  {
    flea_x509_cert_info_t* subject;
    flea_al_u16_t issuer_pos;
    flea_bool_t failed_path = FLEA_FALSE;
    subject = &cert_collection__bt[chain__bu16[*chain_pos__pu16]];

    if(is_cert_trusted(subject))
    {
      flea_err_e validation_error = THR_validate_cert_path(
        cert_cpv__pt,
        time_mbn__pt,
        key_to_construct_mbn__pt,
        cert_cpv__pt->cert_ver_flags__e
        );
      if(validation_error == FLEA_ERR_FINE)
      {
        return FLEA_ERR_FINE;
      }
      failed_path = FLEA_TRUE;

      if((*chain_pos__pu16) == 0)
      {
        // directly trusted cert not valid
        FLEA_THROW("no valid certificate path found", FLEA_ERR_CERT_PATH_NOT_FOUND);
      }
      (*chain_pos__pu16)--; // look for next candidate above me
    }
    // no trusted cert yet found. can only enlarge the chain if it has
    // capacitiy left.
    else if(*chain_pos__pu16 + 1 < FLEA_MAX_CERT_CHAIN_DEPTH)
    {
      flea_al_u16_t start_offs = chain__bu16[(*chain_pos__pu16) + 1];
      issuer_pos = find_issuer(
        &subject->cert_ref__t /* cert to which to find an issuer */,
        cert_collection__bt,
        cert_collection_size__alu16,
        start_offs /* offset where to start the search */,
        &chain__bu16[0] /* already used certs in terms of their possitions*/,
        (*chain_pos__pu16) + 1 /* number of used certs */
        );
    }
    else
    {
      issuer_pos = END_OF_COLL;
    }
    if(!failed_path && issuer_pos == END_OF_COLL)
    {
      // back up
      while(((*chain_pos__pu16) + 1 >= FLEA_MAX_CERT_CHAIN_DEPTH) ||
        (chain__bu16[(*chain_pos__pu16) + 1] >= cert_collection_size__alu16))
      {
        failed_path = FLEA_FALSE;
        if((*chain_pos__pu16) == 0)
        {
          FLEA_THROW("no valid certificate path found", FLEA_ERR_CERT_PATH_NOT_FOUND);
        }
        if(!(((*chain_pos__pu16) + 1) >= FLEA_MAX_CERT_CHAIN_DEPTH)) // causes valid test to fail
        {
          chain__bu16[(*chain_pos__pu16) + 1] = 0;
        }
        (*chain_pos__pu16)--;
      }
    }
    else if(!failed_path && (issuer_pos != END_OF_COLL)) // new issuer found
    {
      /* found a candidate.
       * add untrusted issuer to the chain.
       * capacity was already checked above*/
      (*chain_pos__pu16)++;
      chain__bu16[(*chain_pos__pu16)] = issuer_pos;
      continue;
    }

    // no issuer new issuer found (backed up or not, maybe stepped down from invalid trust anchor). try the next one at the
    // current level
    chain__bu16[(*chain_pos__pu16) + 1] += 1;
  }
  FLEA_THROW("user cancelled certfication path search", FLEA_ERR_X509_USER_CANCELLED);
  FLEA_THR_FIN_SEC_empty(
  );
} /* THR_flea_cpv_t__validate_and_create_pub_key */

flea_err_e THR_flea_cpv_t__ctor_cert(
  flea_cpv_t*                  cpv__pt,
  const flea_u8_t*             target_cert__pcu8,
  flea_al_u16_t                target_cert_len__alu16,
  flea_rev_chk_mode_e          rev_chk_mode__e,
  flea_x509_validation_flags_e cert_ver_flags__e
)
{
  FLEA_THR_BEG_FUNC();
# ifdef FLEA_HEAP_MODE
  FLEA_ALLOC_MEM_ARR(cpv__pt->chain__bu16, FLEA_MAX_CERT_CHAIN_DEPTH);
  FLEA_ALLOC_MEM_ARR(cpv__pt->crl_collection__brcu8, FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT);
  FLEA_ALLOC_MEM_ARR(cpv__pt->cert_collection__bt, FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT);
  cpv__pt->crl_collection_allocated__u16  = FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT;
  cpv__pt->cert_collection_allocated__u16 = FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT;
# else  /* ifdef FLEA_HEAP_MODE */
  cpv__pt->crl_collection_allocated__u16  = FLEA_MAX_CERT_COLLECTION_NB_CRLS;
  cpv__pt->cert_collection_allocated__u16 = FLEA_MAX_CERT_COLLECTION_SIZE;
# endif /* ifdef FLEA_HEAP_MODE */
  cpv__pt->cert_ver_flags__e = cert_ver_flags__e;
  cpv__pt->nb_crls__u16      = 0;
  cpv__pt->rev_chk_mode__e   = rev_chk_mode__e;
  cpv__pt->abort_cert_path_finding__vb = FLEA_FALSE;
  FLEA_CCALL(
    THR_flea_cpv_t__add_cert_without_trust_status(
      cpv__pt,
      target_cert__pcu8,
      target_cert_len__alu16
    )
  );
  FLEA_THR_FIN_SEC_empty();
}

void flea_cpv_t__abort_cert_path_building(flea_cpv_t* cpv__pt)
{
  cpv__pt->abort_cert_path_finding__vb = FLEA_TRUE;
}

flea_err_e THR_flea_cpv_t__add_crl(
  flea_cpv_t*      cpv__pt,
  const flea_u8_t* crl_der__pcu8,
  flea_dtl_t       crl_der_len__dtl
)
{
  FLEA_THR_BEG_FUNC();
  if(cpv__pt->nb_crls__u16 == cpv__pt->crl_collection_allocated__u16)
  {
# ifdef FLEA_HEAP_MODE
    const flea_al_u16_t entry_size = sizeof(cpv__pt->crl_collection__brcu8[0]);
    FLEA_CCALL(
      THR_flea_alloc__realloc_mem(
        (void**) &cpv__pt->crl_collection__brcu8,
        entry_size * cpv__pt->crl_collection_allocated__u16,
        entry_size * (cpv__pt->crl_collection_allocated__u16 + FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT)
      )
    );
    cpv__pt->crl_collection_allocated__u16 += FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT;
# else  /* ifdef FLEA_HEAP_MODE */
    FLEA_THROW("crl capacity exceeded", FLEA_ERR_BUFF_TOO_SMALL);
# endif /* ifdef FLEA_HEAP_MODE */
  }
  cpv__pt->crl_collection__brcu8[cpv__pt->nb_crls__u16].data__pcu8 = crl_der__pcu8;
  cpv__pt->crl_collection__brcu8[cpv__pt->nb_crls__u16].len__dtl   = crl_der_len__dtl;
  cpv__pt->nb_crls__u16++;
  FLEA_THR_FIN_SEC_empty();
}

static flea_err_e THR_flea_cpv_t__add_cert_info(
  flea_cpv_t*                  cpv__pt,
  const flea_x509_cert_info_t* cert_info__pt
)
{
  FLEA_THR_BEG_FUNC();
  if(cpv__pt->cert_collection_size__u16 == cpv__pt->cert_collection_allocated__u16)
  {
# ifdef FLEA_HEAP_MODE
    const flea_al_u16_t entry_size = sizeof(cpv__pt->cert_collection__bt[0]);
    FLEA_CCALL(
      THR_flea_alloc__realloc_mem(
        (void**) &cpv__pt->cert_collection__bt,
        entry_size * cpv__pt->cert_collection_size__u16,
        entry_size * (cpv__pt->cert_collection_size__u16 + FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT)
      )
    );
    cpv__pt->cert_collection_allocated__u16 += FLEA_CERT_AND_CRL_PREALLOC_OBJ_CNT;
# else  /* ifdef FLEA_HEAP_MODE */
    FLEA_THROW("cert collection full", FLEA_ERR_BUFF_TOO_SMALL);
# endif /* ifdef FLEA_HEAP_MODE */
  }
  cpv__pt->cert_collection__bt[cpv__pt->cert_collection_size__u16] = *cert_info__pt;
  cpv__pt->cert_collection_size__u16++;
  FLEA_THR_FIN_SEC_empty();
}

static flea_err_e THR_flea_cpv_t__add_cert(
  flea_cpv_t*      cpv__pt,
  const flea_u8_t* cert__pcu8,
  flea_al_u16_t    cert_len__alu16,
  flea_bool_t      is_trusted__b
)
{
  flea_x509_cert_info_t cert_info__t;

  FLEA_THR_BEG_FUNC();
  flea_x509_cert_info_t__INIT(&cert_info__t);
  flea_x509_cert_ref_t__INIT(&cert_info__t.cert_ref__t);
  FLEA_CCALL(THR_flea_x509_cert_ref_t__ctor(&cert_info__t.cert_ref__t, cert__pcu8, cert_len__alu16));
  cert_info__t.is_trusted__b = is_trusted__b;
  cert_info__t.ref_to_raw_der__rcu8.data__pu8 = (flea_u8_t*) cert__pcu8;
  cert_info__t.ref_to_raw_der__rcu8.len__dtl  = cert_len__alu16;
  FLEA_CCALL(THR_flea_x509_cert__get_bv_ref_to_tbs(cert__pcu8, cert_len__alu16, &cert_info__t.ref_to_tbs__rcu8));
  FLEA_CCALL(THR_flea_cpv_t__add_cert_info(cpv__pt, &cert_info__t));
  FLEA_THR_FIN_SEC(
    flea_x509_cert_ref_t__dtor(&ref__t);
  );
}

flea_err_e THR_flea_cpv_t__add_cert_without_trust_status(
  flea_cpv_t*      cpv__pt,
  const flea_u8_t* cert__pcu8,
  flea_al_u16_t    cert_len__alu16
)
{
  return THR_flea_cpv_t__add_cert(cpv__pt, cert__pcu8, cert_len__alu16, FLEA_FALSE);
}

flea_err_e THR_flea_cpv_t__add_trust_anchor_cert(
  flea_cpv_t*      cpv__pt,
  const flea_u8_t* cert__pcu8,
  flea_al_u16_t    cert_len__alu16
)
{
  return THR_flea_cpv_t__add_cert(cpv__pt, cert__pcu8, cert_len__alu16, FLEA_TRUE);
}

flea_err_e THR_flea_cert_path__validate_single_cert(
  flea_x509_cert_ref_t*  cert_ref__pt,
  flea_bool_t            is_trusted__b,
  flea_bool_t            is_target__b,
  const flea_gmt_time_t* arg_compare_time_mbn__pt
)
{
  flea_basic_constraints_t* basic_constraints__pt;
  flea_key_usage_t* key_usage__pt;
  flea_gmt_time_t compare_time__t;

  FLEA_THR_BEG_FUNC();

  if(arg_compare_time_mbn__pt)
  {
    compare_time__t = *arg_compare_time_mbn__pt;
  }
  else
  {
    FLEA_CCALL(THR_flea_lib__get_gmt_time_now(&compare_time__t));
  }

  key_usage__pt         = &cert_ref__pt->extensions__t.key_usage__t;
  basic_constraints__pt = &cert_ref__pt->extensions__t.basic_constraints__t;
  // verify validity date
  if(1 == flea_asn1_cmp_utc_time(&cert_ref__pt->not_before__t, &compare_time__t))
  {
    FLEA_THROW("certificate not yet valid", FLEA_ERR_X509_CERT_NOT_YET_VALID);
  }
  if(-1 == flea_asn1_cmp_utc_time(&cert_ref__pt->not_after__t, &compare_time__t))
  {
    FLEA_THROW("certificate not yet valid", FLEA_ERR_X509_CERT_EXPIRED);
  }


  /** flea does check the TA to be a CA **/
  if(!is_target__b)
  {
    if(!basic_constraints__pt->is_present__u8)
    {
      FLEA_THROW("basic constraints missing", FLEA_ERR_CERT_INTERMED_IS_NOT_CA_CERT);
    }
    if(!basic_constraints__pt->is_ca__b)
    {
      FLEA_THROW("basic constraints does not indicate CA", FLEA_ERR_CERT_INTERMED_IS_NOT_CA_CERT);
    }
    if(key_usage__pt->is_present__u8 &&
      !(key_usage__pt->purposes__u16 & flea_ku_key_cert_sign))
    {
      FLEA_THROW("key usage cert sign missing", FLEA_ERR_CERT_INTERMED_IS_NOT_CA_CERT);
    }
  }
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_cert_path__validate_single_cert */

#endif /* #ifdef FLEA_HAVE_ASYM_SIG */
