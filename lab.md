# 虚拟化课程实验 2024秋
## 概览

本实验文档包括以下四个部分：
1. CPU 虚拟化实验：使用 KVM 接口运行最简化 QEMU；
2. 内存虚拟化实验：打印虚拟机的内存地址翻译（GVA->HPA）过程；
3. I/O 虚拟化：访问虚拟设备，实现自定义的功能；
4. StratoVirt 与 QEMU 方案的对比；

### 通用实验环境

实验需要 Linux 支持 KVM，即存在 `/dev/kvm` 文件，因此**如果硬件不支持嵌套虚拟化，则需要在物理机环境下进行，并自行配置环境（配置方法见附录“非嵌套虚拟化的实验环境”）；**
本实验在 **x86-64** 平台进行。

实验需要使用 `qemu-system-x86_64` ，初始化配置需要软件包 `cloud-image-utils` 中的 `cloud-localds` 工具。Debian、Arch下安装依赖的命令如下：
- Debian/Ubuntu：`apt install qemu-system-x86 cloud-image-utils`
- Arch：`pacman -S qemu-system-x86 cloud-image-utils`

#### 获取虚拟机镜像
为了简化环境配置，助教提供了安装有实验用内核的虚拟机镜像，供同学们选择（[pan.sjtu 链接](https://pan.sjtu.edu.cn/web/share/302c7ecaa54ac32ff96c08845fb91d6f)）。你可以选择助教提供的镜像，也可以参考助教提供镜像的制作步骤（见附录“助教配置实验用虚拟机步骤”）自行配置。

#### 设置虚拟机使用的 cloud-init 信息

通过YAML格式的配置文件，设置虚拟机的用户名为 `lab` 密码为 `p` 。如果使用SSH密钥访问，请自行修改配置中的公钥部分内容。

```bash
# Cloud Image Config
cat > cloud-config <<EOF
#cloud-config
users:
- name: lab
  sudo: ALL=(ALL) NOPASSWD:ALL
  shell: /bin/bash
  groups: [adm, audio, cdrom, dialout, dip, floppy, plugdev, sudo, video, kvm]
  gecos: lab user
  plain_text_passwd: p
  lock_passwd: false
  # 如果有SSH密钥，请取消下面两行的注释，并按实际公钥内容修改
  #ssh_authorized_keys:
  #- ssh-ed25519 AAAA... # 修改成你的公钥（id_XXX.pub 文件内容）

ssh_pwauth: true

apt:
  primary:
  - arches: [ default ]
    uri: 'https://mirror.sjtu.edu.cn/debian/' # 或者其它镜像源
  security:
  - arches: [ default ]
    uri: 'https://mirror.sjtu.edu.cn/debian-security/' # 或者其它镜像源

package_update: true
EOF

cloud-localds ./seed.iso cloud-config
```

#### 初始化配置虚拟机

```bash
sha512sum -c SHA512SUMS  # 校验
zstd -d debian-12.qcow2.zst  # 解压
```

用 `qemu-img` 扩容 `debian-12.qcow2` （20G肯定足够实验需要，具体可按实际需要调整）。

```bash
qemu-img resize debian-12.qcow2 20G
```

然后用cloud-init配置文件 `seed.iso` 配置虚拟机镜像 `debian-12.qcow2` ，命令如下。

```bash
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -drive file=seed.iso,format=raw,if=virtio \
    -device virtio-rng-pci \
    -m 1G
```

启动后可以用 `seed.iso` 中配置的用户名与密码登录从QEMU模拟的串口终端登录，也可以从 `ssh -p2222 lab@localhost` 用密码（或SSH密钥，如有配置）从SSH登录。

登录后 `sudo poweroff` 关闭虚拟机。`debian-12.qcow2` 已配置完成。配置结束后虚拟机启动命令可删去 `seed.iso` 项，简化后如下。

```bash
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -device virtio-rng-pci \
    -m 1G
```

### 重要链接

- 课程的参考书籍为 《深入浅出系统虚拟化：原理与实践》；

- 实验的代码：<https://github.com/GiantVM/Book>，请 clone 该仓库；

- StratoVirt 平台：<https://gitee.com/openeuler/stratovirt>，是最后一部分的轻量级虚拟机对比方案；

- QEMU 源码：<https://github.com/qemu/qemu>，实验三涉及到需要修改并重新编译 QEMU 的内容，目前助教在 6.2、7.2、9.1 版本上均可以完成实验；

## CPU 虚拟化

该部分对应 《深入浅出系统虚拟化：原理与实践》 的 2.3.5 节的实验，请先学习该部分的内容，实验代码见：<https://github.com/GiantVM/Book/tree/master/Chapter-2>

### 调研环节

请在报告中回答以下问题：
1. 请调研并用你的理解解释可虚拟化架构与不可虚拟化架构的概念（参考书籍 2.1 节）
2. 请基于你的理解谈谈虚拟化的“陷入再模拟”的概念（参考书籍 1.3.3 节）
3. 请调研并用你的理解解释 Intel VT-x 的特权级是如何划分的。这种非根模式为何有助于 Hypervisor “陷入再模拟”（参考书籍2.2节）？

### 实验环节
#### 实验说明

- 本实验对 Linux 内核版本没有要求，只需要支持 KVM 的机器即可；
- 下载[实验代码仓库](https://github.com/GiantVM/Book)，进入 `Chapter-2` 目录下，文件 `sample-qemu.c` 实现了一个类似于 QEMU 的小程序，在 x86 平台通过调用 KVM 接口，执行指定二进制代码输出 “Hello, World!”；
- 可以通过 `make` 指令编译目标程序，运行后可以在终端中看到输出结果（如果显示 `/dev/kvm` 访问权限问题，请 `sudo` 执行，或者 `sudo gpasswd -a "$USER" kvm` 添加当前用户的 `kvm` 用户组权限，然后重新登录用户）；
- 该实验在通过 ioctl 调用 `KVM` 创建 VM、设置虚拟 CPU、设置虚拟内存、设置虚拟机寄存器内容后，在 while 循环中调用 `KVM_RUN` 指令进入 VM 中开始执行预设的写在 `code` 数组中的二进制代码，并在触发部分特权指令（如 `code` 变量中使用的 out 汇编指令）后，KVM 退出，进入到“陷入再模拟”阶段，可以根据 KVM 的退出原因以及相应参数进行自定义的模拟，直到处理完成后进入下一个 `KVM_RUN` 的阶段使客户机继续运行。

#### 实验目标

- 理解了上述流程后，请着重关注 `KVM_EXIT_IO` 的模拟过程，**并根据你的理解重写该部分的 “陷入再模拟” 过程**，使得运行时输出到终端的 “Hello World!” 中的小写字符变为输出大写字符“HELLO WORLD!”；
- **请将修改后的代码版本以 `modified-qemu.c` 文件名与报告一并提交；**

## 内存虚拟化

该部分对应 《深入浅出系统虚拟化：原理与实践》 的 3.3.2 和 3.3.4 节的实验，请先学习该部分的内容，实验代码见：<https://github.com/GiantVM/Book/tree/master/Chapter-3>

### 调研环节

请在报告中回答以下问题：

1. 虚拟内存到物理内存的映射是如何通过页表实现的？（参考教材 3.2.1 节）
2. 请阐述**影子页表**（SPT）和**扩展页表**（EPT）各自的优缺点（参考教材 3.2.2-3.2.3 节）
3. **（选做，不扣分）**请根据你的理解解释 QEMU是如何通过 KVM 接口建立 EPT 的（参考教材第三章图 3-12 ）

### 实验环节
#### 实验说明

本次实验的内存虚拟化部分需要修改内核以添加自定义Hypercall。

在 Linux 内核的基础上，实验需要的修改主要增添了一个 Hypercall，具体内容位于 x86.c 中 `kvm_emulate_hypercall` 函数中的 case 22，即客户机中可以通过 Hypercall 22 来访问该功能。随机 KVM 会在主机中打印出 EPT 地址翻译过程（GPA -> HPA, 在主机中通过 dmesg 查看）。实验代码中的 gpt-dump.c 则是作为内核模块运行在客户机上，也会打印出客户机中内核可见的 GVA -> GPA 地址翻译过程（在 `gpt-dump.txt` 中查看）

#### 实验目标

这一部分的任务是：
1. 配置实验环境；
2. 下载[实验代码](https://github.com/GiantVM/Book)，进入 `Chapter-3` 目录下，编译会调用 Hypercall 22 的内核模块；
3. 构建客户机环境，在其中安装调用 Hypercall 22 的内核模块并运行，观察运行结果；
4. 在主机中借助 QEMU 监控器打印客户机 MemoryRegion 树结构；

请将各个部分完成后的情况截图并写在报告里，完成实验后请将 `gpt-dump.txt` 的内容以及主机执行 `dmesg` 命令后打印的 EPT 翻译的内容记录到报告里，并分析从内存地址从 GVA 到 HPA 的地址翻译过程

#### 配置实验环境

`debian-12.qcow2` 配置请见前面“通用实验环境”。然后用 `qemu-img` 创建用于容纳内层虚拟机的外层虚拟机镜像 `debian-12-outer.qcow2` 。

```bash
qemu-img create -b debian-12.qcow2 -F qcow2 -f qcow2 debian-12-outer.qcow2
```

然后再启动虚拟机，作为虚拟化的宿主机（注意启动命令里，使用的虚拟机镜像是 <code>debian-12<b>-outer</b>.qcow2</code>）。

```bash
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12-outer.qcow2,format=qcow2,if=virtio \
    -device virtio-rng-pci \
    -m 1G

# 把 debian-12.qcow2 传入外层的虚拟机，作为嵌套虚拟机启动的镜像
scp -P2222 debian-12.qcow2 lab@localhost:
```

#### 运行客户机

请在外层虚拟机中运行 QEMU 命令行启动嵌套虚拟机。

```bash
# 注意启动镜像的文件名，以及分配内存不要用满外层虚拟机的内存
sudo apt install qemu-system-x86
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -device virtio-rng-pci \
    -m 512M
```

#### 查看地址翻译过程

请在客户机和主机中完成实验，验证 EPT 翻译的过程：

```bash
# 在客户机中运行
cd ~/Book/Chapter-3
./run.sh
# 应在此处保存 gpt-dump.txt 中的内容，为客户机页表的翻译过程
# 若提示 Command not found，请执行 chmod +x ./run.sh 为脚本添加可执行权限后再运行

# 退出客户机：
sudo poweroff

# 在主机运行以下命令，应该可以看到 EPT 翻译过程
sudo dmesg
# 应在此处保存主机中 EPT 的翻译过程的内容
```

#### 打印 QEMU MemoryRegion 树

请在主机中运行 QEMU 命令行启动客户机，并通过 QEMU 监控器打印客户机的 MemoryRegion 树

```bash
# 在主机中完成实验
# 启动 qemu，注意与之前代码有不同之处
# `-serial null` 参数将抑制串口输出到当前命令行
# `-monitor stdio` 参数将启动 qemu monitor 并重定向到字符设备 stdio，即当前命令行
qemu-system-x86_64 -nographic -serial null -monitor stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -device virtio-rng-pci \
    -m 1G
```
此时命令行将进入 qemu monitor，可以通过 help 查看支持的命令

```plaintext
(qemu) help
```

使用命令 `info mtree` 来打印此客户机的 MemoryRegion 树，不同宽度的缩进表示不同树的深度，应在此处截图

```plaintext
(qemu) info mtree
```

#### 进行实验分析

- 请根据你记录的翻译过程进行分析，描述 GVA 到 HPA 的翻译过程，并写在报告中；

- 请简要描述你的客户机 MemoryRegion 树结构。

## I/O 虚拟化

该部分对应 《深入浅出系统虚拟化：原理与实践》 的 4.3.4 节的实验，请先学习该部分的内容，实验代码见：<https://github.com/GiantVM/Book/tree/master/Chapter-4>

### 调研环节

请在报告中回答以下问题：
1. 请阐述 x86 中 MMIO 与 PIO 两种 I/O 方式的特点（参考教材 4.2.2 节）；
2. 请简要描述 virtio 设备在进行 I/O 操作时的工作原理，这样的半虚拟化架构有什么优点？（参考教材 4.2.3 节）
3. **（选做，不扣分）**请调研设备枚举过程并根据你的理解回答设备是如何被发现的（参考书籍的 4.2 节，以及 UEFI 相关内容）；

### 实验环节

本次实验对 Linux 版本没有特别要求，可以复用之前的客户机环境。

#### 实验目标

- 在 QEMU 中添加模拟的 edu 设备，在客户机环境安装对应驱动并使用测试程序访问虚拟设备，请将客户机中的 log 保存到报告中；
- 基于 QEMU 模拟 edu 设备的实现原理，为 edu 设备添加一项功能；

#### 启动客户机

请在主机上创建 QEMU 客户机，并添加 edu 设备，可以复用之前实验的客户机镜像文件，注意 QEMU 启动的脚本有所区别，添加了 `-device edu` 行：
```bash
# 在主机上执行：
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -drive file=seed.iso,format=raw,if=virtio \
    -device virtio-rng-pci \
	-device edu,dma_mask=0xFFFFFFFF \
    -m 1G
```

#### 安装 edu 设备驱动

请在客户机内编译并安装 edu 设备驱动：

```bash
# 在客户机内执行：
# 配置 Guest 环境
sudo apt install git gcc make

# 克隆仓库，切换到实验代码 Chapter-4 目录下进行编译
git clone https://github.com/GiantVM/Book.git
cd Book/Chapter-4
make

# 安装设备驱动并查看设备状态
sudo insmod pci.ko

# 查看 PCI 设备
lspci
# 结果中应该会看到一行：
# 00:04.0 Unclassified device [00ff]: Device 1234:11e8 (rev 10)
# 同样的 QEMU 命令启动，应该是同样的设备地址，运行如下命令查看详细信息（替换其中的 00:04.0 为 PCIe 设备号）
lspci -s 00:04.0 -vvv -xxxx
# 应该可以看到 kernel drvier in use 一项

# 此时安装了驱动，但是还没有在 Linux 中注册设备节点，即看不到 /dev/edu 文件
# 查看驱动信息，以及主设备号，其中的数字为主设备号
cat /proc/devices | grep edu
# 如： 248 pci-edu

# 构造设备节点，<major> 为你看到的主设备号
sudo mknod /dev/edu c <major> 0

# 此后应该可以看到 /dev/edu 文件了
ls /dev/edu

# 清空 dmesg，运行测试代码
sudo dmesg --clear
sudo ./test

# 请将输出内容保存到报告中
sudo dmesg
```

#### 修改 edu 设备

QEMU 中默认的 edu 设备实现了很简单的功能，并支持通过 ioctl 来调用这些功能，其具体的内容可以参考教材 4.3.4 小节的内容。
- 代码 `Chapter-4/test.c` 中体现了 edu 驱动的调用方法，通过指定不同的 ioctl 参数，以调用不同的 edu 驱动功能；
- `Chapter-4/pci.c` 的驱动代码中，则实现了 edu 驱动的 write、read 和 ioctl 等功能，其中 `edu_ioctl` 函数根据传入参数的不同，将会对 edu 设备执行不同的 MMIO 操作；
- 这些 MMIO 操作最终都会在 QEMU 的模拟 edu 设备中进行处理，其实现可以在 QEMU 源码的 `hw/misc/edu.c` 文件中找到，其中最主要的是 `edu_mmio_write` 和 `edu_mmio_read` 两个函数，根据 MMIO 请求的地址不同，将完成不同的设备操作；

请参考上述过程，修改 `Chapter-4/test.c` 、 `Chapter-4/pci.c` 以及QEMU 源代码的 `edu_mmio_write` 或 `edu_mmio_read` 函数（需要重新编译 QEMU），为 edu 模拟设备额外添加一个简单的功能（如打印指定信息，简单的数值运算等）。
请将修改后的代码命名为 `*_modified.c` 和报告一同提交，并在报告中简单描述实现的过程，附上可以体现修改后运行结果的截图。

## StratoVirt 与 QEMU 方案对比

StratoVirt 作为一个使用 Rust 语言编写的轻量级虚拟化平台，相对 QEMU 删减了许多不常用的功能与设备模型，并针对轻量化这一特性进行优化。该部分的内容为对比 StratoVirt 与 QEMU 两个虚拟化解决方案的各项性能指标。

### 调研环节

请在报告中回答以下问题：
1. Linux 启动的协议以及 E820 表是如何设计的？（参考教材 6.4.5 节）
2. 请简述 Epoll 实现原理？（参考教材 6.4.7 节）
3. 请执行以下脚本并简述 StratoVirt 的技术重点
	1. 本部分依赖于 shell 环境，选取一个空文件夹即可，脚本为： Book/Chapter-6/QA.sh
	2. 该脚本基于 StratoVirt 的 gitee 仓库中的 `mini_stratovirt_edu` 分支进行，并基于不同 commit 由简到繁构建系统，可以从代码角度以问答形式帮你了解 StratoVirt 的技术重点

```bash
./QA.sh
```

### 实验环节
#### 实验目标

你需要对比两种虚拟化平台的以下方面：
- Linux 启动时间的对比
- StratoVirt 与 QEMU 内存占用对比
- I/O 速度对比
请将实验数据记录在报告中，并分析两种平台的轻量化程度

#### StratoVirt 环境搭建

StratoVirt 的构建可以参考其代码仓库和官方文档：<https://gitee.com/openeuler/stratovirt>
主要包括以下内容：
- Rust 和 Cargo 环境的准备
- StratoVirt 源码下载与编译

#### 测试虚拟机准备

在“通用实验环境”的 `debian-12.qcow2` 基础上，获取启动内核文件，并安装 `sysbench` 。

```bash
# 在实验主机中，启动虚拟机
qemu-system-x86_64 -nographic -serial mon:stdio -cpu host -enable-kvm \
    -nic user,ipv6=off,model=virtio,hostfwd=tcp:127.0.0.1:2222-:22 \
    -drive file=debian-12.qcow2,format=qcow2,if=virtio \
    -device virtio-rng-pci \
    -m 1G

# 在实验主机中，获取内核
scp -P2222 lab@localhost:/boot/vmlinuz-6.6.63-lab .

# 在虚拟机中，安装sysbench
sudo apt install sysbench
sudo poweroff
```

#### 启动性能对比

- 启动时间：请在 StratoVirt 客户机和 QEMU 客户机中分别记录启动时间，要求两种方案各启动 3 次记录结果（建议与内存占用一起测试）。
- 内存占用：请在主机中记录同样使用 1G 内存时，使用 pmap 记录 StratoVirt 与 QEMU 进程各自占用的内存数量，要求两种方案各启动 10 次记录结果。

由于使用 microvm 设备模型启动，需要在QEMU启动时指定内核。

StratoVirt启动命令：

```bash
# 使用 microvm 启动，注意 <path/to/stratovirt> 应当替换为编译后的
# stratovirt 源代码目录下的 target/release/stratovirt 可执行文件
# 或者将 stratovirt 添加到环境变量中
<path/to/stratovirt> -disable-seccomp \
    -machine microvm -m 1024 -cpu host \
    -kernel vmlinuz-6.6.63-lab -append 'console=ttyS0 root=/dev/vda1' \
    -drive file=debian-12.qcow2,id=rootfs,format=qcow2 \
    -device virtio-blk-device,drive=rootfs,id=rootfs \
    -qmp unix:/tmp/stratovirt.sock,server,nowait -serial stdio

# 虚拟机 poweroff/reboot 退出如果卡在 “reboot: System halted”，可以直接 `pkill stratovirt` 强行退出

# 在客户机中查看内核的启动时间并记录在报告中
systemd-analyze time

# 在主机使用 pmap 查看进程占用的内存
pmap -x $(pgrep stratovirt)
```

QEMU：

```bash
# 使用 qemu 启动
qemu-system-x86_64 -enable-kvm \
    -machine microvm -m 1024 -cpu host \
    -kernel vmlinuz-6.6.63-lab -append 'console=ttyS0 root=/dev/vda1' \
    -drive file=debian-12.qcow2,id=rootfs,format=qcow2 \
    -device virtio-blk-device,drive=rootfs,id=rootfs \
    -qmp unix:/tmp/qemu.sock,server,nowait -nographic

# 查看启动时间并记录在报告中
systemd-analyze time

# 使用 pmap 查看进程占用的内存
pmap -x $(pgrep qemu)
```

#### sysbench性能测试
##### CPU 性能对比

采用sysbench进行测试，执行 `sysbench cpu help` 指令可以看到相应的帮助信息。

```plaintext
$ sysbench cpu help
sysbench 1.0.20 (using bundled LuaJIT 2.1.0-beta2)

cpu options:
  --cpu-max-prime=N upper limit for primes generator [10000]

```

要求在客户机中使用 sysbench cpu 测试完成以下测试：
- 线程数依次设置为1，4，16，32，64，测试不同线程情况下的结果；
- 每种类型的测试至少跑两次；

##### 内存性能对比

依旧采用sysbench进行测试，执行 sysbench memory help 指令可以看到相应的帮助信息。

```plaintext
$ sysbench memory help
sysbench 1.0.20 (using bundled LuaJIT 2.1.0-beta2)

memory options:
  --memory-block-size=SIZE    size of memory block for test [1K]
  --memory-total-size=SIZE    total size of data to transfer [100G]
  --memory-scope=STRING       memory access scope {global,local} [global]
  --memory-hugetlb[=on|off]   allocate memory from HugeTLB pool [off]
  --memory-oper=STRING        type of memory operations {read, write, none} [write]
  --memory-access-mode=STRING memory access mode {seq,rnd} [seq]

```

要求在客户机中使用 sysbench memory 测试以下测试：
- 线程数为4，内存块大小分别设置为1k，2k，4k，8k，测试不同内存块大小的影响
- 每组测试要测试 **顺序访问** 和 **随机访问** 两种情况，每组测试至少执行两次，每次至少 10 秒钟

##### I/O 性能对比

依旧采用sysbench进行测试，执行 `sysbench fileio help` 指令可以看到相应的帮助信息：

```plaintext
$ sysbench fileio help
sysbench 1.0.20 (using bundled LuaJIT 2.1.0-beta2)

fileio options:
  --file-num=N                  number of files to create [128]
  --file-block-size=N           block size to use in all IO operations [16384]
  --file-total-size=SIZE        total size of files to create [2G]
  --file-test-mode=STRING       test mode {seqwr, seqrewr, seqrd, rndrd, rndwr, rndrw}
  --file-io-mode=STRING         file operations mode {sync,async,mmap} [sync]
  --file-extra-flags=[LIST,...] list of additional flags to use to open files {sync,dsync,direct} []
  --file-fsync-freq=N           do fsync() after this number of requests (0 - don't use fsync()) [100]
  --file-fsync-all[=on|off]     do fsync() after each write operation [off]
  --file-fsync-end[=on|off]     do fsync() at the end of test [on]
  --file-fsync-mode=STRING      which method to use for synchronization {fsync, fdatasync} [fsync]
  --file-merged-requests=N      merge at most this number of IO requests if possible (0 - don't merge) [0]
  --file-rw-ratio=N             reads/writes ratio for combined test [1.5]

```

要求在客户机中使用 sysbench 完成以下几组测试：
- 线程数为1，bs设置为4k，测试模拟单个队列读写的延迟；
- 线程数为32，bs设置为128k，测试吞吐量，跑满整个磁盘带宽；
- 线程数为32，bs设置为4k，测试 IOPS；

每组测试至少要测试**随机读**、**随机写**、**顺序读**、**顺序写**四种情况，每种类型的测试**至少跑两次**，每次至少 10 秒钟，请将**测试方式**写入报告，并将结果记录为表格。

QEMU 命令行最好使用针对物理设备的 IO 而非镜像来完成实验，且 QEMU 侧需要禁用缓存。

QEMU磁盘设置参考 `-drive cache=none,if=virtio,id=vdisk1,format=raw,file=[mnt disk path]`

这里的挂载磁盘路径最好是对磁盘重新分区后的一个裸盘，因为读写测试时可能会影响磁盘上的内容

#### 总结

在获取启动时间、内存占用、CPU性能、内存性能和I/O性能数据后，请将数据记录以表格的形式在报告中，并分析 StratoVirt 相比 QEMU 的轻量化方面效果如何，并结合设计思想、编程语言、实现方案等分析为何会有这样的效果。

## 附录

### 非嵌套虚拟化的实验环境
如果你的实验环境不可使用嵌套虚拟化，那么实验2“内存虚拟化”将需要修改物理机的内核才可实验。内核编译方法参考后文“内核构建步骤”。

### 助教配置实验用虚拟机步骤

如果你选择自行搭建实验用虚拟机，下面提供了助教的配置步骤，供同学们参考。

#### 内核构建步骤

这一步在Debian 12 (Bookworm) Docker容器中编译内核，你可以按需修改选择的内核版本、构建的内核配置 `.config` 等。助教测试了 v6.1.115 v6.6.63 内核，均可正常运行。patch在最新的stable kernel v6.12.1 上也可应用，理论上应该也可正常运行（但未测试）。

```bash
# 安装构建需要的依赖
sudo apt install build-essential flex bison bc rsync libelf-dev libssl-dev debhelper

# 下载 linux 内核
wget https://mirror.sjtu.edu.cn/kernel/v6.x/linux-6.6.63.tar.{xz,sign}
# 如果不使用交大镜像： https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.63.tar{.xz,.sign}

# 验证签名
unxz -k linux-6.6.63.tar.xz
gpg --auto-key-locate wkd --locate-keys "gregkh@kernel.org"
gpg --verify linux-6.6.63.tar.sign

# 解压，打 hypercall 22 补丁
tar -xf linux-6.6.63.tar
cd linux-6.6.63
patch -p2 < ~/Book/Chapter-3/linux-v6.x/kvm-patch-hypercall-linux_v6.1.115.patch

# 配置内核
make x86_64_defconfig  # x86_64 默认配置
make kvm_guest.config  # 内核会作为kvm guest启动
for x in $(grep -v '^#' ~/microvm.config); do  # 为了 lab4 的 microvm 启动
    echo "$x=y" >> .config  # 内容参见 https://github.com/firecracker-microvm/firecracker/blob/main/docs/kernel-policy.md
done
for x in CONFIG_KVM{,_INTEL,_AMD}; do  # 内核提供kvm功能
    echo "$x=m" >> .config
done
make olddefconfig  # 整理

# 编译内核
make bindeb-pkg LOCALVERSION=-lab KDEB_PKGVERSION=$(make kernelversion)-1 -j16
# 此时可见 .. 里有 linux-{headers,image}-6.6.63-lab_6.6.63-1_amd64.deb  linux-libc-dev_6.6.63-1_amd64.deb
# 或者 make tarxz-pkg 以获取 `.tar.zst` 形式的安装包。编译机不是 Debian 时 *deb-pkg 需要额外安装dpkg包，tar*-pkg 不需要这些依赖，会更便利
```

#### 虚拟机安装内核步骤

这一步在Arch Linux上，下载Debian官方提供的虚拟机镜像，然后用 `qemu-nbd` 操作虚拟机硬盘，挂载文件系统并安装内核、调整GRUB默认启动项。

```bash
# 把 20241110-1927 改成 latest 可以获取最新版镜像
wget https://mirror.sjtu.edu.cn/debian-cdimage/cloud/bookworm/20241110-1927/debian-12-genericcloud-amd64-20241110-1927.qcow2

# 验证校验和
wget https://cloud.debian.org/images/cloud/bookworm/20241110-1927/SHA512SUMS
grep debian-12-genericcloud-amd64-20241110-1927.qcow2 SHA512SUMS | sha512sum -c -

mv debian-12-genericcloud-amd64-20241110-1927.qcow2 debian-12.qcow2

# 挂载qcow2镜像中的分区，参见 https://wiki.archlinux.org/title/QEMU#Mounting_a_partition_from_a_qcow2_image
sudo modprobe nbd
sudo qemu-nbd -c /dev/nbd0 debian-12.qcow2
mkdir rootfs
sudo mount /dev/nbd0p1 rootfs

# 把包含修改后内核的 *.deb 安装包文件夹共享进去
sudo mount --bind /path/to/deb-packages-dir rootfs/root

# chroot，并挂载 /{dev,sys} 以防后续 update-grub 时 grub-probe “failed to get canonical path of ...”
sudo arch-chroot rootfs

# 在chroot里面，root身份
PATH+=':/usr/sbin:/sbin'  # Arch默认sbin和bin合并所以PATH不含sbin，chroot到Debian需要额外添加
umount /sys/firmware/efi/efivars  # 显然不希望里面更新GRUB修改物理机的EFI设置，所以卸载EFI变量
apt search linux-image  # 查看当前安装的linux-image包名，显示linux-image-6.1.0-27-cloud-amd64
apt remove linux-image-6.1.0-27-cloud-amd64
dpkg -i /root/linux-{headers,image}-6.6.63-lab_6.6.63-1_amd64.deb  # 安装内核
exit

# 退出chroot后
sudo umount --recursive rootfs
sudo qemu-nbd -d /dev/nbd0
```
