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

static inline void pr_sep(void) {
	// pr_err(" ...........................................................................\n");
	pr_err("\n");
}

/* static vals */
unsigned long vaddr, paddr, pgd_idx, pud_idx, pmd_idx, pte_idx;
const char *PREFIXES[] = {"PGD", "PUD", "PMD", "PTE"};

/* static inline functions */
static inline void pr_pte(unsigned long address, unsigned long pte,
                          unsigned long i, int level) {
	if (level == 1)
		pr_cont(" NEXT_LVL_GPA(CR3)  =  ");
	else
		pr_cont(" NEXT_LVL_GPA(%s)  =  ", PREFIXES[level - 2]);
	pr_cont(PTE_PHYADDR_PATTREN, UL_TO_PTE_PHYADDR(address >> PAGE_SHIFT));
	pr_cont(" +  8 * %-3lu\n", i);
	pr_sep();

	pr_cont(" %-3lu: %s " PTE_PATTERN"\n", i, PREFIXES[level - 1], UL_TO_PTE(pte));
}

static inline void print_ptr_vaddr(volatile unsigned long *ptr) {
	unsigned long mask = ((1 << 9) - 1);

	vaddr = (unsigned long) ptr;
	pgd_idx = (vaddr >> 39) & mask;
	pud_idx = (vaddr >> 30) & mask;
	pmd_idx = (vaddr >> 21) & mask;
	pte_idx = (vaddr >> 12) & mask;
	pr_info(" GPT PGD index: %lu", pgd_idx);
	pr_info(" GPT PUD index: %lu", pud_idx);
	pr_info(" GPT PMD index: %lu", pmd_idx);
	pr_info(" GPT PTE index: %lu", pte_idx);

	pr_info("          %lu       %lu       %lu       %lu", pgd_idx, pud_idx, pmd_idx, pte_idx);
	pr_info(" GVA [PGD IDX] [PUD IDX] [PMD IDX] [PTE IDX] [  Offset  ]");
	pr_info(" GVA "VADDR_PATTERN"\n", UL_TO_VADDR(vaddr));
}

static inline void print_pa_check(unsigned long vaddr) {
	paddr = __pa(vaddr);
	pr_info(" GPA            =      " PADDR_PATTERN "\n", UL_TO_PADDR(paddr));
}

/* page table walker functions */
void dump_pgd(pgd_t *pgtable, int level);

void dump_pud(pud_t *pgtable, int level);

void dump_pmd(pmd_t *pgtable, int level);

void dump_pte(pte_t *pgtable, int level);

static int gpt_dump_init(void) {
	volatile unsigned long *ptr;

	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	*ptr = 1772333;
	printk("Value at GVA: %lu", ++*ptr);

	print_ptr_vaddr(ptr);
	dump_pgd(current->mm->pgd, 1);
	print_pa_check(vaddr);

	kvm_hypercall1(22, paddr);

	kfree((const void *) ptr);

	return 0;
}

static void gpt_dump_cleanup(void) {}

void dump_pgd(pgd_t *pgtable, int level) {
	pr_sep();

	unsigned long idx = pgd_idx;
	if (!(0 <= idx && idx < PTRS_PER_PTE))
		return;

	pgd_t pgd = pgtable[idx];
	if (pgd_val(pgd)) {
		if (pgd_leaf(pgd)) {
			pr_info("Large pgd detected! return");
		} else if (pgd_present(pgd)) {
			pr_pte(__pa(pgtable), pgd_val(pgd), idx, level);
			dump_pud((pud_t *) pgd_page_vaddr(pgd), level + 1);
		}
	}
}

void dump_pud(pud_t *pgtable, int level) {
	unsigned long idx = pud_idx;
	if (!(0 <= idx && idx < PTRS_PER_PUD))
		return;

	pud_t pud = pgtable[idx];
	if (pud_val(pud)) {
		if (pud_leaf(pud)) {
			pr_info("Large pud detected! return");
		} else if (pud_present(pud) && !pud_leaf(pud)) {
			pr_pte(__pa(pgtable), pud_val(pud), idx, level);
			dump_pmd(pud_pgtable(pud), level + 1);
		}
	}
}

void dump_pmd(pmd_t *pgtable, int level) {
	unsigned long idx = pmd_idx;
	if (!(0 <= idx && idx < PTRS_PER_PMD))
		return;

	pmd_t pmd = pgtable[idx];
	if (pmd_val(pmd)) {
		if (pmd_leaf(pmd)) {
			pr_info("Large pmd detected! return");
		} else if (pmd_present(pmd) && !pmd_leaf(pmd)) {
			pr_pte(__pa(pgtable), pmd_val(pmd), idx, level);
			dump_pte((pte_t *) pmd_page_vaddr(pmd), level + 1);
		}
	}
}

void dump_pte(pte_t *pgtable, int level) {
	unsigned long idx = pte_idx;
	if (!(0 <= idx && idx < PTRS_PER_PTE))
		return;

	pte_t pte = pgtable[idx];
	if (pte_val(pte)) {
		if (pte_present(pte)) {
			pr_pte(__pa(pgtable), pte_val(pte), idx, level);
		}
	}
}
module_init(gpt_dump_init);
module_exit(gpt_dump_cleanup);
