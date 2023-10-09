obj-m += pci.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

.PHONY: all clean

all:
	$(MAKE) -C '/usr/src/linux-headers-$(shell uname -r)' M='$(PWD)' modules
	$(CC) test.c -o test

clean:
	$(MAKE) -C '/usr/src/linux-headers-$(shell uname -r)' M='$(PWD)' clean
	rm test
