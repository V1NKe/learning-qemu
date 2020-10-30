#!/bin/bash
cd ./rootfs
make
cd ..
./qemu-system-x86_64 -m 1024 -smp 2 -boot c -net nic -initrd ./rootfs.cpio -nographic -kernel ./vmlinuz-5.0.5-generic -L pc-bios -append "priority=low console=ttyS0" -drive file=./nvme.raw,if=none,id=D22 -device nvme,drive=D22,serial=1234 -monitor /dev/null
