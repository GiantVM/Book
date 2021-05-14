#/bin/sh
qemu-system-x86_64  -smp 2 -m 4096 -enable-kvm ubuntu.img -cdrom ./ubuntu-16.04.7.iso -device edu,dma_mask=0xFFFFFFFF -net nic -net user,hostfwd=tcp::2222-:22
