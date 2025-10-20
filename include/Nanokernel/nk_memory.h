/* nk_memory.h - System 7X Nanokernel Memory Manager (C23)
 *
 * A modern 64-bit physical memory manager and kernel heap for the
 * System 7X nanokernel layer. Designed for bare-metal x86-64 operation
 * with no dependencies on libc or operating system services.
 *
 * Features:
 * - Bitmap-based physical memory manager for 4KB pages
 * - Simple kernel heap with first-fit allocation
 * - Power-of-two alignment for optimal performance
 * - Supports memory sizes from 8MB to multiple terabytes
 * - Fully freestanding (no undefined behavior, no libc)
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 *   Constants
 * ============================================================ */

#define NK_PAGE_SIZE   4096u
#define NK_PAGE_SHIFT  12u
#define NK_PAGE_ALIGN(x) (((x) + NK_PAGE_SIZE - 1u) & ~(NK_PAGE_SIZE - 1u))

/* ============================================================
 *   Physical Memory Manager (PMM)
 * ============================================================ */

/**
 * Initialize the physical memory manager.
 *
 * @param mem_size_bytes Total physical memory available in bytes
 * @param phys_base      Physical address where usable memory starts
 *
 * The PMM will use the beginning of the memory region to store its
 * bitmap, which tracks allocated/free pages. Those pages are marked
 * as allocated to prevent reuse.
 */
void pmm_init(uint64_t mem_size_bytes, uintptr_t phys_base);

/**
 * Allocate a single 4KB page of physical memory.
 *
 * @return Physical address of allocated page, or nullptr if no memory available
 */
void *pmm_alloc_page(void);

/**
 * Free a previously allocated page.
 *
 * @param addr Physical address returned by pmm_alloc_page()
 */
void pmm_free_page(void *addr);

/**
 * Get total number of pages managed by PMM.
 *
 * @return Total page count
 */
uint64_t pmm_total_pages(void);

/**
 * Get number of free pages available for allocation.
 *
 * @return Free page count
 */
uint64_t pmm_free_pages(void);

/* ============================================================
 *   Kernel Heap
 * ============================================================ */

/**
 * Initialize the kernel heap allocator.
 *
 * @param heap_start Virtual/physical address where heap begins
 * @param heap_end   Virtual/physical address where heap ends
 *
 * The heap region is managed as a linked list of free blocks.
 * All allocations are page-aligned for simplicity and performance.
 */
void kheap_init(uintptr_t heap_start, uintptr_t heap_end);

/**
 * Allocate memory from kernel heap.
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or nullptr on failure
 *
 * All allocations are page-aligned. Requests are rounded up to
 * the nearest page boundary.
 */
void *kmalloc(size_t size);

/**
 * Free memory previously allocated by kmalloc.
 *
 * @param ptr Pointer returned by kmalloc(), or nullptr (no-op)
 */
void kfree(void *ptr);

/**
 * Resize a previously allocated memory block.
 *
 * @param ptr      Existing allocation, or nullptr (acts like kmalloc)
 * @param new_size New size in bytes, or 0 (acts like kfree)
 * @return Pointer to resized block, or nullptr on failure
 *
 * Data from the old block is preserved up to min(old_size, new_size).
 * If reallocation fails, the original block remains unchanged.
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * Allocate multiple contiguous pages directly from PMM.
 *
 * @param num_pages Number of contiguous 4KB pages to allocate
 * @return Pointer to first page, or nullptr on failure
 *
 * This is optimized for large allocations and bypasses the heap entirely.
 * Use kfree_pages() to free allocations made with this function.
 */
void *kmalloc_pages(size_t num_pages);

/**
 * Free pages allocated by kmalloc_pages().
 *
 * @param ptr       Pointer returned by kmalloc_pages()
 * @param num_pages Number of pages originally allocated
 */
void kfree_pages(void *ptr, size_t num_pages);

/* ============================================================
 *   Diagnostics
 * ============================================================ */

/**
 * Print memory statistics to kernel console.
 *
 * Requires nk_printf() to be available. Displays:
 * - Total and free page counts
 * - Memory usage in MiB
 */
void mem_print_stats(void);

/* ============================================================
 *   Status and Control
 * ============================================================ */

/**
 * Check if nanokernel memory manager is active.
 *
 * @return 1 if nanokernel is active and managing memory, 0 otherwise
 *
 * Used by system components to determine if they should use
 * nanokernel memory allocation vs classic allocation.
 */
int nk_is_active(void);

/* ============================================================
 *   Kernel Printf (must be provided externally)
 * ============================================================ */

/**
 * Kernel printf function - must be implemented elsewhere.
 *
 * Expected signature: void nk_printf(const char *fmt, ...);
 */
extern void nk_printf(const char *fmt, ...);
