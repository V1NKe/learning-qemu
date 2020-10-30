#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qapi/visitor.h"

#define TYPE_PCI_VIN_DEVICE "vin"
#define VIN(obj) OBJECT_CHECK(VinState,obj,TYPE_PCI_VIN_DEVICE)

#define FACT_IRQ  0x00001
#define DMA_IRQ   0x00100

#define DMA_START 0x40000
#define DMA_SIZE  4096

#define MMAP_SIZE 0x10000

typedef struct {
    PCIDevice pdev;
    MemoryRegion mmio;

    QemuThread thread;
    QemuMutex thr_mutex;
    QemuCond thr_cond;
    bool stopping;

    uint32_t addr4;
    uint32_t fact;
#define VIN_STATUS_COMPUTING 0x01
#define VIN_STATUS_IRQFACT   0x80
    uint32_t status;

    uint32_t irq_status;

#define VIN_DMA_RUN  0x1
#define VIN_DMA_DIR(cmd)  (((cmd)&2)>>1)
#define VIN_DMA_FROM_PCI  0
#define VIN_DMA_TO_PCI    1
#define VIN_DMA_IRQ       4
    struct dma_state{
        dma_addr_t src;
	dma_addr_t dst;
	dma_addr_t cnt;
	dma_addr_t cmd;
    }dma;
    QEMUTimer dma_timer;
    char dma_buf[DMA_SIZE];
    uint64_t dma_mask;
    char *mmap_addr;
    int choice;
    char *malloc_once;
}VinState;

static bool vin_msi_enabled(VinState *vin){
    return msi_enabled(&vin->pdev);
}

static void vin_raise_irq(VinState *vin,uint32_t val){
    vin->irq_status |= val;
    if(vin->irq_status){
        msi_notify(&vin->pdev,0);
    }else{
        pci_set_irq(&vin->pdev,1);
    }
}

static void vin_lower_irq(VinState *vin,uint32_t val){
    vin->irq_status &= ~val;

    if(!vin->irq_status && !vin_msi_enabled(vin)){
        pci_set_irq(&vin->pdev,0);
    }
}

static bool within(uint32_t addr,uint32_t start,uint32_t end){
    return start <= addr && addr < end;
}

static void vin_check_range(uint32_t addr,uint32_t size1,uint32_t start,uint32_t size2){
    uint32_t end1 = addr + size1;
    uint32_t end2 = start + size2;

    if(within(addr,start,end2) && end1 > addr && within(end1,start,end2)){
        return;
    }

    hw_error("VIN: DMA range 0x%.8x-0x%.8x out of bounds (0x%.8x-0x%.8x)!",addr,end1-1,start,end2-1);
}

static dma_addr_t vin_clamp_addr(const VinState *vin,dma_addr_t addr){
    dma_addr_t res = addr & vin->dma_mask;

    if(addr != res){
        printf("VIN: clamping DMA %#.16"PRIx64" to %#.16"PRIx64"!\n",addr,res);
    }

    return res;
}

static void vin_dma_timer(void *opaque){
    VinState *vin = opaque;
    bool raise_irq = false;

    if(!(vin->dma.cmd & VIN_DMA_RUN)){
        return;
    }

    if(VIN_DMA_DIR(vin->dma.cmd) == VIN_DMA_FROM_PCI){
        uint32_t dst = vin->dma.dst;
	vin_check_range(dst,vin->dma.cnt,DMA_START,DMA_SIZE);
	dst -= DMA_START;
	pci_dma_read(&vin->pdev,vin_clamp_addr(vin,vin->dma.src),vin->dma_buf+dst,vin->dma.cnt);
    }else{
        uint32_t src = vin->dma.src;
	vin_check_range(src,vin->dma.cnt,DMA_START,DMA_SIZE);
	src -= DMA_START;
	pci_dma_write(&vin->pdev,vin_clamp_addr(vin,vin->dma.dst),vin->dma_buf+src,vin->dma.cnt);
    }

    vin->dma.cmd &= ~VIN_DMA_RUN;
    if(vin->dma.cmd & VIN_DMA_IRQ){
        raise_irq = true;
    }

    if(raise_irq){
        vin_raise_irq(vin,DMA_IRQ);
    }
}

static void dma_rw(VinState *vin,bool write,dma_addr_t *val,dma_addr_t *dma,bool timer){
    if(write && (vin->dma.cmd & VIN_DMA_RUN)){
        return;
    }

    if(write){
        *dma = *val;
    }else{
        *val = *dma;
    }

    if(timer){
        timer_mod(&vin->dma_timer,qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL)+100);
    }
}

static uint64_t vin_mmio_read(void *opaque,hwaddr addr,unsigned size){
    VinState *vin = opaque;
    uint64_t buf = 0x0;

    int cmd = (0xFF0000 & addr) >> 16;
    switch(cmd){
	case 0x6:
		{
		    uint32_t offset = (0xFFFF & addr);
		    if(offset < (MMAP_SIZE - size)){
		        memcpy(&buf,vin->mmap_addr+offset,size);
		    }
		}
		break;
	default :
		break;
    }
    return buf;
}

