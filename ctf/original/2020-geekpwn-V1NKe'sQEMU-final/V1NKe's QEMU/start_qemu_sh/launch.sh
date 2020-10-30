#!/bin/bash

sudo ./qemu-system-x86_64 \
	-kernel ./bzImage \
	-append "console=ttyS0 root=/dev/sda rw" \
	-hda ./qemu.img \
	-enable-kvm -m 2G -nographic \
	-net user,hostfwd=tcp::2222-:22 -net nic
