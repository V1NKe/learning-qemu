gcc -static exp.c -o exp
rm ../initramfs-busybox-x86_64.cpio.gz
find . | cpio -o --format=newc > ../initramfs-busybox-x86_64.cpio
cd ..
gzip initramfs-busybox-x86_64.cpio
