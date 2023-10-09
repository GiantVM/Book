#/bin/sh

rm edu-driver.ko
make -C /usr/src/linux-headers-4.15.0-112-generic M=/home/zzj/edu modules
