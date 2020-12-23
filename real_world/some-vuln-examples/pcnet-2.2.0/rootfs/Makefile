ifneq ($(KERNELRELEASE),)
	CONFIG_MODULE_SIG=y
	obj-m := exp.o

else

	KERN_DIR ?= /usr/src/linux-5.4.40/
	PWD := $(shell pwd)
	CONFIG_MODULE_SIG=y

default:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules

endif


clean:
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions *.mod