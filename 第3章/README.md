# linux-kernel-module
linux-kernel-module for GPT and EPT dump experiments.

First we modify ``kvm`` according to the book, or according to the ``./kvm`` directory, then recompile ``kvm`` in the host OS.

Clone it in the linux-4.19 guest OS, and run ``./run.sh`` command.

See GPT dump results in gpt-dump.txt, see EPT dump results in host OS ``dmesg``.

For more details please buy our book and read Chapter 3. Thanks!
