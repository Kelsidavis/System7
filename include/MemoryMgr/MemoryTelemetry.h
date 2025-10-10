/*
 * Memory Manager Telemetry and Debugging
 * System 7.1 Portable
 *
 * Provides comprehensive heap telemetry including:
 * - Allocation tracking with call site info
 * - Memory canaries for corruption detection
 * - Guard pages and bounds checking
 * - Allocation statistics and profiling
 * - Leak detection
 */

#ifndef MEMORY_TELEMETRY_H
#define MEMORY_TELEMETRY_H

#include "SystemTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define TELEMETRY_ENABLED          1    /* Enable telemetry system */
#define TELEMETRY_TRACK_CALLSITES  1    /* Track allocation call sites */
#define TELEMETRY_USE_CANARIES     1    /* Enable canary values */
#define TELEMETRY_MAX_ALLOCATIONS  4096 /* Max tracked allocations */

/* Canary values for corruption detection */
#define CANARY_PREFIX  0xDEADBEEF  /* Before allocated block */
#define CANARY_SUFFIX  0xCAFEBABE  /* After allocated block */
#define CANARY_FREE    0xFEEDFACE  /* Marks freed blocks */

/* Allocation call site info */
typedef struct {
    const char* file;
    const char* function;
    uint32_t line;
    uint32_t size;
    uint32_t timestamp;  /* Tick count at allocation */
} AllocationInfo;

/* Allocation tracking entry */
typedef struct {
    void* pointer;           /* User pointer */
    uint32_t size;           /* Requested size */
    AllocationInfo info;     /* Call site */
    bool is_handle;          /* Handle vs Ptr */
    bool active;             /* Still allocated */
    uint32_t alloc_id;       /* Unique ID */
    uint32_t free_timestamp; /* When freed (if freed) */
} AllocationRecord;

/* Telemetry statistics */
typedef struct {
    /* Current state */
    uint32_t total_allocations;
    uint32_t total_frees;
    uint32_t active_allocations;
    uint32_t active_bytes;

    /* Peak usage */
    uint32_t peak_allocations;
    uint32_t peak_bytes;

    /* Totals over lifetime */
    uint64_t lifetime_allocations;
    uint64_t lifetime_bytes;

    /* Errors */
    uint32_t corruption_detected;
    uint32_t double_frees;
    uint32_t invalid_frees;
    uint32_t buffer_overflows;
    uint32_t buffer_underflows;
} TelemetryStats;

/**
 * Initialize telemetry system
 * Must be called before any memory operations
 */
void telemetry_init(void);

/**
 * Record a new allocation
 *
 * @param ptr User pointer
 * @param size Requested size
 * @param is_handle True if Handle, false if Ptr
 * @param file Source file (__FILE__)
 * @param line Line number (__LINE__)
 * @param func Function name (__func__)
 * @return Allocation ID, or 0 on failure
 */
uint32_t telemetry_record_alloc(void* ptr, uint32_t size, bool is_handle,
                                 const char* file, uint32_t line,
                                 const char* func);

/**
 * Record a deallocation
 *
 * @param ptr User pointer being freed
 * @return true if successful, false if pointer was invalid
 */
bool telemetry_record_free(void* ptr);

/**
 * Check for memory corruption at a pointer
 *
 * @param ptr User pointer to check
 * @return true if corruption detected
 */
bool telemetry_check_corruption(void* ptr);

/**
 * Validate all active allocations for corruption
 * Expensive operation - use for debugging
 *
 * @return Number of corrupted blocks found
 */
uint32_t telemetry_validate_all(void);

/**
 * Install canary values around an allocation
 *
 * @param block_ptr Pointer to block header
 * @param user_size User-requested size
 */
void telemetry_install_canaries(void* block_ptr, uint32_t user_size);

/**
 * Check canary values around an allocation
 *
 * @param block_ptr Pointer to block header
 * @param user_size User-requested size
 * @return true if canaries are intact, false if corrupted
 */
bool telemetry_check_canaries(void* block_ptr, uint32_t user_size);

/**
 * Get telemetry statistics
 *
 * @param stats Output statistics structure
 */
void telemetry_get_stats(TelemetryStats* stats);

/**
 * Print telemetry report to serial
 */
void telemetry_print_report(void);

/**
 * Find allocation record by pointer
 *
 * @param ptr User pointer
 * @return Allocation record, or NULL if not found
 */
const AllocationRecord* telemetry_find_allocation(void* ptr);

/**
 * Dump all active allocations
 * For leak detection and debugging
 */
void telemetry_dump_allocations(void);

/**
 * Find potential memory leaks
 * Returns pointers that were allocated but never freed
 *
 * @param output Array to store leak pointers
 * @param max_leaks Maximum leaks to return
 * @return Number of leaks found
 */
uint32_t telemetry_find_leaks(const AllocationRecord** output, uint32_t max_leaks);

/* Instrumentation macros */
#if TELEMETRY_ENABLED

#define TELEMETRY_ALLOC(ptr, size, is_handle) \
    telemetry_record_alloc((ptr), (size), (is_handle), __FILE__, __LINE__, __func__)

#define TELEMETRY_FREE(ptr) \
    telemetry_record_free(ptr)

#define TELEMETRY_CHECK(ptr) \
    telemetry_check_corruption(ptr)

#else

#define TELEMETRY_ALLOC(ptr, size, is_handle) ((void)0)
#define TELEMETRY_FREE(ptr) ((void)0)
#define TELEMETRY_CHECK(ptr) ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_TELEMETRY_H */
