/*
 * ARM64 Cache Management
 * Data and instruction cache operations for ARMv8-A
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cache.h"

/*
 * Data cache clean by virtual address
 * Writes dirty cache lines back to memory
 */
void dcache_clean_range(void *start, size_t length) {
    uint64_t addr = (uint64_t)start;
    uint64_t end = addr + length;

    /* Get cache line size */
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t dminline = (ctr >> 16) & 0xF;
    uint32_t cache_line_size = 4 << dminline;

    /* Align to cache line boundary */
    addr &= ~(cache_line_size - 1);

    /* Clean each cache line */
    while (addr < end) {
        __asm__ volatile("dc cvac, %0" :: "r"(addr) : "memory");
        addr += cache_line_size;
    }

    /* Data Synchronization Barrier */
    __asm__ volatile("dsb sy" ::: "memory");
}

/*
 * Data cache invalidate by virtual address
 * Marks cache lines as invalid, forcing reload from memory
 */
void dcache_invalidate_range(void *start, size_t length) {
    uint64_t addr = (uint64_t)start;
    uint64_t end = addr + length;

    /* Get cache line size */
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t dminline = (ctr >> 16) & 0xF;
    uint32_t cache_line_size = 4 << dminline;

    /* Align to cache line boundary */
    addr &= ~(cache_line_size - 1);

    /* Invalidate each cache line */
    while (addr < end) {
        __asm__ volatile("dc ivac, %0" :: "r"(addr) : "memory");
        addr += cache_line_size;
    }

    /* Data Synchronization Barrier */
    __asm__ volatile("dsb sy" ::: "memory");
}

/*
 * Data cache clean and invalidate by virtual address
 * Writes dirty lines back to memory then invalidates
 */
void dcache_flush_range(void *start, size_t length) {
    uint64_t addr = (uint64_t)start;
    uint64_t end = addr + length;

    /* Get cache line size */
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t dminline = (ctr >> 16) & 0xF;
    uint32_t cache_line_size = 4 << dminline;

    /* Align to cache line boundary */
    addr &= ~(cache_line_size - 1);

    /* Clean and invalidate each cache line */
    while (addr < end) {
        __asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
        addr += cache_line_size;
    }

    /* Data Synchronization Barrier */
    __asm__ volatile("dsb sy" ::: "memory");
}

/*
 * Instruction cache invalidate all
 * Should be called after code modification
 */
void icache_invalidate_all(void) {
    __asm__ volatile("ic iallu" ::: "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

/*
 * Instruction cache invalidate by virtual address range
 */
void icache_invalidate_range(void *start, size_t length) {
    uint64_t addr = (uint64_t)start;
    uint64_t end = addr + length;

    /* Get cache line size */
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t iminline = ctr & 0xF;
    uint32_t cache_line_size = 4 << iminline;

    /* Align to cache line boundary */
    addr &= ~(cache_line_size - 1);

    /* Invalidate instruction cache for each line */
    while (addr < end) {
        __asm__ volatile("ic ivau, %0" :: "r"(addr) : "memory");
        addr += cache_line_size;
    }

    /* Data Synchronization Barrier */
    __asm__ volatile("dsb sy" ::: "memory");
    /* Instruction Synchronization Barrier */
    __asm__ volatile("isb" ::: "memory");
}

/*
 * Synchronize instruction and data caches
 * Use after code modification to ensure visibility
 */
void cache_sync_code(void *start, size_t length) {
    /* Clean data cache to ensure writes are visible */
    dcache_clean_range(start, length);

    /* Invalidate instruction cache to reload instructions */
    icache_invalidate_range(start, length);
}

/*
 * Get data cache line size
 */
uint32_t dcache_get_line_size(void) {
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t dminline = (ctr >> 16) & 0xF;
    return 4 << dminline;
}

/*
 * Get instruction cache line size
 */
uint32_t icache_get_line_size(void) {
    uint64_t ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uint32_t iminline = ctr & 0xF;
    return 4 << iminline;
}

/*
 * Data Memory Barrier
 */
void dmb(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}

/*
 * Data Synchronization Barrier
 */
void dsb(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

/*
 * Instruction Synchronization Barrier
 */
void isb(void) {
    __asm__ volatile("isb" ::: "memory");
}
