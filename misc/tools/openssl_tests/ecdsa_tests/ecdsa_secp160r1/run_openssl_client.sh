#!/bin/sh

rm /tmp/cachain.pem
cat misc/tools/openssl_tests/ecdsa_tests/ecdsa_secp160r1/certs/ECDSA_SECP160R1_ROOT_CA.TA.pem.crt >> /tmp/cachain.pem
echo "" >> /tmp/cachain.pem
cat misc/tools/openssl_tests/ecdsa_tests/ecdsa_secp160r1/certs/ECDSA_SECP160R1_SUB_CA.CA.pem.crt >> /tmp/cachain.pem

openssl s_client -connect 127.0.0.1:4444 -CAfile /tmp/cachain.pem -curves secp160r1 -cert misc/tools/openssl_tests/ecdsa_tests/ecdsa_secp160r1/certs/ECDSA_SECP160R1_EE.TC.pem.crt -key misc/tools/openssl_tests/ecdsa_tests/ecdsa_secp160r1/certs/ECDSA_SECP160R1_EE.pem
