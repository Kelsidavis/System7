/*
 * Memory Telemetry Implementation
 * System 7.1 Portable
 */

#include "MemoryMgr/MemoryTelemetry.h"
#include "System/Panic.h"
#include <string.h>

/* Forward declarations */
extern void serial_puts(const char* str);
extern void serial_putchar(char c);
extern uint64_t nk_get_ticks(void);

static inline uint32_t get_tick_count(void) {
    return (uint32_t)nk_get_ticks();
}

/* Allocation tracking database */
static AllocationRecord g_allocations[TELEMETRY_MAX_ALLOCATIONS];
static uint32_t g_next_alloc_id = 1;
static bool g_telemetry_initialized = false;

/* Statistics */
static TelemetryStats g_stats;

/* === Helper Functions === */

static void print_hex32(uint32_t val) {
    const char hex[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_putchar(hex[(val >> i) & 0xF]);
    }
}

static void print_decimal(uint32_t val) {
    if (val == 0) {
        serial_putchar('0');
        return;
    }
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

/* === Initialization === */

void telemetry_init(void) {
    if (g_telemetry_initialized) return;

    memset(g_allocations, 0, sizeof(g_allocations));
    memset(&g_stats, 0, sizeof(g_stats));
    g_next_alloc_id = 1;
    g_telemetry_initialized = true;

    serial_puts("[TELEMETRY] Memory telemetry system initialized\n");
}

/* === Allocation Tracking === */

uint32_t telemetry_record_alloc(void* ptr, uint32_t size, bool is_handle,
                                 const char* file, uint32_t line,
                                 const char* func) {
    if (!g_telemetry_initialized) telemetry_init();
    if (!ptr) return 0;

    /* Find free slot */
    uint32_t slot = 0xFFFFFFFF;
    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS; i++) {
        if (!g_allocations[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == 0xFFFFFFFF) {
        serial_puts("[TELEMETRY] WARNING: Allocation tracking table full!\n");
        return 0;
    }

    /* Record allocation */
    AllocationRecord* rec = &g_allocations[slot];
    rec->pointer = ptr;
    rec->size = size;
    rec->is_handle = is_handle;
    rec->active = true;
    rec->alloc_id = g_next_alloc_id++;
    rec->free_timestamp = 0;

    rec->info.file = file;
    rec->info.function = func;
    rec->info.line = line;
    rec->info.size = size;
    rec->info.timestamp = get_tick_count();

    /* Update statistics */
    g_stats.total_allocations++;
    g_stats.active_allocations++;
    g_stats.active_bytes += size;
    g_stats.lifetime_allocations++;
    g_stats.lifetime_bytes += size;

    if (g_stats.active_allocations > g_stats.peak_allocations) {
        g_stats.peak_allocations = g_stats.active_allocations;
    }
    if (g_stats.active_bytes > g_stats.peak_bytes) {
        g_stats.peak_bytes = g_stats.active_bytes;
    }

    return rec->alloc_id;
}

bool telemetry_record_free(void* ptr) {
    if (!g_telemetry_initialized) return false;
    if (!ptr) return true;  /* Freeing NULL is valid */

    /* Find allocation */
    AllocationRecord* rec = NULL;
    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS; i++) {
        if (g_allocations[i].active && g_allocations[i].pointer == ptr) {
            rec = &g_allocations[i];
            break;
        }
    }

    if (!rec) {
        serial_puts("[TELEMETRY] ERROR: Free of untracked pointer ");
        print_hex32((uint32_t)(uintptr_t)ptr);
        serial_puts("\n");
        g_stats.invalid_frees++;
        return false;
    }

    /* Check for double-free */
    if (!rec->active) {
        serial_puts("[TELEMETRY] ERROR: Double-free detected at ");
        print_hex32((uint32_t)(uintptr_t)ptr);
        serial_puts("\n");
        serial_puts("  Originally allocated at ");
        serial_puts(rec->info.file);
        serial_puts(":");
        print_decimal(rec->info.line);
        serial_puts("\n");
        g_stats.double_frees++;

        KERNEL_PANIC(PANIC_CODE_DOUBLE_FREE, "Double-free detected by telemetry");
        return false;
    }

    /* Mark as freed */
    rec->active = false;
    rec->free_timestamp = get_tick_count();

    /* Update statistics */
    g_stats.total_frees++;
    g_stats.active_allocations--;
    if (rec->size <= g_stats.active_bytes) {
        g_stats.active_bytes -= rec->size;
    } else {
        serial_puts("[TELEMETRY] WARNING: Stats underflow on free\n");
        g_stats.active_bytes = 0;
    }

    return true;
}

/* === Canary Management === */

void telemetry_install_canaries(void* block_ptr, uint32_t user_size) {
#if TELEMETRY_USE_CANARIES
    if (!block_ptr) return;

    /* Assuming block layout:
     * [BlockHeader][CANARY_PREFIX][user_data][CANARY_SUFFIX]
     *
     * For simplicity, we write canaries directly before and after user data.
     * The block_ptr should point to the start of the block header.
     * User data starts after the header.
     */

    /* This is a simplified implementation. In practice, you'd need to know
     * the exact layout of BlockHeader and adjust accordingly. */

    /* For now, we'll store canary info in the telemetry record itself
     * and perform validation during check_canaries() */
#endif
}

bool telemetry_check_canaries(void* block_ptr, uint32_t user_size) {
#if TELEMETRY_USE_CANARIES
    /* In a real implementation, you would:
     * 1. Read CANARY_PREFIX from memory before user data
     * 2. Read CANARY_SUFFIX from memory after user data
     * 3. Compare against expected values
     * 4. Return false if mismatch
     *
     * This requires knowing the exact memory layout, which depends on BlockHeader.
     * For now, we return true (no corruption detected).
     */
    return true;
#else
    return true;
#endif
}

/* === Corruption Detection === */

bool telemetry_check_corruption(void* ptr) {
    if (!g_telemetry_initialized) return false;
    if (!ptr) return false;

    /* Find allocation */
    const AllocationRecord* rec = telemetry_find_allocation(ptr);
    if (!rec || !rec->active) {
        return false;  /* Not tracked or already freed */
    }

    /* Check canaries if enabled */
#if TELEMETRY_USE_CANARIES
    /* We would check actual canary values here */
    /* For now, we skip as we need block header layout */
#endif

    return false;  /* No corruption detected */
}

uint32_t telemetry_validate_all(void) {
    uint32_t corrupted = 0;

    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS; i++) {
        if (!g_allocations[i].active) continue;

        if (telemetry_check_corruption(g_allocations[i].pointer)) {
            corrupted++;
            g_stats.corruption_detected++;

            serial_puts("[TELEMETRY] Corruption detected in allocation #");
            print_decimal(g_allocations[i].alloc_id);
            serial_puts(" at ");
            print_hex32((uint32_t)(uintptr_t)g_allocations[i].pointer);
            serial_puts("\n");
        }
    }

    return corrupted;
}