static void vin_mmio_write(void *opaque,hwaddr addr,uint64_t val,unsigned size){
    VinState *vin = opaque;

    int cmd = (0xFF0000 & addr) >> 16;

    switch(cmd){
        case 0x3://edit
	    {
	        uint32_t offset = (0xFFFF & addr);
		if(offset < (MMAP_SIZE - size)){
	            memcpy(vin->mmap_addr+offset,(void *)&val,size);
		}
	    }
	    break;
        case 0x8:
	    {
                //char *buf;
                uint32_t size = (0xFFFF & addr);
                malloc(size*sizeof(uint64_t));
                //memcpy(buf,(void *)&val,8);
	    }
            break;
        case 0x1:
	    {
	        uint32_t offset = (0xFFFF & addr);
		if(offset < (MMAP_SIZE - size)){
	            free(vin->mmap_addr+offset);
		}
            }
	    break;
        case 0x4:
            {
                uint32_t size = (0xFFFF & addr);
                if(vin->choice == 1){
                    vin->malloc_once = malloc(size*sizeof(uint64_t));
                    --vin->choice;
                }
            }
            break;
        case 0x7:
            {
                uint32_t offset = (0xFFFF & addr);
                if(offset < 0x30){
                    memcpy(vin->malloc_once+offset,(void *)&val,size);
                }
            }
            break;
	default :
	    //printf("Invaild option!");
	    break;
    }
}

static const MemoryRegionOps vin_mmio_ops = {
    .read = vin_mmio_read,
    .write = vin_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void *vin_fact_thread(void *opaque){
    VinState *vin = opaque;

    while(1){
        uint32_t val,ret = 1;

	qemu_mutex_lock(&vin->thr_mutex);
	while((atomic_read(&vin->status) & VIN_STATUS_COMPUTING) == 0 && !vin->stopping){
	    qemu_cond_wait(&vin->thr_cond,&vin->thr_mutex);
	}

	if(vin->stopping){
	    qemu_mutex_unlock(&vin->thr_mutex);
	    break;
	}

	val = vin->fact;
	qemu_mutex_unlock(&vin->thr_mutex);

	while(val > 0){
	    ret *= val--;
	}

	qemu_mutex_lock(&vin->thr_mutex);
	vin->fact = ret;
	qemu_mutex_unlock(&vin->thr_mutex);
	atomic_and(&vin->status,~VIN_STATUS_COMPUTING);

	if(atomic_read(&vin->status) & VIN_STATUS_IRQFACT){
	    qemu_mutex_lock_iothread();
	    vin_raise_irq(vin,FACT_IRQ);
	    qemu_mutex_unlock_iothread();
	}
    }

    return NULL;
}

static void pci_vin_realize(PCIDevice *pdev,Error **errp){
    VinState *vin = VIN(pdev);
    uint8_t *pci_conf = pdev->config;

    pci_config_set_interrupt_pin(pci_conf,1);

    if(msi_init(pdev,0,1,true,false,errp)){
        return;
    }

    timer_init_ms(&vin->dma_timer,QEMU_CLOCK_VIRTUAL,vin_dma_timer,vin);

    qemu_mutex_init(&vin->thr_mutex);
    qemu_cond_init(&vin->thr_cond);
    qemu_thread_create(&vin->thread,"vin",vin_fact_thread,vin,QEMU_THREAD_JOINABLE);
    memory_region_init_io(&vin->mmio,OBJECT(vin),&vin_mmio_ops,vin,"vin-mmio",1 * MiB);
    pci_register_bar(pdev,0,PCI_BASE_ADDRESS_SPACE_MEMORY,&vin->mmio);
}

static void pci_vin_uninit(PCIDevice *pdev){
    VinState *vin = VIN(pdev);

    qemu_mutex_lock(&vin->thr_mutex);
    vin->stopping = true;
    qemu_mutex_unlock(&vin->thr_mutex);
    qemu_cond_signal(&vin->thr_cond);
    qemu_thread_join(&vin->thread);

    qemu_cond_destroy(&vin->thr_cond);
    qemu_mutex_destroy(&vin->thr_mutex);

    timer_del(&vin->dma_timer);
    msi_uninit(pdev);
}

static void vin_obj_uint64(Object *obj,Visitor *v,const char *name,void *opaque,Error **errp){
    uint64_t *val = opaque;

    visit_type_uint64(v,name,val,errp);
}

static void vin_instance_init(Object *obj){
    VinState *vin = VIN(obj);

    vin->dma_mask = (1UL << 28) -1;
    vin->choice = 1;
    vin->malloc_once = NULL;
    vin->mmap_addr = mmap(0,MMAP_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    object_property_add(obj,"dma_mask","uint64",vin_obj_uint64,vin_obj_uint64,NULL,&vin->dma_mask,NULL);
}

static void vin_class_init(ObjectClass *class, void *data){
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize = pci_vin_realize;
    k->exit = NULL;
    k->vendor_id = 0x4242;
    k->device_id = 0x8080;
    k->revision = 0x72;
    k->class_id = PCI_CLASS_OTHERS;
}

static void pci_vin_register_types(void){
    static InterfaceInfo interfaces[] = {
	{ INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    };
    static const TypeInfo vin_info = {
        .name          = "vin",
	.parent        = TYPE_PCI_DEVICE,
	.instance_size = sizeof(VinState),
	.instance_init = vin_instance_init,
	.class_init    = vin_class_init,
	.interfaces = interfaces,
    };

    type_register_static(&vin_info);
}
type_init(pci_vin_register_types)
