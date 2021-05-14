#/bin/sh

qemu-system-x86_64 \
-kernel /home/zzj/linux/arch/x86/boot/bzImage \
-hda /home/zzj/qemu-test/ubuntu.img \
-append "root=/dev/sda1 console=ttyS0 pcie_aspm=off intel_idle.max_cstate=1 numa_balancing=enable" \
-cpu host -machine kernel-irqchip=off -device edu -net nic -net user,hostfwd=tcp::2222-:22 -smp 1 -m 4096 --enable-kvm \

stty echo
