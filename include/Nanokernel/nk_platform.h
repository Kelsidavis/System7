/* nk_platform.h - Nanokernel Platform Abstraction Layer (C23)
 *
 * Defines the interface between portable nanokernel code and
 * platform-specific implementations.
 *
 * Architecture Support:
 * - x86 (32-bit) - src/Nanokernel/platform/x86/
 * - ARM64 - (future) src/Nanokernel/platform/arm64/
 * - PowerPC - (future) src/Nanokernel/platform/ppc/
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 *   Platform Identification
 * ============================================================ */

#if defined(__i386__) || defined(__i686__)
#define NK_PLATFORM_X86 1
#define NK_PLATFORM_NAME "x86"
#define NK_PLATFORM_BITS 32
#elif defined(__x86_64__)
#define NK_PLATFORM_X86_64 1
#define NK_PLATFORM_NAME "x86-64"
#define NK_PLATFORM_BITS 64
#elif defined(__aarch64__)
#define NK_PLATFORM_ARM64 1
#define NK_PLATFORM_NAME "ARM64"
#define NK_PLATFORM_BITS 64
#elif defined(__powerpc__) || defined(__ppc__)
#define NK_PLATFORM_PPC 1
#define NK_PLATFORM_NAME "PowerPC"
#define NK_PLATFORM_BITS 32
#else
#error "Unsupported platform - please add support"
#endif

/* ============================================================
 *   Platform Initialization
 * ============================================================ */

/**
 * Initialize platform-specific nanokernel components.
 *
 * This function must:
 * - Set up interrupt infrastructure (IDT/GDT on x86, vector table on ARM)
 * - Configure interrupt controllers (PIC on x86, GIC on ARM)
 * - Initialize platform timer hardware
 *
 * Called once during kernel boot, before interrupts are enabled.
 */
void nk_platform_init(void);

/* ============================================================
 *   Context Switching (Platform-Specific Assembly)
 * ============================================================ */

/**
 * Switch from old context to new context (cooperative).
 *
 * Platform-specific assembly implementation.
 * Must save all callee-saved registers and switch stacks.
 *
 * @param old  Pointer to old context (NULL for first switch)
 * @param new  Pointer to new context
 */
void nk_switch_context(void *old, void *new);

/**
 * Switch contexts from interrupt handler (preemptive).
 *
 * Platform-specific assembly implementation.
 * Must handle interrupt frame restoration via IRET (x86) or ERET (ARM).
 *
 * @param prev        Previous thread
 * @param next        Next thread
 * @param prev_frame  Interrupt frame pointer
 */
void nk_switch_context_irq(void *prev, void *next, void *prev_frame);

/* ============================================================
 *   Atomic Operations (Platform-Specific)
 * ============================================================ */

#ifdef NK_PLATFORM_X86

/**
 * Disable interrupts and return previous state.
 *
 * x86: CLI instruction
 */
static inline uint32_t nk_platform_irq_disable(void) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

/**
 * Restore interrupt state.
 *
 * x86: STI if interrupts were enabled
 */
static inline void nk_platform_irq_restore(uint32_t flags) {
    if (flags & 0x200) {  // IF flag
        __asm__ volatile("sti" ::: "memory");
    }
}

/**
 * Halt CPU until next interrupt.
 *
 * x86: HLT instruction
 */
static inline void nk_platform_halt(void) {
    __asm__ volatile("hlt");
}

#endif // NK_PLATFORM_X86

/* ============================================================
 *   Platform Memory Barriers
 * ============================================================ */

/**
 * Full memory barrier.
 *
 * Ensures all memory operations before this point complete
 * before any operations after this point begin.
 */
static inline void nk_platform_mb(void) {
    __asm__ volatile("" ::: "memory");
}

/* ============================================================
 *   Platform Timer (Abstraction for Future SMP)
 * ============================================================ */

/**
 * Get current CPU ID (for future SMP support).
 *
 * Currently always returns 0 (uniprocessor).
 *
 * @return CPU ID (0-based)
 */
static inline uint32_t nk_platform_cpu_id(void) {
    return 0;  // TODO: Read from LAPIC (x86) or MPIDR (ARM64)
}

/* ============================================================
 *   Debug Support
 * ============================================================ */

/**
 * Platform-specific debug breakpoint.
 *
 * x86: INT3 instruction
 * ARM: BRK #0
 */
#ifdef NK_PLATFORM_X86
#define nk_platform_breakpoint() __asm__ volatile("int3")
#endif
