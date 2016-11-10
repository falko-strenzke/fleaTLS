/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "self_test.h"
#include <stdio.h>
#include <iostream>
#include "pc/test_util.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h> // Linux specific

int main (int argc, const char** argv)
{

  property_set_t cmdl_args(argc, argv);
  flea_u32_t reps = 1;
  flea_u32_t rnd = 0;
  const char* cert_path_prefix = NULL;
  std::string cert_path_prefix_str;
  if(cmdl_args.have_index("cert_path_prefix"))
  {
    cert_path_prefix_str = cmdl_args.get_property_as_string("cert_path_prefix");
    cert_path_prefix = cert_path_prefix_str.c_str();
  }
  if(cmdl_args.have_index("random"))
    {
      printf("flea test: running randomized tests\n");
      struct timeval tv;
      gettimeofday(&tv, NULL);
      rnd = (tv.tv_sec * tv.tv_usec) ^ tv.tv_sec ^ tv.tv_usec;
      printf("rnd = %u\n", rnd);
    }
  reps = cmdl_args.get_property_as_u32_default("repeat", 1);
  
    /*std::cerr << "DEBUG EXIT" << std::endl;
    exit(1);*/
    
    //=====
/*    if(reps != 1)
    {
std::cerr << "DEBUG EXIT" << std::endl;
    exit(1);
    }*/
    //=======
  

 return flea_unit_tests(rnd, reps, cert_path_prefix);
}
