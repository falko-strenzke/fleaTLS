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
#include "flea/error.h"
#include "flea/alloc.h"
#include <string.h>
#include "flea/mem_read_stream.h"
#include "self_test.h"

flea_err_e THR_flea_test_rw_stream_init_dtor()
{
  flea_rw_stream_t source2__t;

  FLEA_THR_BEG_FUNC();
  flea_rw_stream_t__INIT(&source2__t);

  FLEA_THR_FIN_SEC(
    flea_rw_stream_t__dtor(&source2__t);
  );
}

flea_err_e THR_flea_test_mem_read_stream()
{
  const flea_u8_t source_mem__au8[64] = {
    0xc3, 0x0e, 0x32, 0xff, 0xed, 0xc0, 0x77, 0x4e, 0x6a, 0xff, 0x6a, 0xf0, 0x86, 0x9f, 0x71, 0xaa,
    0x0f, 0x3a, 0xf0, 0x7a, 0x9a, 0x31, 0xa9, 0xc6, 0x84, 0xdb, 0x20, 0x7e, 0xb0, 0xef, 0x8e, 0x4e,
    0x35, 0x90, 0x7a, 0xa6, 0x32, 0xc3, 0xff, 0xdf, 0x86, 0x8b, 0xb7, 0xb2, 0x9d, 0x3d, 0x46, 0xad,
    0x83, 0xce, 0x9f, 0x9a, 0x10, 0x2e, 0xe9, 0x9d, 0x49, 0xa5, 0x3e, 0x87, 0xf4, 0xc3, 0xda, 0x55
  };

  FLEA_DECL_BUF(trg_buf__bu8, flea_u8_t, sizeof(source_mem__au8));
  flea_mem_read_stream_help_t hlp__t;
  flea_rw_stream_t source__t;
  flea_dtl_t nb_read = 60;
  FLEA_THR_BEG_FUNC();
  flea_rw_stream_t__INIT(&source__t);
  FLEA_ALLOC_BUF(trg_buf__bu8, sizeof(source_mem__au8));
  memset(trg_buf__bu8, 0, sizeof(source_mem__au8));
  FLEA_CCALL(THR_flea_rw_stream_t__ctor_memory(&source__t, source_mem__au8, sizeof(source_mem__au8), &hlp__t));
  FLEA_CCALL(THR_flea_rw_stream_t__read(&source__t, trg_buf__bu8, &nb_read, flea_read_nonblocking));
  if(nb_read != 60)
  {
    FLEA_THROW("wrong number of read bytes", FLEA_ERR_FAILED_TEST);
  }
  if(trg_buf__bu8[nb_read] != 0 || trg_buf__bu8[nb_read + 1] != 0 || trg_buf__bu8[nb_read + 2] != 0 ||
    trg_buf__bu8[nb_read + 3] != 0)
  {
    FLEA_THROW("target buffer overwrite", FLEA_ERR_FAILED_TEST);
  }
  nb_read = 10;
  FLEA_CCALL(THR_flea_rw_stream_t__read(&source__t, &trg_buf__bu8[60], &nb_read, flea_read_blocking));
  if(nb_read != 4)
  {
    FLEA_THROW("wrong number of read bytes", FLEA_ERR_FAILED_TEST);
  }
  if(memcmp(trg_buf__bu8, source_mem__au8, sizeof(source_mem__au8)))
  {
    FLEA_THROW("error with content", FLEA_ERR_FAILED_TEST);
  }
  FLEA_THR_FIN_SEC(
    flea_rw_stream_t__dtor(&source__t);
    FLEA_FREE_BUF_FINAL(trg_buf__bu8);
  );
} /* THR_flea_test_data_source_mem */
