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
#include "internal/common/ber_dec.h"
#include "flea/x509.h"
#include "internal/common/x509_int.h"
#include "flea/alloc.h"
#include "flea/array_util.h"
#include "flea/asn1_date.h"
#include "internal/common/namespace_asn1.h"
#include "flea/mem_read_stream.h"
#include <string.h>


const flea_u8_t id_pe__cau8 [7] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01};

flea_err_e THR_flea_x509_cert_parse_basic_constraints(
  flea_bdec_t*              cont_dec__pt,
  flea_basic_constraints_t* basic_constraints__pt
)
{
  flea_u32_t x__u32;
  flea_bool_t found__b;

  FLEA_THR_BEG_FUNC();
  basic_constraints__pt->is_present__u8 = FLEA_TRUE;
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(cont_dec__pt));
  FLEA_CCALL(THR_flea_bdec_t__dec_bool_def_false(cont_dec__pt, &basic_constraints__pt->is_ca__b));

  FLEA_CCALL(THR_flea_bdec_t__dec_optl_int_u32(cont_dec__pt, FLEA_ASN1_INT, &x__u32, &found__b));
  if(found__b)
  {
    if(x__u32 > 0xFFFE)
    {
      FLEA_THROW("pathlen of more than 0xFFFE not supported", FLEA_ERR_CERT_PATH_LEN_CONSTR_EXCEEDED);
    }
    basic_constraints__pt->path_len__u16   = x__u32;
    basic_constraints__pt->has_path_len__b = FLEA_TRUE;
  }
  else
  {
    basic_constraints__pt->path_len__u16   = 0xFFFF;
    basic_constraints__pt->has_path_len__b = FLEA_FALSE;
  }

  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(cont_dec__pt));
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_x509_cert__parse_eku(
  flea_bdec_t*      cont_dec__pt,
  flea_key_usage_t* ext_key_usage__pt
)
{
  const flea_u8_t id_kp__cau8 [] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03};

  FLEA_DECL_flea_byte_vec_t__CONSTR_HEAP_ALLOCATABLE_OR_STACK(oid__t, sizeof(id_kp__cau8) + 1);
  flea_u16_t purposes__u16 = 0;

  ext_key_usage__pt->is_present__u8 = FLEA_TRUE;
  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(cont_dec__pt));

  // seq of oids
  while(flea_bdec_t__has_current_more_data(cont_dec__pt))
  {
    FLEA_CCALL(THR_flea_bdec_t__rd_val(cont_dec__pt, FLEA_ASN1_OID, 0, &oid__t));
    if((oid__t.len__dtl != sizeof(id_kp__cau8) + 1) ||
      memcmp(oid__t.data__pu8, id_kp__cau8, sizeof(id_kp__cau8)) ||
      (oid__t.data__pu8[sizeof(id_kp__cau8)] > 15))
    {
      FLEA_THROW("unknown extended key usage purpose", FLEA_ERR_X509_EKU_VAL_ERR);
    }
    purposes__u16 |= (1 << oid__t.data__pu8[sizeof(id_kp__cau8)]);
  }
  if(purposes__u16 & (flea_u16_t) ~(
      (1 << FLEA_ASN1_EKU_BITP_any_ext_ku)
      | (1 << FLEA_ASN1_EKU_BITP_server_auth)
      | (1 << FLEA_ASN1_EKU_BITP_client_auth)
      | (1 << FLEA_ASN1_EKU_BITP_code_signing)
      | (1 << FLEA_ASN1_EKU_BITP_email_protection)
      | (1 << FLEA_ASN1_EKU_BITP_time_stamping)
      | (1 << FLEA_ASN1_EKU_BITP_ocsp_signing)))
  {
    FLEA_THROW("unknown extended key usage purpose", FLEA_ERR_X509_EKU_VAL_ERR);
  }

  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(cont_dec__pt));
  ext_key_usage__pt->purposes__u16 = purposes__u16;
  FLEA_THR_FIN_SEC(
    flea_byte_vec_t__dtor(&oid__t);
  );
} /* THR_flea_x509_cert__parse_eku */

