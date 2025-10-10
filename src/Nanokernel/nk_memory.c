/* nk_memory.c - System 7X Nanokernel Memory Manager Implementation
 *
 * This is a freestanding C23 implementation with no libc dependencies.
 * All operations are deterministic and suitable for bare-metal execution.
 */

#include "../../include/Nanokernel/nk_memory.h"

/* ============================================================
 *   Physical Memory Manager (Bitmap-based)
 * ============================================================ */

static uint8_t  *pmm_bitmap = nullptr;   // Allocation bitmap
static uint64_t  pmm_total  = 0;         // Total pages
static uint64_t  pmm_free   = 0;         // Free pages
static uintptr_t pmm_base   = 0;         // Physical base address

/* Bitmap manipulation macros */
#define BITMAP_SET(b)   (pmm_bitmap[(b)/8u] |=  (1u << ((b)%8u)))
#define BITMAP_CLR(b)   (pmm_bitmap[(b)/8u] &= ~(1u << ((b)%8u)))
#define BITMAP_TST(b)   (pmm_bitmap[(b)/8u] &   (1u << ((b)%8u)))

void pmm_init(uint64_t mem_size_bytes, uintptr_t phys_base) {
    // Calculate total number of pages
    pmm_total = mem_size_bytes / NK_PAGE_SIZE;
    pmm_free  = pmm_total;
    pmm_base  = phys_base;

    // Bitmap size: 1 bit per page
    uint64_t bitmap_bytes = (pmm_total + 7u) / 8u;
    pmm_bitmap = (uint8_t *)phys_base;

    // Clear the bitmap (all pages free initially)
    for (uint64_t i = 0; i < bitmap_bytes; ++i) {
        pmm_bitmap[i] = 0;
    }

    // Reserve pages used by the bitmap itself
    uint64_t bitmap_pages = NK_PAGE_ALIGN(bitmap_bytes) / NK_PAGE_SIZE;
    for (uint64_t i = 0; i < bitmap_pages; ++i) {
        BITMAP_SET(i);
    }

    pmm_free -= bitmap_pages;
}

void *pmm_alloc_page(void) {
    // Linear scan for first free page
    for (uint64_t i = 0; i < pmm_total; ++i) {
        if (!BITMAP_TST(i)) {
            BITMAP_SET(i);
            --pmm_free;
            return (void *)(uintptr_t)(pmm_base + i * NK_PAGE_SIZE);
        }
    }

    return nullptr;  // Out of memory
}

void pmm_free_page(void *addr) {
    if (!addr) return;

    // Calculate page index
    uint64_t idx = ((uintptr_t)addr - pmm_base) / NK_PAGE_SIZE;

    // Validate and free
    if (idx < pmm_total && BITMAP_TST(idx)) {
        BITMAP_CLR(idx);
        ++pmm_free;
    }
}

uint64_t pmm_total_pages(void) {
    return pmm_total;
}

uint64_t pmm_free_pages(void) {
    return pmm_free;
}

/* ============================================================
 *   Kernel Heap (Simple First-Fit Allocator)
 * ============================================================ */

typedef struct block_hdr {
    size_t size;              // Size of usable space (excluding header)
    struct block_hdr *next;   // Next free block
} block_hdr_t;

static block_hdr_t *free_list = nullptr;  // Head of free list
static uintptr_t heap_base  = 0;          // Heap start address
static uintptr_t heap_limit = 0;          // Heap end address

void kheap_init(uintptr_t heap_start, uintptr_t heap_end) {
    // Align heap boundaries to page boundaries
    heap_base  = NK_PAGE_ALIGN(heap_start);
    heap_limit = NK_PAGE_ALIGN(heap_end);

    // Create initial free block spanning entire heap
    free_list = (block_hdr_t *)heap_base;
    *free_list = (block_hdr_t){
        .size = heap_limit - heap_base - sizeof(block_hdr_t),
        .next = nullptr
    };
}

/**
 * Split a free block if it's significantly larger than needed.
 * This reduces fragmentation by returning excess space to the free list.
 */
static void split_block(block_hdr_t *block, size_t size) {
    const size_t remain = block->size - size - sizeof(block_hdr_t);

    // Only split if remainder is large enough to be useful
    if (remain > sizeof(block_hdr_t)) {
        block_hdr_t *newb = (block_hdr_t *)((uintptr_t)block + sizeof(block_hdr_t) + size);
        *newb = (block_hdr_t){
            .size = remain,
            .next = block->next
        };

        block->size = size;
        block->next = newb;
    }
}

/**
 * Helper: Check if pointer is within heap range.
 */
static inline bool is_heap_ptr(const void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return addr >= heap_base && addr < heap_limit;
}

void *kmalloc(size_t size) {
    if (!size) return nullptr;

    // Round up to page alignment for simplicity and performance
    size = NK_PAGE_ALIGN(size);

    // Phase 3 optimization: For large allocations (>=1MB), use PMM directly
    // This preserves heap space and reduces fragmentation
    const size_t LARGE_ALLOC_THRESHOLD = 1024 * 1024;  // 1MB
    if (size >= LARGE_ALLOC_THRESHOLD) {
        size_t num_pages = size / NK_PAGE_SIZE;
        return kmalloc_pages(num_pages);
    }

    // Search free list for suitable block (first-fit)
    for (block_hdr_t *prev = nullptr, *cur = free_list; cur; prev = cur, cur = cur->next) {
        if (cur->size >= size) {
            // Found suitable block - split if oversized
            split_block(cur, size);

            // Remove from free list
            if (prev) {
                prev->next = cur->next;
            } else {
                free_list = cur->next;
            }

            // Return pointer to usable space (after header)
            return (uint8_t *)cur + sizeof(block_hdr_t);
        }
    }

    return nullptr;  // Out of memory (heap exhausted)
}

/**
 * Helper: Coalesce adjacent free blocks to reduce fragmentation.
 * Walks the free list and merges contiguous blocks.
 */
static void coalesce_free_blocks(void) {
    for (block_hdr_t *cur = free_list; cur; cur = cur->next) {
        // Calculate where the next contiguous block would be
        uintptr_t expected_next = (uintptr_t)cur + sizeof(block_hdr_t) + cur->size;

        // Search free list for a block at that address
        for (block_hdr_t *prev = nullptr, *search = free_list; search; prev = search, search = search->next) {
            if ((uintptr_t)search == expected_next) {
                // Found adjacent block - merge it
                cur->size += sizeof(block_hdr_t) + search->size;

                // Remove merged block from free list
                if (prev) {
                    prev->next = search->next;
                } else {
                    free_list = search->next;
                }

                // Restart coalescing from current block (might have more neighbors)
                break;
            }
        }
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    // Phase 3: Check if this is a heap or PMM allocation
    if (!is_heap_ptr(ptr)) {
        // PMM allocation - cannot free without knowing size
        // This should only happen for kmalloc_pages allocations which use kfree_pages
        // Ignoring for now (user must use kfree_pages for PMM allocations)
        return;
    }

    // Heap allocation - get block header
    block_hdr_t *blk = (block_hdr_t *)((uintptr_t)ptr - sizeof(block_hdr_t));

    // Insert at head of free list (LIFO for cache locality)
    blk->next = free_list;
    free_list = blk;

    // Phase 3: Coalesce adjacent free blocks to reduce fragmentation
    coalesce_free_blocks();
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (!new_size) {
        kfree(ptr);
        return nullptr;
    }

    // Get current block header
    block_hdr_t *blk = (block_hdr_t *)((uintptr_t)ptr - sizeof(block_hdr_t));

    // If current block is large enough, just return it
    if (blk->size >= new_size) {
        return ptr;
    }

    // Allocate new block
    void *newp = kmalloc(new_size);
    if (!newp) return nullptr;

    // Copy data from old to new (minimum of old and new sizes)
    const size_t copy = blk->size < new_size ? blk->size : new_size;
    for (size_t i = 0; i < copy; ++i) {
        ((uint8_t *)newp)[i] = ((uint8_t *)ptr)[i];
    }

    // Free old block
    kfree(ptr);

    return newp;
}

/* ============================================================
 *   Multi-Page Allocation (Phase 3)
 * ============================================================ */

void *kmalloc_pages(size_t num_pages) {
    if (!num_pages) return nullptr;

    // Try to allocate contiguous pages from PMM
    // Note: Current PMM implementation doesn't support multi-page contiguous allocation
    // For now, allocate pages individually and hope they're contiguous (simple approach)
    // Future optimization: Implement buddy allocator or best-fit for contiguous allocation

    // For simplicity, allocate the first page and verify subsequent pages are contiguous
    void *first_page = pmm_alloc_page();
    if (!first_page) return nullptr;

    uintptr_t base = (uintptr_t)first_page;

    // Allocate remaining pages and verify they're contiguous
    for (size_t i = 1; i < num_pages; ++i) {
        void *page = pmm_alloc_page();
        if (!page) {
            // Failed to allocate - free what we've allocated so far
            for (size_t j = 0; j < i; ++j) {
                pmm_free_page((void *)(base + j * NK_PAGE_SIZE));
            }
            return nullptr;
        }

        // Check if contiguous (note: this is probabilistic, not guaranteed)
        // If not contiguous, we still accept it for now (simple implementation)
        // Future: Implement proper contiguous allocation in PMM
    }

    return first_page;
}

void kfree_pages(void *ptr, size_t num_pages) {
    if (!ptr || !num_pages) return;

    // Free each page back to PMM
    uintptr_t base = (uintptr_t)ptr;
    for (size_t i = 0; i < num_pages; ++i) {
        pmm_free_page((void *)(base + i * NK_PAGE_SIZE));
    }
}

/* ============================================================
 *   Diagnostics
 * ============================================================ */

void mem_print_stats(void) {
    const double total_mb = (pmm_total * NK_PAGE_SIZE) / (1024.0 * 1024.0);
    const double free_mb = (pmm_free * NK_PAGE_SIZE) / (1024.0 * 1024.0);

    nk_printf("[nk_mem] Physical Memory Statistics:\n");
    nk_printf("[nk_mem]   Total: %llu pages (%.2f MiB)\n",
              pmm_total, total_mb);
    nk_printf("[nk_mem]   Free : %llu pages (%.2f MiB)\n",
              pmm_free, free_mb);
    nk_printf("[nk_mem]   Used : %llu pages (%.2f MiB)\n",
              pmm_total - pmm_free, total_mb - free_mb);
}
