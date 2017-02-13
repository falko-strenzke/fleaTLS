/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "internal/common/default.h"
#include "internal/common/tls/handsh_reader.h"
#include "flea/types.h"
#include "flea/error.h"
#include "flea/error_handling.h"
#include "flea/rw_stream.h"

flea_err_t THR_flea_tls__read_handsh_hdr(
  flea_rw_stream_t* stream__pt,
  flea_u8_t*        handsh_type__pu8,
  flea_u32_t*       msg_len__pu32
)
{
  FLEA_THR_BEG_FUNC();
  flea_u8_t hdr__au8[4];
  FLEA_CCALL(THR_flea_rw_stream_t__force_read(stream__pt, hdr__au8, sizeof(hdr__au8)));
  *handsh_type__pu8 = hdr__au8[0];
  *msg_len__pu32    = (((flea_u32_t) hdr__au8[1]) << 16) | (((flea_u32_t) hdr__au8[2]) << 8)
    | (((flea_u32_t) hdr__au8[3]));
  FLEA_THR_FIN_SEC_empty();
}

flea_err_t THR_flea_tls_handsh_reader_t__ctor(
  flea_tls_handsh_reader_t* handsh_rdr__pt,
  flea_rw_stream_t*         underlying_read_stream__pt
)
{
  FLEA_THR_BEG_FUNC();

  FLEA_CCALL(
    THR_flea_tls__read_handsh_hdr(
      underlying_read_stream__pt,
      &handsh_rdr__pt->hlp__t.handshake_msg_type__u8,
      &handsh_rdr__pt->hlp__t.remaining_bytes__u32
    )
  );
  FLEA_CCALL(
    THR_flea_rw_stream_t__ctor_tls_handsh_reader(
      &handsh_rdr__pt->handshake_read_stream__t,
      &handsh_rdr__pt->hlp__t,
      underlying_read_stream__pt,
      handsh_rdr__pt->hlp__t.remaining_bytes__u32
    )
  );
  FLEA_THR_FIN_SEC_empty();
}

flea_u32_t flea_tls_handsh_reader_t__get_msg_rem_len(flea_tls_handsh_reader_t* handsh_rdr__pt)
{
  return handsh_rdr__pt->hlp__t.remaining_bytes__u32;
}

flea_rw_stream_t * flea_tls_handsh_reader_t__get_read_stream(flea_tls_handsh_reader_t* handsh_rdr__pt)
{
  return handsh_rdr__pt->hlp__t.underlying_read_stream__pt;
}