flea_err_e THR_flea_x509_cert__parse_key_usage(
  flea_bdec_t*      cont_dec__pt,
  flea_key_usage_t* key_usage__pt
)
{
  FLEA_DECL_byte_vec_t__CONSTR_STACK_BUF_EMPTY_NOT_ALLOCATABLE(bit_str__t, 3);
  flea_u16_t ku__u16;

  FLEA_THR_BEG_FUNC();
  key_usage__pt->is_present__u8 = FLEA_TRUE;
  FLEA_CCALL(
    THR_flea_bdec_t__rd_val_cft(
      cont_dec__pt,
      FLEA_ASN1_CFT_MAKE2(UNIVERSAL_PRIMITIVE, FLEA_ASN1_BIT_STRING),
      &bit_str__t
    )
  );
  if(bit_str__t.len__dtl < 2)
  {
    FLEA_THROW("empty key usage value", FLEA_ERR_ASN1_DER_DEC_ERR);
  }
  ku__u16 = bit_str__t.data__pu8[1] << 8;
  if(bit_str__t.len__dtl > 2)
  {
    /* unused implicitly set to zero -- in DER unused must be encoded as zero */
    ku__u16 |= bit_str__t.data__pu8[2];
  }
  key_usage__pt->purposes__u16 = ku__u16;
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_x509__dec_algid_ref(
  flea_x509_algid_ref_t* algid_ref__pt,
  flea_bdec_t*           dec__pt
)
{
  FLEA_THR_BEG_FUNC();
  flea_bool_t optional_found__b = FLEA_TRUE;
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(dec__pt));
  FLEA_CCALL(
    THR_flea_bdec_t__dec_cft(
      dec__pt,
      FLEA_ASN1_CFT_MAKE2(UNIVERSAL_PRIMITIVE, OID),
      &algid_ref__pt->oid_ref__t
    )
  );
  FLEA_CCALL(
    THR_flea_bdec_t__dec_tlv_raw_optional(
      dec__pt,
      &algid_ref__pt->params_ref_as_tlv__t,
      &optional_found__b
    )
  );
  if(!optional_found__b)
  {
    algid_ref__pt->params_ref_as_tlv__t.len__dtl = 0;
  }
  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(dec__pt));
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_x509__process_alg_ids(
  flea_x509_algid_ref_t*       tbs_ref__pt,
  const flea_x509_algid_ref_t* outer_ref__pt
)
{
  FLEA_THR_BEG_FUNC();

  if(!flea_ber_dec__are_der_refs_equal(&outer_ref__pt->oid_ref__t, &tbs_ref__pt->oid_ref__t))
  {
    FLEA_THROW("the two signature algorithm identifiers in the certificate do not match", FLEA_ERR_X509_SIG_ALG_ERR);
  }
  FLEA_THR_FIN_SEC_empty();
}

/*
 * id-pkix  OBJECT IDENTIFIER  ::=
 * { iso(1) identified-organization(3) dod(6) internet(1)
 * security(5) mechanisms(5) pkix(7) }
 *
 * id-pe  OBJECT IDENTIFIER  ::=  { id-pkix 1 }
 */
