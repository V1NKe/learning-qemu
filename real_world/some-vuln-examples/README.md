## Description

the virtio-2.6.0、vga-2.6.0、pcnet-2.2.0 is from the pdf example.

There are 3 examples about qemu device vulnerability.

He introduce how to find the input function from the bug trigger position,the way to fuzz device in code and the exploit writing skills.

### virtio-2.6.0:

I only write the poc ,and I think it can bot be exploited.You can try to write the exploit.

### vga-2.6.0:

I write the read and write primitive.but I can not find the place to leak some useful address.Maybe the reason is that I use the ubuntu18.04 host.

### pcnet-2.2.0:

I have a trouble with the phys to virtual address.The data I write didn't assignment.But the reason I have no idea.

