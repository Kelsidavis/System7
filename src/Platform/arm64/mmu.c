/*
 * ARM64 MMU (Memory Management Unit)
 * Virtual memory setup for ARMv8-A
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mmu.h"

/* String function declaration */
extern void *memset(void *s, int c, size_t n);

/* Page table entry attributes */
#define PTE_VALID           (1ULL << 0)
#define PTE_TABLE           (1ULL << 1)
#define PTE_BLOCK           (0ULL << 1)
#define PTE_PAGE            (1ULL << 1)

/* Memory attributes */
#define PTE_ATTR_DEVICE_nGnRnE  (0ULL << 2)  /* Device memory */
#define PTE_ATTR_NORMAL_NC      (1ULL << 2)  /* Normal non-cacheable */
#define PTE_ATTR_NORMAL         (2ULL << 2)  /* Normal cacheable */

/* Access permissions */
#define PTE_AP_RW_EL1       (0ULL << 6)  /* Read/Write at EL1 */
#define PTE_AP_RW_ALL       (1ULL << 6)  /* Read/Write at all levels */
#define PTE_AP_RO_EL1       (2ULL << 6)  /* Read-only at EL1 */
#define PTE_AP_RO_ALL       (3ULL << 6)  /* Read-only at all levels */

/* Shareability */
#define PTE_SH_INNER        (3ULL << 8)  /* Inner shareable */

/* Access flag */
#define PTE_AF              (1ULL << 10)

/* Page sizes */
#define PAGE_SIZE_4K        4096
#define PAGE_SIZE_2M        (2 * 1024 * 1024)
#define PAGE_SIZE_1G        (1024 * 1024 * 1024)

/* Translation table levels */
#define TTB_L0_ENTRIES      512
#define TTB_L1_ENTRIES      512
#define TTB_L2_ENTRIES      512
#define TTB_L3_ENTRIES      512

/* Page table storage - 3 levels for 4KB granule
 * L1 table: 512 entries covering 512GB
 * L2 tables: 4 tables x 512 entries covering 2MB each
 * Total memory mapped: 4GB with 2MB granularity */
static uint64_t __attribute__((aligned(4096))) ttb_l1[TTB_L1_ENTRIES];
static uint64_t __attribute__((aligned(4096))) ttb_l2[4][TTB_L2_ENTRIES];

static bool mmu_initialized = false;

/*
 * Create a block entry for L1 (1GB blocks) or L2 (2MB blocks)
 */
static uint64_t mmu_create_block_entry(uint64_t addr, uint64_t attr) {
    return (addr & 0x0000FFFFFFE00000ULL) |  /* Address bits */
           attr |
           PTE_VALID |
           PTE_BLOCK |
           PTE_AF;
}

/*
 * Create a table entry pointing to next level
 */
static uint64_t mmu_create_table_entry(uint64_t table_addr) {
    return (table_addr & 0x0000FFFFFFFFF000ULL) |
           PTE_VALID |
           PTE_TABLE;
}

/*
 * Initialize MMU with identity mapping
 * Maps first 4GB of physical memory
 */
