ifneq ($(KERNELRELEASE),)
obj-m := buttons.o
else
KDIR := /work/system/linux-3.4.10

all:
	$(MAKE) -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` modules clean
	rm -rf modules.order
endif
	
