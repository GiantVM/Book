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
    pr_cont(" +  64 * %-3lu\n", i);
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

int init_module(void) {
    volatile unsigned long *ptr;
    int i;

    ptr = kmalloc(sizeof(int), GFP_KERNEL);
    for (i = 0; i < 1; ++i)
      ptr[i] = i*i;
    *ptr = 1772333;
    printk("Value at GVA: %lu", ++*ptr);

    print_ptr_vaddr(ptr);
    dump_pgd(current->mm->pgd, 1);
    print_pa_check(vaddr);

    kvm_hypercall1(22, paddr);
    for (i = 0; i < 1; ++i)
      ptr[i] = ptr[i] - 1;
    kfree((const void *) ptr);

    return -1;
}

void cleanup_module(void) {}

void dump_pgd(pgd_t *pgtable, int level) {
    unsigned long i;
    pgd_t pgd;
    pr_sep();


    for (i = 0; i < PTRS_PER_PGD; i++) {
        pgd = pgtable[i];

        if (pgd_val(pgd)) {
            if (i == pgd_idx) {
                if (pgd_large(pgd)) {
                    pr_info("Large pgd detected! return"); break;
                }
                if (pgd_present(pgd)) {

                    pr_pte(__pa(pgtable), pgd_val(pgd), i, level);

                    dump_pud((pud_t *) pgd_page_vaddr(pgd), level + 1);
                }
            }
        }
    }
}

void dump_pud(pud_t *pgtable, int level) {
    unsigned long i;
    pud_t pud;

    for (i = 0; i < PTRS_PER_PUD; i++) {
        pud = pgtable[i];

        if (pud_val(pud)) {
            if (i == pud_idx) {
                if (pud_large(pud)) {
                    pr_info("Large pud detected! return"); break;
                }
                if (pud_present(pud) && !pud_large(pud)) {

                    pr_pte(__pa(pgtable), pud_val(pud), i, level);

                    dump_pmd((pmd_t *) pud_page_vaddr(pud), level + 1);
                }
            }
        }
    }
}

void dump_pmd(pmd_t *pgtable, int level) {
    unsigned long i;
    pmd_t pmd;

    for (i = 0; i < PTRS_PER_PMD; i++) {
        pmd = pgtable[i];

        if (pmd_val(pmd)) {
            if (i == pmd_idx) {

                if (pmd_large(pmd)) {
                    pr_info("Large pmd detected! return"); break;
                }
                if (pmd_present(pmd) && !pmd_large(pmd)) {
                    pr_pte(__pa(pgtable), pmd_val(pmd), i, level);

                    dump_pte((pte_t *) pmd_page_vaddr(pmd), level + 1);
                }
            }
        }
    }
}

void dump_pte(pte_t *pgtable, int level) {
    unsigned long i;
    pte_t pte;

    for (i = 0; i < PTRS_PER_PTE; i++) {
        pte = pgtable[i];

        if (pte_val(pte)) {
            if (pte_present(pte)) {
                if (i == pte_idx)
                    pr_pte(__pa(pgtable), pte_val(pte), i, level);
            }
        }
    }
}