static flea_err_e THR_flea_x509_cert_ref__t__parse_extensions(
  flea_x509_ext_ref_t* ext_ref__pt,
  flea_bdec_t*         dec__pt,
  flea_bool_t*         have_extensions__pb
)
{
  flea_byte_vec_t ext_oid_ref__t = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;
  flea_bool_t critical__b;

  flea_bdec_t cont_dec__t;
  flea_rw_stream_t source__t;

  FLEA_THR_BEG_FUNC();
  flea_bdec_t__INIT(&cont_dec__t);
  flea_rw_stream_t__INIT(&source__t);
  /* open implicit */
  FLEA_CCALL(
    THR_flea_bdec_t__op_cons_optional(
      dec__pt,
      3,
      FLEA_ASN1_CONSTRUCTED | FLEA_ASN1_CONTEXT_SPECIFIC,
      have_extensions__pb
    )
  );
  if(!*have_extensions__pb)
  {
    FLEA_THR_RETURN();
  }
  /* open extensions */
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(dec__pt));
  while(flea_bdec_t__has_current_more_data(dec__pt))
  {
    flea_al_u8_t ext_indic_pos__alu8;
    flea_byte_vec_t ostr__t = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;
    flea_al_u16_t oid_indicator__alu16 = 0;
    flea_mem_read_stream_help_t hlp__t;
    /* open this extension */
    FLEA_CCALL(THR_flea_bdec_t__open_sequence(dec__pt));
    FLEA_CCALL(THR_flea_bdec_t__get_der_ref_to_oid(dec__pt, &ext_oid_ref__t));
    FLEA_CCALL(THR_flea_bdec_t__dec_bool_def_false(dec__pt, &critical__b));

    /* decode the extension value in the octet string */
    FLEA_CCALL(
      THR_flea_bdec_t__get_r_raw_cft(
        dec__pt,
        FLEA_ASN1_CFT_MAKE2(FLEA_ASN1_UNIVERSAL_PRIMITIVE, FLEA_ASN1_OCTET_STRING),
        &ostr__t
      )
    );

    /* open 'octet string' sequence */
    if(ext_oid_ref__t.len__dtl == 3 && ext_oid_ref__t.data__pu8[0] == 0x55 && ext_oid_ref__t.data__pu8[1] == 0x1D)
    {
      oid_indicator__alu16  = ID_CE_INDIC;
      ext_indic_pos__alu8   = 2;
      oid_indicator__alu16 |= ext_oid_ref__t.data__pu8[ext_indic_pos__alu8];
    }
    else if((ext_oid_ref__t.len__dtl == sizeof(id_pe__cau8) + 1) &&
      (!memcmp(ext_oid_ref__t.data__pu8, id_pe__cau8, sizeof(id_pe__cau8))))
    {
      oid_indicator__alu16  = ID_PE_INDIC;
      ext_indic_pos__alu8   = sizeof(id_pe__cau8);
      oid_indicator__alu16 |= ext_oid_ref__t.data__pu8[ext_indic_pos__alu8];
    }
    else
    {
      if(critical__b)
      {
        FLEA_THROW("unsupported critical extension", FLEA_ERR_X509_ERR_UNSUP_CRIT_EXT);
      }
      oid_indicator__alu16 = ID_UNSUPP_EXT_OID;
    }
    /* standard extension */
    FLEA_CCALL(THR_flea_rw_stream_t__ctor_memory(&source__t, ostr__t.data__pu8, ostr__t.len__dtl, &hlp__t));
    FLEA_CCALL(THR_flea_bdec_t__ctor(&cont_dec__t, &source__t, 0, flea_dec_ref));
    switch(oid_indicator__alu16)
    {
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
      flea_bool_t found__b;
#endif
        case ID_CE_OID_AKI:
        {
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
          /* authority key identifier */
          ext_ref__pt->auth_key_id__t.is_present__u8 = FLEA_TRUE;

          FLEA_CCALL(THR_flea_bdec_t__open_sequence(&cont_dec__t));
          FLEA_CCALL(
            THR_flea_bdec_t__get_r_cft_raw_optional(
              &cont_dec__t,
              (flea_asn1_tag_t) FLEA_ASN1_CFT_MAKE2(FLEA_ASN1_CONTEXT_SPECIFIC, 0),
              &ext_ref__pt->auth_key_id__t.key_id__t,
              &found__b
            )
          );
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */

          break;
        }
        case ID_CE_OID_KEY_USAGE:
        {
          FLEA_CCALL(THR_flea_x509_cert__parse_key_usage(&cont_dec__t, &ext_ref__pt->key_usage__t));
          break;
        }
        case ID_CE_OID_SUBJ_KEY_ID:
        {
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
          FLEA_CCALL(
            THR_flea_bdec_t__get_r_raw_cft(
              &cont_dec__t,
              FLEA_ASN1_OCTET_STRING,
              &ext_ref__pt->subj_key_id__t
            )
          );
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
          break;
        }
        case ID_CE_OID_SUBJ_ALT_NAME:
        {
          ext_ref__pt->san__t.is_present__u8 = FLEA_TRUE;
          FLEA_CCALL(THR_flea_bdec_t__get_r_next_tlv_raw(&cont_dec__t, &ext_ref__pt->san__t.san_raw__t));
          break;
        }

        case ID_CE_OID_ISS_ALT_NAME:
        {
          // nothing to do, flea does not process it
          break;
        }
        case ID_CE_OID_POLICIES:
        {
          // nothing to do, flea does not process it
          break;
        }
        case ID_CE_OID_BASIC_CONSTR:
        {
          FLEA_CCALL(THR_flea_x509_cert_parse_basic_constraints(&cont_dec__t, &ext_ref__pt->basic_constraints__t));
          break;
        }
        case ID_CE_OID_EXT_KEY_USAGE:
        {
          FLEA_CCALL(THR_flea_x509_cert__parse_eku(&cont_dec__t, &ext_ref__pt->ext_key_usage__t));
          break;
        }
        case ID_CE_OID_CRL_DISTR_POINT:
        {
          ext_ref__pt->crl_distr_point__t.is_present__u8 = FLEA_TRUE;
          FLEA_CCALL(
            THR_flea_bdec_t__get_r_next_tlv_raw(
              &cont_dec__t,
              &ext_ref__pt->crl_distr_point__t.raw_ref__t
            )
          );
          break;
        }
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
        case ID_CE_OID_FRESHEST_CRL:
        {
          ext_ref__pt->freshest_crl__t.is_present__u8 = FLEA_TRUE;
          FLEA_CCALL(
            THR_flea_bdec_t__get_r_next_tlv_raw(
              &cont_dec__t,
              &ext_ref__pt->freshest_crl__t.raw_ref__t
            )
          );
          break;
        }
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
        case ID_PE_OID_AUTH_INF_ACC:
        {
          ext_ref__pt->auth_inf_acc__t.is_present__u8 = FLEA_TRUE;
          FLEA_CCALL(
            THR_flea_bdec_t__get_r_next_tlv_raw(
              &cont_dec__t,
              &ext_ref__pt->auth_inf_acc__t.raw_ref__t
            )
          );
          break;
        }
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
        default:
          if(critical__b)
          {
            FLEA_THROW("unsupported critical extension", FLEA_ERR_X509_ERR_UNSUP_CRIT_EXT);
          }
    }


    flea_bdec_t__dtor(&cont_dec__t);
    flea_rw_stream_t__dtor(&source__t);
    /* close extension sequence */
    FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(dec__pt));
  } /* while(flea_bdec_t__has_current_more_data(dec__pt)) */

  /* close extensions sequence*/
  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(dec__pt));

  /* close implicit */
  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(dec__pt));
  FLEA_THR_FIN_SEC(
    flea_bdec_t__dtor(&cont_dec__t);
    flea_rw_stream_t__dtor(&source__t);
  );
} /* THR_flea_x509_cert_ref__t__parse_extensions */

