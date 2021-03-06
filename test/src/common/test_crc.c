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
#include "flea/hash.h"
#include "self_test.h"
#include "flea/error_handling.h"
#include "flea/alloc.h"
#include "flea/crc.h"
#include "flea/algo_config.h"
#include <string.h>

flea_err_e THR_flea_test_crc16()
{
  FLEA_THR_BEG_FUNC();
  flea_u16_t crc_init_value__u16 = 0;
  flea_u16_t exp_res__u16        = 0xC965; // APPROVED BY 2 OTHER CALCULATORS
  flea_u16_t crc_res__u16;
  flea_u8_t test_string__au8[] = {0xAB, 0xCD};
  crc_res__u16 = flea_crc16_ccit_compute(crc_init_value__u16, test_string__au8, sizeof(test_string__au8));

  if(crc_res__u16 != exp_res__u16)
  {
    FLEA_THROW("wrong CRC16 result", FLEA_ERR_FAILED_TEST);
  }
  crc_res__u16 = flea_crc16_ccit_compute(crc_init_value__u16, &test_string__au8[0], 1);
  crc_res__u16 = flea_crc16_ccit_compute(crc_res__u16, &test_string__au8[1], 1);
  if(crc_res__u16 != exp_res__u16)
  {
    FLEA_THROW("wrong CRC16 result", FLEA_ERR_FAILED_TEST);
  }
  FLEA_THR_FIN_SEC_empty();
}
