/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#ifndef _flea_hndsh_rdr_tls__H_
# define _flea_hndsh_rdr_tls__H_

# include "internal/common/default.h"
# include "internal/common/tls/handsh_reader.h"

# ifdef __cplusplus
extern "C" {
# endif


flea_err_e THR_flea_tls_hndsh_rdr__ctor_tls(
  flea_tls_handsh_reader_t* handsh_rdr__pt,
  flea_recprot_t*           rec_prot__pt
) FLEA_ATTRIB_UNUSED_RESULT;


# ifdef __cplusplus
}
# endif
#endif /* h-guard */
