KERNELDIR := /home/chen/orangepi-build/kernel/orange-pi-5.10-rk35xx
CURRENT_PATH := $(shell pwd)
obj-m := led.o

build: kernel_modules
kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean