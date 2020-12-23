#!/bin/bash
set -e
cd rootfs && ./pack.sh
cd ..
sudo ./qemu-system-x86_64 \
-m 1024 -smp 2 -boot c \
-initrd ./rootfs.cpio -nographic \
-kernel ./bzImage \
-append "priority=low console=ttyS0" \
-monitor /dev/null \
--enable-kvm \
-device pcnet,netdev=pc0 \
-netdev tap,id=pc0
