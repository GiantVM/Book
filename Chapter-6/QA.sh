#! /bin/bash
## was https://tcloud.sjtu.edu.cn/virtlabs/QA.sh (404 now)

RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
RESET=$(tput sgr0)

short_pause=1
mid_pause=2

if [ ! -d stratovirt ]; then
	git clone https://gitee.com/openeuler/stratovirt.git
fi

cd stratovirt
git checkout mini_stratovirt_edu


# Q1
git checkout cb249db1eb95738f703039f5f88d8ee4b5b4b27e
clear
echo "Now we start to build a initial mini stratovirt"
echo -e "\n\n"
echo -e "\
To make a minimal Stratovirt, we can use the KVM interfaces by ioctl syscall:\n\
\n\
The steps are:\n\
1. Open \"/dev/kvm\" and create a new vm\n\
2. Initialize guest OS memory \n\
3. Create a vCPU and initialize registers\n\
4. Run vCPU loop with \"trap and emulation\"\n\
"
sleep $short_pause
echo -e "\n"
echo "${RED}Question1${RESET}: How do we get the output in serial?"
read -p "Where to trap serial output(which address for PIO)? " answer1
echo -e "\n\
Let's look at the code: \n\
\n\
-------------------------------------------------------------\n\
"

echo "$(sed -n "26,34p" src/main.rs)"

echo -e "\
-------------------------------------------------------------\n\
\n\
Answer: The serial output address is pio address 0x3f8.\n\
Your answer: $answer1\n\
"
sleep $mid_pause
read -p"Enter next phase?[y]" ans

# Q2
clear
echo -e "\
Now we need to alloc and initialize guest memory\n\
The steps are:\n\
1. Alloc contiguous virtual memory in host OS\n\
2. use \`kvm_userspace_memory_region\` interface to setup memory mapping by ioctl\n\
"

echo -e "\
There are some example ways to alloc memory: \n\
1. use stack memory by local variables\n\
2. use heap memory by malloc\n\
3. let kernel alloc contiguous virtual memory by mmap\n\
\n\
"

sleep $short_pause
read -p"${RED}Question2${RESET}: What are the ways to alloc large contiguous virtual memory?" ans

echo -e "\n\
Let's look at the code in StratoVirt:\n\
"

echo "-------------------------------------------------------------"

echo "$(sed -n "41,63p" src/main.rs)"

echo "-------------------------------------------------------------"

echo -e "\
Answer: The mmap syscall is the best way to alloc large guest memory\n\
because the size of stack and heap are limited\n\
\n\
"
sleep $mid_pause

read -p"Enter next phase?[y]" ans

# Q3
clear
echo -e "\
Virtualization technologies capture sensitive instructions at the instruction-level\n\
and are emulated by the hypervisor in a loop called \"entrance-emulation\"\n\
\n\
As for StratoVirt, we use KVM::run interface to VM-ENTRY, this returns when VM-EXIT\n\
"

sleep $short_pause

read -p"${RED}Question3${RESET}: How can we process the trapped PIO instructions(in, out instruction)?" ans

echo "-------------------------------------------------------------"

echo "$(sed -n "96,113p" src/main.rs)"

echo "-------------------------------------------------------------"

echo -e "\
Answer: We use println to perform serial output.
\n\
Your answer: $ans\n\
"

sleep $mid_pause
read -p"Enter next phase?[y]" ans

# Q4
git checkout mini_stratovirt_edu
git checkout 5f6ca750b05c0e5fe3dbfeb9f910b6c237eac1a4

clear
echo -e "\
In practice, a virtual machine often requires multiple memory regions, and memory \n\
needs to be allocated for interrupt controllers, PCI devices, and so on, thus requiring\n\
more flexible memory management\n\
"

echo "The memory layer in StratoVirt is here: [0,1M] for bootloader and [3G,4G] for devices"

echo -e "\
   ------------   \n\
   |          |   \n\
   |   mem    |   \n\
   |          |   \n\
   |          |   \n\
   ------------ 4G\n\
   |  devices |   \n\
   ------------ 3G\n\
   |          |   \n\
   |   mem    |   \n\
   |          |   \n\
   |          |   \n\
   ------------ 1M\n\
   |  loader  |   \n\
   ------------ 0 \n\
" 
read -p"${RED}Question4${RESET}: How to setup multiple memory mapping in KVM?" ans

echo -e "\
\n\
Let' s look at the code in StratoVirt\n\
"
echo "-------------------------------------------------------------"

echo "$(sed -n "33,48p" src/memory/guest_memory.rs)"

echo "-------------------------------------------------------------"

echo -e "Answer: build multiple region with different slots and use \`set_user_memory_region\` \n\
interface to update memory mapping in KVM\n\
"

sleep $mid_pause
read -p"Enter next phase?[y]" ans

#Q5
clear

echo -e "\
Now when StratoVirt traps mmio instructions, we will get the target guest address\n\
\n\
"

echo "-------------------------------------------------------------"

echo "$(sed -n "78,94p" src/main.rs)"

echo "-------------------------------------------------------------"

echo -e "\
\n\
"

read -p"${RED}Question5${RESET}: How can we find corresponding host target address?" ans

sleep $short_pause

echo -e "\
\n\
Answer: Find corresponding host address by compare guest address regions\n\
Let' s look at the code in StratoVirt:\n\
"
echo "-------------------------------------------------------------"

echo "$(sed -n "79,96p" src/memory/guest_memory.rs)"

echo "-------------------------------------------------------------"

echo ""
sleep $mid_pause

read -p"Enter next phase?[y]" ans

