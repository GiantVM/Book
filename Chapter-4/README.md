# IO虚拟化实验部分

I/O 虚拟化章节列举了多种常见的 I/O 虚拟化技术以及其原理。在该章节的实验中，要为 edu 虚拟设备添加设备驱动，并使用测试脚本对虚拟设备进行访问。

使用方法：在客户机中使用 `make` 编译驱动程序与测试程序

```
sudo insmod pci.ko

dmesg

cat /proc/devices | grep edu
# 245 pci-edu

sudo mknod /dev/edu c <major> 0

ls /dev/edu

# testing
sudo ./test
dmesg
```
