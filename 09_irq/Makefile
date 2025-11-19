export CROSS_COMPILE=/home/chen/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
KERNELDIR := /home/chen/orangepi-build/kernel/orange-pi-5.10-rk35xx

CURRENT_PATH := $(shell pwd)
obj-m := keyirq.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

# build a simple user-space test app
app: ledAPP

ledAPP: ledAPP.c
	$(CROSS_COMPILE)gcc -O2 -Wall -o $@ $<

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	rm -f keyAPP