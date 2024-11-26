# 第3章实验

linux-kernel-module for GPT and EPT dump experiments.

First we modify ``kvm`` according to the book, or
1. (for linux 4.17, 4.18 and 4.19) ``cp linux-v4.1x/kvm/* "$LINUX_SRC"/arch/x86/kvm/`` directory,
2. (for linux 6.1, 6.6 and 6.12) ``patch -d "$LINUX_SRC" -p2 < "$PWD"/linux-v6.x/kvm-patch-hypercall22-linux_v6.1.115.patch``.

then recompile ``kvm`` in the host OS.

In the guest OS, clone the code, and run
1. (for linux 4.17, 4.18 and 4.19) ``cd linux-v4.1x; ../run.sh``
2. (for linux 6.1, 6.6 and 6.12) ``cd linux-v6.x; ../run.sh``

See GPT dump results in gpt-dump.txt, see EPT dump results in host OS ``dmesg``.

For more details please buy our book and read Chapter 3. Thanks!