flea_err_e THR_flea_x509__parse_dn_ref(
  flea_x509_dn_ref_t* dn_ref__pt,
  flea_bdec_t*        dec__pt
)
{
  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(THR_flea_bdec_t__get_r_next_tlv_raw(dec__pt, &dn_ref__pt->raw_dn_complete__t));
  FLEA_CCALL(
    THR_flea_x509__dec_dn_ref_elements(
      dn_ref__pt,
      dn_ref__pt->raw_dn_complete__t.data__pu8,
      dn_ref__pt->raw_dn_complete__t.len__dtl,
      FLEA_TRUE
    )
  );
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_x509__dec_dn_ref_elements(
  flea_x509_dn_ref_t* dn_ref__pt,
  const flea_u8_t*    data__pcu8,
  flea_dtl_t          data_len__dtl,
  flea_bool_t         with_outer_seq__b
)
{
  flea_rw_stream_t source__t;
  flea_bdec_t dec__t;
  flea_mem_read_stream_help_t hlp__t;

  FLEA_THR_BEG_FUNC();
  flea_rw_stream_t__INIT(&source__t);
  flea_bdec_t__INIT(&dec__t);
  FLEA_CCALL(
    THR_flea_rw_stream_t__ctor_memory(
      &source__t,
      data__pcu8,
      data_len__dtl,
      &hlp__t
    )
  );
  FLEA_CCALL(THR_flea_bdec_t__ctor(&dec__t, &source__t, 0, flea_dec_ref));
  if(with_outer_seq__b)
  {
    FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));
  }
  while(flea_bdec_t__has_current_more_data(&dec__t))
  {
    flea_byte_vec_t* entry_ref__pt = NULL;
    flea_byte_vec_t entry_ref__t   = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;
    flea_asn1_str_type_t str_type__t;
    FLEA_CCALL(THR_flea_bdec_t__open_set(&dec__t));
    FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));
    FLEA_CCALL(THR_flea_bdec_t__get_der_ref_to_oid(&dec__t, &entry_ref__t));
    if(entry_ref__t.len__dtl != 3 ||
      entry_ref__t.data__pu8[0] != FLEA_ASN1_OID_FIRST_BYTE(2, 5) || entry_ref__t.data__pu8[1] != 4)
    {
      FLEA_THROW("invalid oid for distinguished name component", FLEA_ERR_X509_DN_ERROR);
    }
    switch(entry_ref__t.data__pu8[2])
    {
        case 3:
          entry_ref__pt = &dn_ref__pt->common_name__t;
          break;
        case 6:
          entry_ref__pt = &dn_ref__pt->country__t;
          break;
        case 10:
          entry_ref__pt = &dn_ref__pt->org__t;
          break;
        case 11:
          entry_ref__pt = &dn_ref__pt->org_unit__t;
          break;
#ifdef FLEA_HAVE_X509_DN_DETAILS
        case 46:
          entry_ref__pt = &dn_ref__pt->dn_qual__t;
          break;
        case 8:
          entry_ref__pt = &dn_ref__pt->state_or_province_name__t;
          break;
        case 5:
          entry_ref__pt = &dn_ref__pt->serial_number__t;
          break;
        case 7:
          entry_ref__pt = &dn_ref__pt->locality_name__t;
          break;
        default:
          FLEA_THROW("unsupported distinguished name component", FLEA_ERR_X509_DN_ERROR);
#else /* ifdef FLEA_HAVE_X509_DN_DETAILS */
        default:
          /* skip it */
          entry_ref__pt = &entry_ref__t;
#endif /* ifdef FLEA_HAVE_X509_DN_DETAILS */
    }
    if(entry_ref__pt == NULL)
    {
      FLEA_THROW("unknown component in distinguished name", FLEA_ERR_X509_DN_ERROR);
    }
    FLEA_CCALL(
      THR_flea_bdec_t__get_r_string(
        &dec__t,
        &str_type__t,
        entry_ref__pt
      )
    );
    // close the sequence
    FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));
    // close the set -- multivalued RDNs are not supported
    FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));
  }

  if(with_outer_seq__b)
  {
    FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));
  }
  FLEA_THR_FIN_SEC(
    flea_rw_stream_t__dtor(&source__t);
    flea_bdec_t__dtor(&dec__t);
  );
} /* THR_flea_x509__parse_dn_ref */

