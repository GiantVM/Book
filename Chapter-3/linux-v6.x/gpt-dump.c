#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/kvm_para.h>

MODULE_LICENSE("GPL");

/* convert to 0, 0, 1, 0, 1, 1, 0, ... */

// convert unsigned long to vaddr
#define BYTE_TO_BINARY(byte) \
  ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'),                    \
  ((byte) & 0x20 ? '1' : '0'), ((byte) & 0x10 ? '1' : '0'),                    \
  ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'),                    \
  ((byte) & 0x02 ? '1' : '0'), ((byte) & 0x01 ? '1' : '0')

#define TBYTE_TO_BINARY(tbyte)  \
  ((tbyte) & 0x04 ? '1' : '0'),    \
  ((tbyte) & 0x02 ? '1' : '0'), ((tbyte) & 0x01 ? '1' : '0')

#define UL_TO_PTE_OFFSET(ulong) \
  TBYTE_TO_BINARY((ulong) >> 9), TBYTE_TO_BINARY((ulong) >> 6), \
  TBYTE_TO_BINARY((ulong) >> 3), TBYTE_TO_BINARY((ulong))

#define UL_TO_PTE_INDEX(ulong) \
  TBYTE_TO_BINARY((ulong) >> 6), TBYTE_TO_BINARY((ulong) >> 3), TBYTE_TO_BINARY((ulong))

#define UL_TO_VADDR(ulong) \
  UL_TO_PTE_INDEX((ulong) >> 39), UL_TO_PTE_INDEX((ulong) >> 30), \
  UL_TO_PTE_INDEX((ulong) >> 21), UL_TO_PTE_INDEX((ulong) >> 12), \
  UL_TO_PTE_OFFSET((ulong))


// convert unsigned long to pte
#define UL_TO_PTE_PHYADDR(ulong) \
  BYTE_TO_BINARY((ulong) >> 32),    \
  BYTE_TO_BINARY((ulong) >> 24), BYTE_TO_BINARY((ulong) >> 16), \
  BYTE_TO_BINARY((ulong) >> 8), BYTE_TO_BINARY((ulong) >> 0)

#define UL_TO_PTE_IR(ulong) \
  UL_TO_PTE_OFFSET(ulong)

#define UL_TO_PTE(ulong) \
  UL_TO_PTE_IR((ulong) >> 52), UL_TO_PTE_PHYADDR((ulong) >> PAGE_SHIFT), UL_TO_PTE_OFFSET(ulong)

#define UL_TO_PADDR(ulong) \
  UL_TO_PTE_PHYADDR((ulong) >> PAGE_SHIFT), UL_TO_PTE_OFFSET(ulong)

/* printk pattern strings */

// convert unsigned long to vaddr
#define TBYTE_TO_BINARY_PATTERN   "%c%c%c"
#define BYTE_TO_BINARY_PATTERN    "%c%c%c%c%c%c%c%c"

#define PTE_INDEX_PATTERN \
  TBYTE_TO_BINARY_PATTERN TBYTE_TO_BINARY_PATTERN TBYTE_TO_BINARY_PATTERN " "

#define VADDR_OFFSET_PATTERN \
  TBYTE_TO_BINARY_PATTERN TBYTE_TO_BINARY_PATTERN \
  TBYTE_TO_BINARY_PATTERN TBYTE_TO_BINARY_PATTERN

#define VADDR_PATTERN \
  PTE_INDEX_PATTERN PTE_INDEX_PATTERN \
  PTE_INDEX_PATTERN PTE_INDEX_PATTERN \
  VADDR_OFFSET_PATTERN


// convert unsigned long to pte
// 40 bits
#define PTE_PHYADDR_PATTREN \
  BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN \
  BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN \
  BYTE_TO_BINARY_PATTERN " "

// 12 bits
#define PTE_IR_PATTERN \
  VADDR_OFFSET_PATTERN " "

// 12 + 40 + 12 bits
#define PTE_PATTERN \
  PTE_IR_PATTERN PTE_PHYADDR_PATTREN VADDR_OFFSET_PATTERN

#define PADDR_PATTERN \
  PTE_PHYADDR_PATTREN VADDR_OFFSET_PATTERN

static inline void pr_sep(void)
{
	// pr_err(" ...........................................................................\n");
	pr_err("\n");
}

/* static vals */
const char *PAGE_LEVEL[] = { "PGD", "PUD", "PMD", "PTE" };
enum { PGD, PUD, PMD, PTE };

