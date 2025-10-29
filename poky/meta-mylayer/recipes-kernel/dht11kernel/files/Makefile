# Yocto will inject KERNEL_SRC and KERNEL_VERSION
KERNEL_SRC ?= $(KERNEL_SRC)
KERNEL_VERSION ?= $(KERNEL_VERSION)

obj-m := dht11_kmod.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