flea_err_e THR_flea_x509_cert__get_bv_ref_to_tbs(
  const flea_u8_t* der_encoded_cert__pu8,
  flea_al_u16_t    der_encoded_cert_len__alu16,
  flea_byte_vec_t* ref_to_tbs__pt
)
{
  flea_ref_cu8_t ref__rcu8;

  FLEA_THR_BEG_FUNC();
  FLEA_CCALL(THR_flea_x509_cert__get_ref_to_tbs(der_encoded_cert__pu8, der_encoded_cert_len__alu16, &ref__rcu8));
  flea_byte_vec_t__set_as_ref(ref_to_tbs__pt, ref__rcu8.data__pcu8, ref__rcu8.len__dtl);
  FLEA_THR_FIN_SEC_empty();
}

flea_err_e THR_flea_x509_cert__get_ref_to_tbs(
  const flea_u8_t* der_encoded_cert__pu8,
  flea_al_u16_t    der_encoded_cert_len__alu16,
  flea_ref_cu8_t*  ref_to_tbs__pt
)
{
  flea_rw_stream_t source_tbs__t;
  flea_bdec_t dec_tbs__t;
  flea_mem_read_stream_help_t hlp_tbs__t;
  flea_byte_vec_t intermed_ref = flea_byte_vec_t__CONSTR_ZERO_CAPACITY_NOT_ALLOCATABLE;

  FLEA_THR_BEG_FUNC();
  flea_rw_stream_t__INIT(&source_tbs__t);
  flea_bdec_t__INIT(&dec_tbs__t);

  FLEA_CCALL(
    THR_flea_rw_stream_t__ctor_memory(
      &source_tbs__t,
      der_encoded_cert__pu8,
      der_encoded_cert_len__alu16,
      &hlp_tbs__t
    )
  );

  FLEA_CCALL(THR_flea_bdec_t__ctor(&dec_tbs__t, &source_tbs__t, 0, flea_dec_ref));
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec_tbs__t));
  FLEA_CCALL(THR_flea_bdec_t__get_r_next_tlv_raw(&dec_tbs__t, &intermed_ref));
  ref_to_tbs__pt->data__pcu8 = intermed_ref.data__pu8;
  ref_to_tbs__pt->len__dtl   = intermed_ref.len__dtl;
  FLEA_THR_FIN_SEC(
    flea_rw_stream_t__dtor(&source_tbs__t);
    flea_bdec_t__dtor(&dec_tbs__t);
  );
}

