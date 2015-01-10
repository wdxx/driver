ifneq ($(KERNELRELEASE),)
obj-m := button_input.o
else
KDIR := /work/system/linux-2.6.30.4

all:
	$(MAKE) -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` modules clean
	rm -rf modules.order
endif
	