#Q6
clear

echo -e "\
Now we can trap sensitive instructions in kvm::run loop with its addr and data\n\
Then we can find the corresponding host address by \"find_host_mmap\" function\n\
Now we should emulate mmio read/write instructions\n\
\n\
"

sleep $short_pause

read -p"${RED}Question6${RESET}: How can we emulate a mmio read/write instruction?" ans

echo -e "\
\n\
Let' s look at the code in StratoVirt:\n\
"
echo "-------------------------------------------------------------"

echo "$(sed -n "98,120p" src/memory/guest_memory.rs)"

echo "-------------------------------------------------------------"

echo -e "\
Answer: Take mmio read as an example, the steps are:\n\
1. Use memory mapping and guest address to calculate host target address\n\
2. Read target address in binary format\n\
3. Move data to dst address\n\ 
\n\
"

sleep $mid_pause

read -p"Enter next phase?[y]" ans

#Q7
git checkout mini_stratovirt_edu
git checkout 651b538943adc6f5690755336aa314080604554a
clear

echo -e "\
Now we can manage memory fairly well, but there is no cpu management yet\n\
In Virtualization, some instructions may modify registers\n\
And storing registers info will support us to implement multi-CPU virtualization\n\
\n\
"

sleep $short_pause

read -p"${RED}Question7${RESET}: What CPU information should we save?" ans

sleep $short_pause

echo -e "\
\n\
Let' s look at the code in StratoVirt:\n\
"
echo "-------------------------------------------------------------"

echo "$(sed -n "19,28p" src/cpu/mod.rs)"

echo "-------------------------------------------------------------"

echo "$(sed -n "74,80p" src/main.rs)"

echo "-------------------------------------------------------------"


echo -e "\
Answer: we record regs and special regs:\n\
regs : rax-rdx, rsi, rsp, r8-r15, rip, rflags\n\
sregs: cs-ss(segment regs), ldt, gdt, idt, cr0-cr4, ect.
\n\
"

sleep $mid_pause
read -p"Enter next phase?[y]" ans

#Q8
git checkout cc0f2d91c0e12c349f72d275bce9240d8ae5684e
clear
echo -e "\
Now we have designed CPU and memory models, so than we can map multiple memory\n\
segments and store CPU registers to support multiple CPUs. However, We can only\n\
run some assembly code. If we want to boot a linux kernel, we still need to implement\n\
the linux boot protocol(https://www.kernel.org/doc/html/latest/x86/boot.html)\n\
\n\
"

echo -e "\
For example: a naive bootloader looks like this:\n\
---------------------------------------------------------------------------\n\
start addr    end addr    region              function                     \n\
---------------------------------------------------------------------------\n\
~             end         initrd                                           \n\
0x100000      ~           kernel setup        entrypoint of protect mode   \n\
0xf0000       0xfffff     BIOS boot block                                  \n\
0xa0000       0xeffff     VGA Graphic buffer                               \n\
0x9fc00       0x9ffff     MPtable                                          \n\
0x20000       0x9fbff     Kernel cmdline                                   \n\
0xb000        0x1f000     Page Directory Entry                             \n\
0xa000        0xafff      Page Directory Pointer                           \n\
0x9000        0x9fff      Page Map Level4     4-level page table info      \n\
0x7000        0x8fff      Zero Page           kernel cfg and hardware info \n\
0x0000        0x6fff      Interrupt Table     real-mode interrupt table    \n\
---------------------------------------------------------------------------\n\
The Zero Page is pretty important, which includes:
1. kernel header: magic number, ramdisk base and size, cmdline base and size\n\
2. e820 table: contains hardware memory layout\n\
\n\
"

sleep $short_pause

read -p"${RED}Question8${RESET}: How to build e820 table?" ans


echo "Answer: Let' s look at the code in StrtoVirt"

echo "-------------------------------------------------------------"

echo "$(sed -n "182,206p" src/boot_loader/bootparam.rs)"

echo "-------------------------------------------------------------"


sleep $mid_pause
read -p"Enter next phase?[y]" ans

#Q9
git checkout fbf5955d2774cbe22fb35450b63e98590c61be2e
clear 

echo -e "\
Now we can boot linux kernel, but we haven' t emulate a serial device\n\
We should emulate registers and red/write operations\n\
\n\
"

read -p"${RED}Question9${RESET}: How to emulate the write operation? " ans

sleep $short_pause

echo "Let' s look at the code in StratoVirt"

echo "-------------------------------------------------------------"

echo "$(sed -n "258,306p" src/device/serial.rs)"

echo "-------------------------------------------------------------"

echo -e "Answer: when we trap pio instructions, the instr will be passed to serial\n\
the serial will emulate the change of registers\n\
\n\
"

sleep $mid_pause
read -p"Enter next phase?[y]" ans


#Q10
git checkout 5323f696408eb48a4442d2cd638bd391fffa4120
clear

echo -e "\
Now we have serial device, but the linux kernel will fight you for control of your terminal\n\
This is because the granularity of the lock is too coarse\n\
We can use epoll to solve this problem\n\
\n\
"
sleep $short_pause

read -p"${RED}Question10${RESET}: How to use epoll? " ans

echo "Let' s look at the code in StratoVirt"

echo "-------------------------------------------------------------"

echo "$(sed -n "115,137p" src/device/serial.rs)"

echo "-------------------------------------------------------------"

echo "Answer: we use an event listener in another thread to inspect stdin event"

sleep $mid_pause

echo "${GREEN}Thanks!${RESET}"

git checkout mini_stratovirt_edu > /dev/null 2>&1







