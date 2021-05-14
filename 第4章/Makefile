CONFIG_MODULE_SIG=n

obj-m := edu-driver.o

KERNELBUILD :=/home/zzj/linux

default:
	make -C $(KERNELBUILD) M=$(shell pwd) modules
clean:
	rm -rf *.o  *.ko *.mod.c .*.cmd .tmp_versions *.order *.symvers                                                              