bool mmu_init(void) {
    if (mmu_initialized) return true;

    /* Clear all page tables */
    memset(ttb_l1, 0, sizeof(ttb_l1));
    memset(ttb_l2, 0, sizeof(ttb_l2));

    /* Set up L1 table with L2 table pointers for first 4GB */
    for (int i = 0; i < 4; i++) {
        ttb_l1[i] = mmu_create_table_entry((uint64_t)&ttb_l2[i]);
    }

    /* Map first 4GB with 2MB granularity for QEMU virt machine:
     * 0x00000000 - 0x07FFFFFF: Normal cacheable memory (128MB - Flash/ROM)
     * 0x08000000 - 0x3FFFFFFF: Device memory (896MB - Peripherals including UART)
     * 0x40000000 - 0x7FFFFFFF: Normal cacheable memory (1GB - Main RAM, kernel loaded here)
     * 0x80000000 - 0xFFFFFFFF: Normal cacheable memory (2GB - Additional RAM) */

    /* First 1GB broken down:
     * 0x00000000 - 0x07FFFFFF: Normal memory (first 64 x 2MB blocks)
     * 0x08000000 - 0x3FFFFFFF: Device memory (remaining 448 x 2MB blocks) */
    for (int i = 0; i < 64; i++) {
        uint64_t addr = (uint64_t)i * PAGE_SIZE_2M;
        uint64_t attr = PTE_ATTR_NORMAL | PTE_AP_RW_EL1 | PTE_SH_INNER;
        ttb_l2[0][i] = mmu_create_block_entry(addr, attr);
    }
    for (int i = 64; i < 512; i++) {
        uint64_t addr = (uint64_t)i * PAGE_SIZE_2M;
        uint64_t attr = PTE_ATTR_DEVICE_nGnRnE | PTE_AP_RW_EL1;
        ttb_l2[0][i] = mmu_create_block_entry(addr, attr);
    }

    /* Second 1GB: Normal memory (0x40000000 - 0x7FFFFFFF) - Main RAM */
    for (int i = 0; i < 512; i++) {
        uint64_t addr = 0x40000000ULL + (uint64_t)i * PAGE_SIZE_2M;
        uint64_t attr = PTE_ATTR_NORMAL | PTE_AP_RW_EL1 | PTE_SH_INNER;
        ttb_l2[1][i] = mmu_create_block_entry(addr, attr);
    }

    /* Third 1GB: Normal memory (0x80000000 - 0xBFFFFFFF) */
    for (int i = 0; i < 512; i++) {
        uint64_t addr = 0x80000000ULL + (uint64_t)i * PAGE_SIZE_2M;
        uint64_t attr = PTE_ATTR_NORMAL | PTE_AP_RW_EL1 | PTE_SH_INNER;
        ttb_l2[2][i] = mmu_create_block_entry(addr, attr);
    }

    /* Fourth 1GB: Normal memory (0xC0000000 - 0xFFFFFFFF) */
    for (int i = 0; i < 512; i++) {
        uint64_t addr = 0xC0000000ULL + (uint64_t)i * PAGE_SIZE_2M;
        uint64_t attr = PTE_ATTR_NORMAL | PTE_AP_RW_EL1 | PTE_SH_INNER;
        ttb_l2[3][i] = mmu_create_block_entry(addr, attr);
    }

    /* Map PCI ECAM region at 0x4010000000 using 1GB block entry
     * This covers 256GB-257GB range for PCI configuration space access */
    uint64_t pci_ecam_addr = 0x4000000000ULL;  /* 256GB */
    uint64_t pci_attr = PTE_ATTR_DEVICE_nGnRnE | PTE_AP_RW_EL1;
    ttb_l1[256] = mmu_create_block_entry(pci_ecam_addr, pci_attr);

    /* Set up MAIR (Memory Attribute Indirection Register) */
    uint64_t mair =
        (0x00ULL << 0)  |  /* Attr0: Device nGnRnE */
        (0x44ULL << 8)  |  /* Attr1: Normal non-cacheable */
        (0xFFULL << 16);   /* Attr2: Normal write-back cacheable */

    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));

    /* Set up TCR (Translation Control Register) for EL1
     * T0SZ = 25 (39-bit address space, L1 table as top level)
     * TG0 = 00 (4KB granule)
     * SH0 = 11 (Inner shareable)
     * ORGN0 = 01 (Normal, Outer write-back cacheable)
     * IRGN0 = 01 (Normal, Inner write-back cacheable) */
    uint64_t tcr =
        (25ULL << 0)  |    /* T0SZ: 39-bit VA */
        (0ULL << 14)  |    /* TG0: 4KB */
        (3ULL << 12)  |    /* SH0: Inner shareable */
        (1ULL << 10)  |    /* ORGN0: Write-back */
        (1ULL << 8)   |    /* IRGN0: Write-back */
        (25ULL << 16) |    /* T1SZ: 39-bit VA */
        (0ULL << 30)  |    /* TG1: 4KB */
        (2ULL << 32);      /* IPS: 010b = 40-bit PA (1TB) */

    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));

    /* Set TTBR0 (Translation Table Base Register) */
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)ttb_l1));

    /* Ensure all translations are complete */
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    mmu_initialized = true;
    return true;
}

/*
 * Enable MMU
 */
void mmu_enable(void) {
    if (!mmu_initialized) return;

    /* Invalidate TLB */
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    /* Read SCTLR_EL1 */
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));

    /* Enable MMU and caches */
    sctlr |= (1 << 0);   /* M: MMU enable */
    sctlr |= (1 << 2);   /* C: Data cache enable */
    sctlr |= (1 << 12);  /* I: Instruction cache enable */

    /* Write back SCTLR_EL1 */
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));

    /* Ensure MMU enable is complete */
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

/*
 * Disable MMU
 */
void mmu_disable(void) {
    /* Read SCTLR_EL1 */
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));

    /* Clear M bit to disable MMU */
    sctlr &= ~(1 << 0);

    /* Write back SCTLR_EL1 */
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));

    /* Ensure MMU disable is complete */
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

/*
 * Check if MMU is enabled
 */
bool mmu_is_enabled(void) {
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    return (sctlr & (1 << 0)) != 0;
}

/*
 * Check if MMU is initialized
 */
bool mmu_is_initialized(void) {
    return mmu_initialized;
}
