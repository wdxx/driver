ifneq ($(KERNELRELEASE),)
obj-m := globalmem.o
else
KDIR := /work/kernel/linux-3.4.10/linux-3.4.10

all:
	$(MAKE) -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` modules clean
	rm -rf modules.order
endif
	
