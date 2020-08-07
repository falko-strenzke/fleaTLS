
# Overview

fleaTLS is a lightweight implementation of TLS 1.2 and including written in C and provided under an Apache 2.0 license. 

It's features at a glance are:

* support of RSA (except key generation) and ECC 
* support of GCM mode
* Processing of X.509 certificates and revocation lists
* clean implementation
* well documented:
  * API documentation
  * manual (part of the API doc package)
* lightweight and flexible through control of compiled features

# Documentation 
For getting started with fleaTLS please refer to the Quick Start Manual in the API Documentation found in the file `misc/doc/api_doc_<version>.<date>`.

This documentation can be generated by changing into the directroy `misc/doc/api_doc/` and executing the script  `build_api_doc.sh`. Doxygen must be installed for this purpose.

# Versioning

In order to check out a stable release, always use the master branch. It always points to the latest release. Other branches are work in progress and exist merely for development purposes.
