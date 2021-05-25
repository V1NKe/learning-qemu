#!/bin/bash
set -e
cd rootfs && ./pack.sh
cd ..
./qemu-system-x86_64 \
-m 4G -boot c \
-L ./pc-bios \
-initrd ./rootfs.cpio -nographic \
-kernel ./bzImage \
-append "priority=low console=ttyS0 quiet" \
-drive id=cdrom,file=./cdrom.img,if=none \
-device nvme,drive=cdrom,serial=0x1234,cmb_size_mb=0x1 \
-device virtio-gpu-pci \
-monitor /dev/null
