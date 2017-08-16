# -*- coding: iso-8859-1 -*-

import re
import os
import os.path
import glob
import sys
import shutil
import subprocess

include_dir = "../../include"
src_dir = "../../src"
test_dir = "../../test"
test_data_dir = "../testdata"
build_cfg_dir = "../../build_cfg"
pltf_supp_dir = "../../pltf_support"
generate_dir = "../../../flea_generated_releases"
cmakelists_file = "../../CMakeLists.txt"

license_name_gpl = "gpl"
license_name_closed_source = "flea"

def ignore_svn_function(a, b):
  result = []
  result.append(".svn")
  result.append("stm32f4")
  result.append("*.elf")
  result.append("*.map")
  result.append("*.swp")
  result.append("*~")
  result.append("makefile")
  return result

def collect_files_with_ending(ending, dir):
  result = []
  for root, dirs, files in os.walk(dir):
    for file in files:
      if file.endswith(ending):
        #print(os.path.join(root, file))
        result.append (os.path.join(root, file))
  return result

def generate_for_license(license_name, work_dir):
  #license_file_name = license_name + ".txt"
  license_notice_file_name = "../../misc/licenses/" + license_name + ".notice"
  license_notice_text = open(license_notice_file_name, 'r').read()
  #license__text = open(license_notice_file_name, 'r').read()
  #print "work_dir = " + work_dir 
  files = collect_files_with_ending(".h", work_dir+"/include")
  files += collect_files_with_ending(".c", work_dir+"/src")
  files += collect_files_with_ending(".c", work_dir+"/test")
  files += collect_files_with_ending(".cpp", work_dir+"/test")
  files += collect_files_with_ending(".h", work_dir+"/test")

  for file_name in files:
    #print "opening file " + file_name
    file = open(file_name, 'r').read()
  #
  #  new_file = re.sub(r'\/\*\*\*\*\*\*\*(.*?)________(.*?)conditions(.*?)\*\/', r'/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */', file,flags=re.DOTALL)
    new_file = re.sub(r'\/\* \#\#__FLEA_LICENSE_TEXT_PLACEHOLDER__\#\# \*\/', license_notice_text, file,flags=re.DOTALL)
    new_file = re.sub(r'\/\/ ##__FLEA_UNCOMMENT_IN_RELEASE__## ', r'', new_file, flags=re.DOTALL)
    new_file = re.sub(r'\/\* ##__FLEA_COMMENT_OUT_IN_RELEASE__## \*\/', r'//', new_file, flags=re.DOTALL)

    #new_file = re.sub(r'__FLEA_LICENSE_TEXT_PLACEHOLDER__', license_text, file,flags=re.DOTALL)
    trgt_file =  open(file_name, 'w')
    for line in new_file:
      trgt_file.write("%s" % line)
    trgt_file.close()
    #print new_file

#print files

def generate_with_license(license_name, have_test_data):
  shutil.copytree(include_dir, generate_dir + "/" + license_name + "/flea/include", False, ignore_svn_function) 
  shutil.copytree(src_dir, generate_dir + "/" + license_name + "/flea/src", False, ignore_svn_function) 
  shutil.copytree(test_dir, generate_dir + "/" + license_name + "/flea/test", False, ignore_svn_function) 
  shutil.copytree(build_cfg_dir, generate_dir + "/" + license_name + "/flea/build_cfg", False, ignore_svn_function) 
  shutil.copytree(pltf_supp_dir, generate_dir + "/" + license_name + "/flea/pltf_support", False, ignore_svn_function) 
  shutil.copy(cmakelists_file, generate_dir + "/" + license_name + "/flea")
 
  if(have_test_data):
    shutil.copytree(test_data_dir, generate_dir + "/" + license_name + "/flea/misc/testdata", False, ignore_svn_function) 

 
  license_file_path = "../../misc/licenses/" + license_name + ".txt"
  shutil.copy(license_file_path, generate_dir + "/" + license_name + "/flea/" + license_name + "_license.txt")

  shutil.copy("../../misc/doc/flea_manual/flea_manual.pdf", generate_dir + "/" + license_name + "/flea/")
  #shutil.rm("../../test/
  generate_for_license(license_name, generate_dir+ "/" + license_name + "/flea")

have_test_data = False
if(len(sys.argv) == 2):
    if(sys.argv[1] == "--with_testdata"):
        have_test_data = True
    else:
        print "error: invalid commandline argument"
        sys.exit(1)


shutil.rmtree(generate_dir + "/" + license_name_gpl + "/" + "flea", True)
shutil.rmtree(generate_dir + "/" + license_name_closed_source + "/" + "flea", True)

generate_with_license(license_name_gpl, have_test_data)
generate_with_license(license_name_closed_source, have_test_data)

