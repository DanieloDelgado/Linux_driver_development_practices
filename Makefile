obj-m +=  rgb_platform_driver.o rgb_class_platform_driver.o
 
KERNEL_DIR ?= $(HOME)/linux

all:
	make -C $(KERNEL_DIR) \
	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
	SUBDIRS=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) \
	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
	SUBDIRS=$(PWD) clean
deploy:
	scp *.ko root@192.168.0.100:
