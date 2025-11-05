## 指定交叉编译器前缀（指向 toolchain 的 bin 下并以 '-' 结尾）
export CROSS_COMPILE=/home/chen/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
KERNELDIR := /home/chen/orangepi-build/kernel/orange-pi-5.10-rk35xx

CURRENT_PATH := $(shell pwd)
obj-m := chrdevbase.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
