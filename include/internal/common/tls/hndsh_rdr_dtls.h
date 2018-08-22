/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_hndsh_rdr_dtls__H_
# define _flea_hndsh_rdr_dtls__H_

# include "internal/common/default.h"
# include "internal/common/tls/handsh_reader.h"
# include "internal/common/tls/tls_hndsh_ctx.h"

# ifdef __cplusplus
extern "C" {
# endif


flea_err_e THR_flea_tls_hndsh_rdr__ctor_dtls(
  flea_tls_handsh_reader_t* handsh_rdr__pt,
  flea_dtls_hdsh_ctx_t*     dtls_ctx__pt,
//  flea_recprot_t*           rec_prot__pt,
  flea_tls_rec_cont_type_e  rec_cont_type__e
) FLEA_ATTRIB_UNUSED_RESULT;
# ifdef __cplusplus
}
# endif
#endif /* h-guard */