/* static inline functions */
static inline void pr_pte(unsigned long address, unsigned long pte,
			  unsigned long i, int level)
{
	if (level == 0)
		pr_cont(" NEXT_LVL_GPA(CR3)  =  ");
	else
		pr_cont(" NEXT_LVL_GPA(%s)  =  ", PAGE_LEVEL[level - 1]);
	pr_cont(PTE_PHYADDR_PATTREN, UL_TO_PTE_PHYADDR(address >> PAGE_SHIFT));
	pr_cont(" +  8 * %-3lu\n", i);
	pr_sep();

	pr_cont(" %-3lu: %s " PTE_PATTERN "\n", i, PAGE_LEVEL[level],
		UL_TO_PTE(pte));
}

static inline void print_ptr_vaddr(volatile unsigned long *ptr,
				   unsigned long idx[4])
{
	unsigned long mask = ((1 << 9) - 1);

	unsigned long vaddr = (unsigned long)ptr;
	for (int i = 0; i < 4; ++i) {
		idx[i] = (vaddr >> (39 - 9 * i)) & mask;
		pr_info(" GPT %s index: %lu", PAGE_LEVEL[i], idx[i]);
	}

	pr_info("          %lu       %lu       %lu       %lu",
		       idx[PGD], idx[PUD], idx[PMD], idx[PTE]);
	pr_info(" GVA [PGD IDX] [PUD IDX] [PMD IDX] [PTE IDX] [  Offset  ]");
	pr_info(" GVA " VADDR_PATTERN "\n", UL_TO_VADDR(vaddr));
}

static inline void print_pa_check(unsigned long vaddr, unsigned long *paddr)
{
	*paddr = __pa(vaddr);
	pr_info(" GPA            =      " PADDR_PATTERN "\n",
		UL_TO_PADDR(*paddr));
}

/* page table walker functions */
static void dump_pgd(pgd_t *pgtable, const unsigned long idx[4]);

static void dump_pud(pud_t *pgtable, const unsigned long idx[4]);

static void dump_pmd(pmd_t *pgtable, const unsigned long idx[4]);

static void dump_pte(pte_t *pgtable, const unsigned long idx[4]);

static int gpt_dump_init(void)
{
	volatile unsigned long *ptr;

	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	*ptr = 1772334;
	printk("Value at GVA: %lu", *ptr);

	unsigned long idx[4];
	print_ptr_vaddr(ptr, idx);
	pr_sep();

	dump_pgd(current->mm->pgd, idx);
	unsigned long paddr;
	print_pa_check((unsigned long)ptr, &paddr);

	kvm_hypercall1(22, paddr);

	kfree((const void *)ptr);

	return 0;
}

static void gpt_dump_cleanup(void) {}

#define define_dump(level, this_lvl, next_lvl, vaddr_func)                   \
static void dump_##this_lvl(this_lvl##_t *pgtable,                           \
			    const unsigned long idx[4])                      \
{                                                                            \
	unsigned long i = idx[level];				             \
	if (!(0 <= i && i < PTRS_PER_PTE)) {                                 \
		return;                                                      \
	}                                                                    \
	this_lvl##_t entry = pgtable[i];                                     \
	if (this_lvl##_val(entry)) {                                         \
		if (this_lvl##_leaf(entry)) {                                \
			pr_info("Large %s detected! return",                 \
				PAGE_LEVEL[level]);                          \
		} else if (this_lvl##_present(entry)) {                      \
			pr_pte(__pa(pgtable), this_lvl##_val(entry),         \
			       i, level);                                    \
			dump_##next_lvl(                                     \
				(next_lvl##_t *)vaddr_func(entry),           \
				idx);                                        \
		}                                                            \
	}                                                                    \
}

define_dump(PGD, pgd, pud, pgd_page_vaddr)
define_dump(PUD, pud, pmd, pud_pgtable)
define_dump(PMD, pmd, pte, pmd_page_vaddr)

static void dump_pte(pte_t *pgtable, const unsigned long idx[4])
{
	unsigned long i = idx[PTE];
	if (!(0 <= i && i < PTRS_PER_PTE))
		return;

	pte_t pte = pgtable[i];
	if (pte_val(pte)) {
		if (pte_present(pte)) {
			pr_pte(__pa(pgtable), pte_val(pte), i, PTE);
		}
	}
}
module_init(gpt_dump_init);
module_exit(gpt_dump_cleanup);
