#!/bin/bash
cd ./rootfs
make
cd ..
./qemu-system-x86_64 -initrd ./rootfs.cpio -nographic -kernel ./vmlinuz-5.0.5-generic -L pc-bios -append "priority=low console=ttyS0" -device qwb -monitor /dev/null
