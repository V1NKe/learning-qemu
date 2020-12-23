#!/bin/bash
set -e
make && make clean
sudo /usr/src/linux-5.4.40/scripts/sign-file sha512 /usr/src/linux-5.4.40/certs/signing_key.pem /usr/src/linux-5.4.40/certs/signing_key.x509 exp.ko
# gcc -static exp.c -o exp
find . | cpio -o --format=newc > ../rootfs.cpio