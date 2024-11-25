obj-m += gpt-dump.o

LINUX_SRC ?= /lib/modules/$(shell uname -r)/build
all:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) modules
clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean
