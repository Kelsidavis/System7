/* nk_memory_test.c - Test harness for nanokernel memory manager
 *
 * This demonstrates the C23 memory manager's capabilities and validates
 * its correctness on various allocation patterns.
 */

#include "../../include/Nanokernel/nk_memory.h"
#include <stdarg.h>
#include <stdbool.h>

/* ============================================================
 *   Test Infrastructure
 * ============================================================ */

static uint32_t tests_run = 0;
static uint32_t tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        nk_printf("\n[TEST] %s\n", #name); \
        tests_run++; \
        test_##name(); \
    } \
    static void test_##name(void)

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            nk_printf("  [FAIL] Assertion failed: %s (line %d)\n", #cond, __LINE__); \
            return; \
        } \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        nk_printf("  [PASS]\n"); \
    } while (0)

/* ============================================================
 *   Minimal printf implementation for testing
 * ============================================================ */

extern void serial_putchar(char c);
extern void serial_puts(const char *s);

void nk_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    const char *p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'l': {
                    if (p[1] == 'l' && p[2] == 'u') {
                        // %llu - unsigned long long
                        p += 2;
                        uint64_t val = va_arg(args, uint64_t);

                        // Simple decimal conversion
                        if (val == 0) {
                            serial_putchar('0');
                        } else {
                            char buf[32];
                            int i = 0;
                            while (val > 0) {
                                buf[i++] = '0' + (val % 10);
                                val /= 10;
                            }
                            while (i > 0) {
                                serial_putchar(buf[--i]);
                            }
                        }
                    }
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    serial_puts(s);
                    break;
                }
                case 'u': {
                    uint32_t val = va_arg(args, uint32_t);

                    if (val == 0) {
                        serial_putchar('0');
                    } else {
                        char buf[16];
                        int i = 0;
                        while (val > 0) {
                            buf[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            serial_putchar(buf[--i]);
                        }
                    }
                    break;
                }
                case '.': {
                    // %.2f - floating point (simplified)
                    if (p[1] == '2' && p[2] == 'f') {
                        p += 2;
                        double val = va_arg(args, double);

                        // Print integer part
                        uint64_t int_part = (uint64_t)val;
                        if (int_part == 0) {
                            serial_putchar('0');
                        } else {
                            char buf[32];
                            int i = 0;
                            while (int_part > 0) {
                                buf[i++] = '0' + (int_part % 10);
                                int_part /= 10;
                            }
                            while (i > 0) {
                                serial_putchar(buf[--i]);
                            }
                        }

                        // Print decimal part
                        serial_putchar('.');
                        double frac = val - (uint64_t)val;
                        int dec1 = (int)(frac * 10) % 10;
                        int dec2 = (int)(frac * 100) % 10;
                        serial_putchar('0' + dec1);
                        serial_putchar('0' + dec2);
                    }
                    break;
                }
                case '%': {
                    serial_putchar('%');
                    break;
                }
            }
            p++;
        } else {
            serial_putchar(*p);
            p++;
        }
    }

    va_end(args);
}

/* ============================================================
 *   Test Cases
 * ============================================================ */

TEST(pmm_basic) {
    uint64_t total = pmm_total_pages();
    uint64_t free_before = pmm_free_pages();

    nk_printf("  Total pages: %llu\n", total);
    nk_printf("  Free before: %llu\n", free_before);

    ASSERT(total > 0);
    ASSERT(free_before > 0);
    ASSERT(free_before <= total);

    PASS();
}

TEST(pmm_alloc_free) {
    uint64_t free_before = pmm_free_pages();

    // Allocate a page
    void *page = pmm_alloc_page();
    ASSERT(page != nullptr);

    uint64_t free_after = pmm_free_pages();
    ASSERT(free_after == free_before - 1);

    // Free the page
    pmm_free_page(page);

    uint64_t free_final = pmm_free_pages();
    ASSERT(free_final == free_before);

    PASS();
}

TEST(kheap_basic) {
    // Allocate small block
    void *ptr1 = kmalloc(64);
    ASSERT(ptr1 != nullptr);

    // Allocate larger block
    void *ptr2 = kmalloc(1024);
    ASSERT(ptr2 != nullptr);
    ASSERT(ptr2 != ptr1);

    // Free both
    kfree(ptr1);
    kfree(ptr2);

    PASS();
}

TEST(kheap_realloc) {
    // Allocate initial block
    void *ptr = kmalloc(128);
    ASSERT(ptr != nullptr);

    // Write pattern to verify data preservation
    for (size_t i = 0; i < 128; ++i) {
        ((uint8_t *)ptr)[i] = (uint8_t)(i & 0xFF);
    }

    // Grow block
    void *new_ptr = krealloc(ptr, 256);
    ASSERT(new_ptr != nullptr);

    // Verify data preserved
    for (size_t i = 0; i < 128; ++i) {
        ASSERT(((uint8_t *)new_ptr)[i] == (uint8_t)(i & 0xFF));
    }

    // Shrink block
    void *small_ptr = krealloc(new_ptr, 64);
    ASSERT(small_ptr != nullptr);

    // Verify partial data preserved
    for (size_t i = 0; i < 64; ++i) {
        ASSERT(((uint8_t *)small_ptr)[i] == (uint8_t)(i & 0xFF));
    }

    kfree(small_ptr);

    PASS();
}

TEST(kheap_stress) {
    #define NUM_ALLOCS 100
    void *ptrs[NUM_ALLOCS];

    // Allocate many blocks
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        ptrs[i] = kmalloc(NK_PAGE_SIZE);
        ASSERT(ptrs[i] != nullptr);
    }

    // Free them all
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        kfree(ptrs[i]);
    }

    PASS();
}

/* ============================================================
 *   Test Runner
 * ============================================================ */

void nk_memory_run_tests(void) {
    nk_printf("\n");
    nk_printf("========================================\n");
    nk_printf("  Nanokernel Memory Manager Test Suite\n");
    nk_printf("========================================\n");

    // Run all tests
    run_test_pmm_basic();
    run_test_pmm_alloc_free();
    run_test_kheap_basic();
    run_test_kheap_realloc();
    run_test_kheap_stress();

    // Print summary
    nk_printf("\n");
    nk_printf("========================================\n");
    nk_printf("  Test Results: %u/%u passed\n", tests_passed, tests_run);
    nk_printf("========================================\n");
    nk_printf("\n");

    // Print final statistics
    mem_print_stats();
}

/* ============================================================
 *   Example Usage (can be called from kernel_main)
 * ============================================================ */

void nk_memory_example(void) {
    nk_printf("\n=== Nanokernel Memory Manager Example ===\n\n");

    // Initialize with 8 GiB of memory starting at 1 MB
    const uint64_t memsize = 8ull * 1024 * 1024 * 1024;  // 8 GiB
    const uintptr_t phys_base = 0x100000;                 // 1 MB

    nk_printf("Initializing PMM with %llu MiB...\n", memsize / (1024 * 1024));
    pmm_init(memsize, phys_base);

    nk_printf("Initializing kernel heap...\n");
    kheap_init(0x200000, 0x800000);  // 6 MB heap

    nk_printf("\nInitial state:\n");
    mem_print_stats();

    nk_printf("\nAllocating 1 MiB...\n");
    void *a = kmalloc(1 << 20);

    nk_printf("Allocating 2 MiB...\n");
    void *b = kmalloc(2 << 20);

    nk_printf("\nAfter allocations:\n");
    mem_print_stats();

    nk_printf("\nFreeing allocations...\n");
    kfree(a);
    kfree(b);

    nk_printf("\nAfter freeing:\n");
    mem_print_stats();

    nk_printf("\n=== Example complete ===\n\n");
}