flea_err_e THR_flea_x509_cert_ref_t__ctor(
  flea_x509_cert_ref_t* cert_ref__pt,
  const flea_u8_t*      der_encoded_cert__pu8,
  flea_al_u16_t         der_encoded_cert_len__alu16
)
{
  flea_rw_stream_t source__t;
  flea_bdec_t dec__t;
  flea_mem_read_stream_help_t hlp__t;
  flea_bool_t found_tag__b;
  flea_x509_algid_ref_t outer_sig_algid__t = flea_x509_algid_ref_t__CONSTR_EMPTY_ALLOCATABLE;

  FLEA_DECL_byte_vec_t__CONSTR_STACK_BUF_EMPTY_NOT_ALLOCATABLE(version_vec__t, 1);
  FLEA_THR_BEG_FUNC();
  flea_rw_stream_t__INIT(&source__t);
  flea_bdec_t__INIT(&dec__t);

  FLEA_CCALL(
    THR_flea_rw_stream_t__ctor_memory(
      &source__t,
      der_encoded_cert__pu8,
      der_encoded_cert_len__alu16,
      &hlp__t
    )
  );
  FLEA_CCALL(THR_flea_bdec_t__ctor(&dec__t, &source__t, 0, flea_dec_ref));

  FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));
  FLEA_CCALL(
    THR_flea_bdec_t__op_cons_optional(
      &dec__t,
      0,
      FLEA_ASN1_CONSTRUCTED | FLEA_ASN1_CONTEXT_SPECIFIC,
      &found_tag__b
    )
  );
  if(found_tag__b)
  {
    FLEA_CCALL(THR_flea_bdec_t__rd_val(&dec__t, FLEA_ASN1_INT, 0, &version_vec__t));
    if(version_vec__t.len__dtl != 1)
    {
      FLEA_THROW("x.509 version of invalid length", FLEA_ERR_X509_VERSION_ERROR);
    }
    cert_ref__pt->version__u8 = version_vec__t.data__pu8[0] + 1;
    FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));
  }
  else
  {
    cert_ref__pt->version__u8 = 1;
  }
  FLEA_CCALL(THR_flea_bdec_t__dec_int(&dec__t, &cert_ref__pt->serial_number__t));


  FLEA_CCALL(THR_flea_x509__dec_algid_ref(&cert_ref__pt->tbs_sig_algid__t, &dec__t));
  FLEA_CCALL(THR_flea_x509__parse_dn_ref(&cert_ref__pt->issuer__t, &dec__t));
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));

  FLEA_CCALL(THR_flea_asn1_parse_gmt_time(&dec__t, &cert_ref__pt->not_before__t));
  FLEA_CCALL(THR_flea_asn1_parse_gmt_time(&dec__t, &cert_ref__pt->not_after__t));
  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));

  FLEA_CCALL(THR_flea_x509__parse_dn_ref(&cert_ref__pt->subject__t, &dec__t));

  /* enter subject public key info */
  FLEA_CCALL(THR_flea_bdec_t__open_sequence(&dec__t));
  FLEA_CCALL(THR_flea_x509__dec_algid_ref(&cert_ref__pt->subject_public_key_info__t.algid__t, &dec__t));

  FLEA_CCALL(
    THR_flea_bdec_t__get_r_next_tlv_raw(
      &dec__t,
      &cert_ref__pt->subject_public_key_info__t.public_key_as_tlv__t
    )
  );

  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));

#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
  FLEA_CCALL(
    THR_flea_bdec_t__dec_optl_with_inner_impl_univ(
      &dec__t,
      1,
      FLEA_ASN1_BIT_STRING,
      &cert_ref__pt->issuer_unique_id_as_bitstr__t
    )
  );
#else  /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
  FLEA_CCALL(
    THR_flea_bdec_t__dec_optl_with_inner_impl_univ(
      &dec__t,
      1,
      FLEA_ASN1_BIT_STRING,
      &cert_ref__pt->cert_signature_as_bit_string__t
    )
  );
  cert_ref__pt->cert_signature_as_bit_string__t.len__dtl = 0;
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
#ifdef FLEA_X509_CERT_REF_WITH_DETAILS
  FLEA_CCALL(
    THR_flea_bdec_t__dec_optl_with_inner_impl_univ(
      &dec__t,
      2,
      FLEA_ASN1_BIT_STRING,
      &cert_ref__pt->subject_unique_id_as_bitstr__t
    )
  );