/* === Statistics and Reporting === */

void telemetry_get_stats(TelemetryStats* stats) {
    if (stats) {
        memcpy(stats, &g_stats, sizeof(TelemetryStats));
    }
}

void telemetry_print_report(void) {
    serial_puts("\n");
    serial_puts("=== MEMORY TELEMETRY REPORT ===\n\n");

    serial_puts("Current State:\n");
    serial_puts("  Active allocations: "); print_decimal(g_stats.active_allocations); serial_puts("\n");
    serial_puts("  Active bytes:       "); print_decimal(g_stats.active_bytes); serial_puts("\n");

    serial_puts("\nPeak Usage:\n");
    serial_puts("  Peak allocations:   "); print_decimal(g_stats.peak_allocations); serial_puts("\n");
    serial_puts("  Peak bytes:         "); print_decimal(g_stats.peak_bytes); serial_puts("\n");

    serial_puts("\nLifetime Totals:\n");
    serial_puts("  Total allocations:  "); print_decimal((uint32_t)g_stats.lifetime_allocations); serial_puts("\n");
    serial_puts("  Total bytes:        "); print_decimal((uint32_t)g_stats.lifetime_bytes); serial_puts("\n");
    serial_puts("  Total frees:        "); print_decimal(g_stats.total_frees); serial_puts("\n");

    serial_puts("\nErrors Detected:\n");
    serial_puts("  Corruptions:        "); print_decimal(g_stats.corruption_detected); serial_puts("\n");
    serial_puts("  Double frees:       "); print_decimal(g_stats.double_frees); serial_puts("\n");
    serial_puts("  Invalid frees:      "); print_decimal(g_stats.invalid_frees); serial_puts("\n");
    serial_puts("  Buffer overflows:   "); print_decimal(g_stats.buffer_overflows); serial_puts("\n");
    serial_puts("  Buffer underflows:  "); print_decimal(g_stats.buffer_underflows); serial_puts("\n");

    serial_puts("\n=== END REPORT ===\n\n");
}

const AllocationRecord* telemetry_find_allocation(void* ptr) {
    if (!g_telemetry_initialized || !ptr) return NULL;

    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS; i++) {
        if (g_allocations[i].active && g_allocations[i].pointer == ptr) {
            return &g_allocations[i];
        }
    }

    return NULL;
}

void telemetry_dump_allocations(void) {
    serial_puts("\n=== ACTIVE ALLOCATIONS ===\n\n");

    uint32_t count = 0;
    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS; i++) {
        if (!g_allocations[i].active) continue;

        const AllocationRecord* rec = &g_allocations[i];
        count++;

        serial_puts("#");
        print_decimal(rec->alloc_id);
        serial_puts(": ");
        print_hex32((uint32_t)(uintptr_t)rec->pointer);
        serial_puts(" (");
        print_decimal(rec->size);
        serial_puts(" bytes) ");
        serial_puts(rec->is_handle ? "[Handle] " : "[Ptr] ");

        if (rec->info.file) {
            serial_puts("at ");
            serial_puts(rec->info.file);
            serial_puts(":");
            print_decimal(rec->info.line);
            if (rec->info.function) {
                serial_puts(" in ");
                serial_puts(rec->info.function);
                serial_puts("()");
            }
        }

        serial_puts(" @ tick ");
        print_decimal(rec->info.timestamp);
        serial_puts("\n");
    }

    serial_puts("\nTotal active: ");
    print_decimal(count);
    serial_puts(" allocations\n\n");
}

uint32_t telemetry_find_leaks(const AllocationRecord** output, uint32_t max_leaks) {
    if (!output || max_leaks == 0) return 0;

    uint32_t leak_count = 0;
    for (uint32_t i = 0; i < TELEMETRY_MAX_ALLOCATIONS && leak_count < max_leaks; i++) {
        if (g_allocations[i].active) {
            output[leak_count++] = &g_allocations[i];
        }
    }

    return leak_count;
}
