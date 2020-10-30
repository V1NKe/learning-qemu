#!/bin/sh
cd initramfs-busybox-x86_64
./make.sh
cd ..
./qemu-system-x86_64 -initrd ./initramfs-busybox-x86_64.cpio.gz -nographic -kernel ./vmlinuz-4.4.0-119-generic -append "priority=low console=ttyS0" -device ooo
