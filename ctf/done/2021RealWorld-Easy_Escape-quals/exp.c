#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

void *mmap_addr;
void *fun_addr;
void *fun_result_addr;
#define dma_addr_t uint32_t

typedef struct FunReq{
    uint64_t total_size;
    char *list[127];
}FunReq;

typedef struct FunState{
    dma_addr_t addr;
    uint32_t req_total_size; // size <= 0x1FBFF
    uint32_t FunReq_list_idx;
    dma_addr_t result_addr;  // put_result(fun, 1u);赋值 1或2
    FunReq *req;
    //AddressSpace *as;
}FunState;

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)
#define PFN_PRESENT (1ull << 63)
#define PFN_PFN     ((1ull << 55) - 1)

int fd;

uint32_t page_offset(uint32_t addr)
{   
    return addr & ((1 << PAGE_SHIFT) - 1);
}

uint64_t gva_to_gfn(void *addr)
{   
    uint64_t pme, gfn;
    size_t offset;
    offset = ((uintptr_t)addr >> 9) & ~7;
		lseek(fd, offset, SEEK_SET);
    read(fd, &pme, 8);
    if (!(pme & PFN_PRESENT))
        return -1;
    gfn = pme & PFN_PFN;
    return gfn;
}

uint64_t gva_to_gpa(void *addr)
{
    uint64_t gfn = gva_to_gfn(addr);
    assert(gfn != -1);
    return (gfn << PAGE_SHIFT) | page_offset((uint64_t)addr);
}

void mmio_write(uint32_t addr,uint32_t value){
    *(uint32_t *)((uint8_t *)mmap_addr + addr) = value;
}

uint32_t mmio_read(uint32_t addr){
    return *(uint32_t *)((uint8_t *)mmap_addr+addr);
}

void set_req_total_size(uint32_t val){
    val = val << 10;
    mmio_write(0,val);
}

void set_addr(uint32_t val){
    mmio_write(4,val);
}

void set_result_addr(uint32_t val){
    mmio_write(8,val);
}

void set_list_idx(uint32_t val){
    mmio_write(0xc,val);
}

void handle_data_write(uint32_t idx){
    set_list_idx(idx);
    mmio_write(0x10,0x1);
}

void handle_data_read(uint32_t idx){
    set_list_idx(idx);
    mmio_read(0x10);
}

void create_req(uint32_t size){
    set_req_total_size(size);
    mmio_write(0x14,0x1);
}

void delete_req(){
    mmio_write(0x18,0x1);
}

uint32_t get_addr(){
    return mmio_read(0x4);
}

int main(){
    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    int fd1 = open("/sys/devices/pci0000:00/0000:00:04.0/resource0",O_RDWR);
    if(fd1 == -1){
        perror("open error.");
    }

    mmap_addr = mmap(0,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED,fd1,0);
    if(mmap_addr == MAP_FAILED){
        perror("mmap error.");
    }

    printf("mmap addr: %p\n",mmap_addr);

    //init malloc addr -> fun
    fun_addr = malloc(0x500);
    fun_result_addr = fun_addr;
    uint32_t fun_phy_addr = gva_to_gpa(fun_addr);
    printf("the fun addr is 0x%x\n",fun_phy_addr);
    printf("the fun addr is 0x%x\n",get_addr());

    ((uint32_t *)fun_addr)[0] = 0x11223344;
    ((uint32_t *)fun_addr)[1] = 0x55667788;

    // create_req(0x10);
    // handle_data_write(0x1);

    // printf("the size 0x%x\n",mmio_read(0x0));
    // delete_req();
    // create_req(0xf);

    return 0;
}