#else  /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
  FLEA_CCALL(
    THR_flea_bdec_t__dec_optl_with_inner_impl_univ(
      &dec__t,
      2,
      FLEA_ASN1_BIT_STRING,
      &cert_ref__pt->cert_signature_as_bit_string__t
    )
  );
  cert_ref__pt->cert_signature_as_bit_string__t.len__dtl = 0;
#endif /* ifdef FLEA_X509_CERT_REF_WITH_DETAILS */
  FLEA_CCALL(THR_flea_x509_cert_ref__t__parse_extensions(&cert_ref__pt->extensions__t, &dec__t, &found_tag__b));
  if((found_tag__b && cert_ref__pt->version__u8 != 3) || (cert_ref__pt->version__u8 > 3)
  )
  {
    FLEA_THROW("invalid X.509 version", FLEA_ERR_X509_VERSION_ERROR);
  }

  /* closing the tbs */
  FLEA_CCALL(THR_flea_bdec_t__cl_cons_at_end(&dec__t));

  FLEA_CCALL(THR_flea_x509__dec_algid_ref(&outer_sig_algid__t, &dec__t));
  FLEA_CCALL(THR_flea_x509__process_alg_ids(&cert_ref__pt->tbs_sig_algid__t, &outer_sig_algid__t));
  FLEA_CCALL(
    THR_flea_bdec_t__get_r_raw_cft(
      &dec__t,
      FLEA_ASN1_CFT_MAKE2(UNIVERSAL_PRIMITIVE, BIT_STRING),
      &cert_ref__pt->cert_signature_as_bit_string__t
    )
  );
  FLEA_THR_FIN_SEC(
    flea_rw_stream_t__dtor(&source__t);
    flea_bdec_t__dtor(&dec__t);
  );
} /* THR_flea_x509_cert_ref_t__ctor */

flea_bool_t flea_x509_has_key_usages(
  flea_key_usage_t const*      key_usage__pt,
  flea_key_usage_e             required_usages__u16,
  flea_key_usage_exlicitness_e explicitness__e
)
{
  flea_al_u16_t ku_val__alu16;
  flea_al_u8_t ku_present_alu8;

  ku_val__alu16   = key_usage__pt->purposes__u16;
  ku_present_alu8 = key_usage__pt->is_present__u8;
  if(!ku_present_alu8)
  {
    if(explicitness__e == flea_key_usage_explicit)
    {
      return FLEA_FALSE;
    }
  }
  if(ku_present_alu8 && ((ku_val__alu16 & required_usages__u16) != required_usages__u16))
  {
    return FLEA_FALSE;
  }
  return FLEA_TRUE;
}

flea_bool_t flea_x509_has_extended_key_usages(
  flea_key_usage_t const*      key_usage__pt,
  flea_key_usage_e             required_usages__u16,
  flea_key_usage_exlicitness_e explicitness__e
)
{
  flea_al_u16_t ku_val__alu16;
  flea_al_u8_t ku_present_alu8;

  ku_val__alu16   = key_usage__pt->purposes__u16;
  ku_present_alu8 = key_usage__pt->is_present__u8;
  if(!ku_present_alu8)
  {
    if(explicitness__e == flea_key_usage_explicit)
    {
      return FLEA_FALSE;
    }
  }
  if(ku_present_alu8 && (((ku_val__alu16 & required_usages__u16) != required_usages__u16) &&
    ((ku_val__alu16 & required_usages__u16) != flea_eku_any_ext_ku))
  )
  {
    return FLEA_FALSE;
  }
  return FLEA_TRUE;
}

flea_bool_t flea_x509_cert_ref_t__has_key_usages(
  flea_x509_cert_ref_t const*  cert_ref__pt,
  flea_key_usage_e             required_usages__u16,
  flea_key_usage_exlicitness_e explicitness__e
)
{
  return flea_x509_has_key_usages(&cert_ref__pt->extensions__t.key_usage__t, required_usages__u16, explicitness__e);
}

