# 《深入浅出系统虚拟化：原理与实践》

《深入浅出系统虚拟化：原理与实践》是一本论述系统虚拟化的原理与实践的专业图书。全书分为6章,第1章概述系统虚拟化的基本概念、发展历史、趋势展望、主要功能和分类,以及目前典型的虚拟化系统,并介绍openEuler操作系统的虚拟化技术。第2~4章分别介绍系统虚拟化的三大组成部分:CPU虚拟化、内存虚拟化和I/O虚拟化的相关原理,并配备相应实验便于读者理解。第5章介绍基于ARMv8的鲲鹏虚拟化架构,并概述其CPU、中断、内存、I/O和时钟虚拟化的基本原理。第6章结合代码讲解轻量虚拟化平台StratoVirt的基本原理和技术特点,读者可以跟随本书从零开始打造一个具备基本功能的轻量虚拟化平台。

为便于读者高效学习、深入掌握系统虚拟化的基本原理,本书的源代码及安装运行说明均保存于GiantVM和StratoVirt开源社区。后续将通过开源社区进行代码更新和线上交流。

本仓库包含了若干章节的实验部分的代码。

+ [第2章CPU虚拟化实验](./Chapter-2)由余博识[@201608ybs](https://github.com/201608ybs)负责
+ [第3章内存虚拟化实验](./Chapter-3)由贾兴国[@snake0](https://github.com/snake0)负责
+ [第4章I/O虚拟化实验](./Chapter-4)由张正君[zzjsjtu](https://github.com/zzjsjtu)负责

## MOOC学习链接
请见头歌平台课程：[云计算及虚拟化课程](https://www.educoder.net/paths/xea4n9b8)

## 勘误校对

2023-10-09: 修复原书第三章中实验的 PTE 大小为 64 bytes 的显示错误，修改为 8 bytes，对应原书 P157, P160 的 `gpt-dum.txt` 与 `ept-dump.txt` 中的输出，修改后可见 `Chapter-3` 中的对应文件。修改前的内容可见[该 commit](https://github.com/GiantVM/Book/tree/86b1ae0c7d29b9cf1a1caceb66c68a030f3a0039).

## 升级说明
2024-11-26: 将第三章实验的代码更新到linux v6.x版本。使用方法见Chapter-3里的README.md。

2024-11-28: 补传 Chapter-6/QA.sh 脚本，提供了实验文档 [lab.md](./lab.md)，补充头歌MOOC链接。