flea_bool_t flea_x509_cert_ref_t__has_extended_key_usages(
  flea_x509_cert_ref_t const*  cert_ref__pt,
  flea_ext_key_usage_e         required_usages__u16,
  flea_key_usage_exlicitness_e explicitness__e
)
{
  return flea_x509_has_extended_key_usages(
    &cert_ref__pt->extensions__t.ext_key_usage__t,
    required_usages__u16,
    explicitness__e
  );
}

flea_bool_t flea_x509_is_cert_self_issued(const flea_x509_cert_ref_t* cert__pt)
{
  if(FLEA_DER_REF_IS_ABSENT(&cert__pt->issuer__t.raw_dn_complete__t))
  {
    return FLEA_TRUE;
  }
  if(0 == flea_byte_vec_t__cmp(&cert__pt->subject__t.raw_dn_complete__t, &cert__pt->issuer__t.raw_dn_complete__t))
  {
    return FLEA_TRUE;
  }
  return FLEA_FALSE;
}

static flea_err_e THR_flea_x509__get_dn_component(
  flea_x509_dn_ref_t const* dn_ref__pt,
  flea_dn_cmpnt_e           cmpnt__e,
  flea_ref_cu8_t*           result__prcu8
)
{
  const flea_byte_vec_t* bv__pt;

  FLEA_THR_BEG_FUNC();

  switch(cmpnt__e)
  {
      case flea_dn_cmpnt_cn:
      {
        bv__pt = &dn_ref__pt->common_name__t;
        break;
      }
      case flea_dn_cmpnt_country:
      {
        bv__pt = &dn_ref__pt->country__t;
        break;
      }
      case flea_dn_cmpnt_org:
      {
        bv__pt = &dn_ref__pt->org__t;
        break;
      }
      case flea_dn_cmpnt_org_unit:
      {
        bv__pt = &dn_ref__pt->org_unit__t;
        break;
      }
#ifdef FLEA_HAVE_X509_DN_DETAILS
      case flea_dn_cmpnt_dn_qual:
      {
        bv__pt = &dn_ref__pt->dn_qual__t;
        break;
      }
      case flea_dn_cmpnt_locality_name:
      {
        bv__pt = &dn_ref__pt->locality_name__t;
        break;
      }
      case flea_dn_cmpnt_state_or_province:
      {
        bv__pt = &dn_ref__pt->state_or_province_name__t;
        break;
      }
      case flea_dn_cmpnt_serial_number:
      {
        bv__pt = &dn_ref__pt->serial_number__t;
        break;
      }
      case flea_dn_cmpnt_domain_cmpnt_attrib:
      {
        bv__pt = &dn_ref__pt->domain_component_attribute__t;
        break;
      }
#endif /* ifdef FLEA_HAVE_X509_DN_DETAILS */
      default:
      {
        FLEA_THROW("invalid DN component specified", FLEA_ERR_INV_ARG);
      }
  }
  result__prcu8->data__pcu8 = bv__pt->data__pu8;
  result__prcu8->len__dtl   = bv__pt->len__dtl;
  FLEA_THR_FIN_SEC_empty();
} /* THR_flea_x509__get_dn_component */

flea_err_e THR_flea_x509_cert_ref_t__get_subject_dn_component(
  flea_x509_cert_ref_t const* cert_ref__pt,
  flea_dn_cmpnt_e             cmpnt__e,
  flea_ref_cu8_t*             result__prcu8
)
{
  return THR_flea_x509__get_dn_component(&cert_ref__pt->subject__t, cmpnt__e, result__prcu8);
}

flea_err_e THR_flea_x509_cert_ref_t__get_issuer_dn_component(
  flea_x509_cert_ref_t const* cert_ref__pt,
  flea_dn_cmpnt_e             cmpnt__e,
  flea_ref_cu8_t*             result__prcu8
)
{
  return THR_flea_x509__get_dn_component(&cert_ref__pt->issuer__t, cmpnt__e, result__prcu8);
}

const flea_gmt_time_t* flea_x509_cert_ref_t__get_not_after_ref(const flea_x509_cert_ref_t* cert_ref__pt)
{
  return &cert_ref__pt->not_after__t;
}

const flea_gmt_time_t* flea_x509_cert_ref_t__get_not_before_ref(const flea_x509_cert_ref_t* cert_ref__pt)
{
  return &cert_ref__pt->not_before__t;
}
