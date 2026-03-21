/*
 * PlatformTime.c - Platform-specific timer abstraction
 * Multi-architecture support: x86, ARM, AArch64, RISC-V, PowerPC
 * Freestanding implementation - no OS dependencies
 */

#include "SystemTypes.h"
#include "TimeManager/TimeBase.h"

/* Architecture-agnostic counter reading */
static inline uint64_t read_counter(void) {
#if defined(__x86_64__) || defined(__i386__)
    /* x86: Time Stamp Counter (TSC) */
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;

#elif defined(__aarch64__)
    /* AArch64: Generic timer virtual count */
    uint64_t vct;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(vct));
    return vct;

#elif defined(__arm__)
    /* ARMv7: Fallback to monotonically increasing software counter */
    /* No coprocessor access in freestanding mode */
    static volatile uint64_t soft = 0;
    return (soft += 1000);  /* ~1 MHz simulated tick; calibrated later */

#elif defined(__riscv) || defined(__riscv__)
    /* RISC-V: Cycle CSR */
    uint64_t c = 0;
    #if defined(__riscv_xlen) && (__riscv_xlen == 64)
    /* 64-bit RISC-V: Direct read */
    __asm__ __volatile__("csrr %0, cycle" : "=r"(c));
    #else
    /* 32-bit RISC-V: Read high/low carefully */
    uint32_t hi1, lo, hi2;
    __asm__ __volatile__("csrr %0, cycleh" : "=r"(hi1));
    __asm__ __volatile__("csrr %0, cycle"  : "=r"(lo));
    __asm__ __volatile__("csrr %0, cycleh" : "=r"(hi2));
    if (hi1 != hi2) {
        /* High word changed, re-read low */
        __asm__ __volatile__("csrr %0, cycle" : "=r"(lo));
    }
    c = ((uint64_t)hi2 << 32) | lo;
    #endif
    return c;

#elif defined(__powerpc__) || defined(__powerpc64__)
    /* PowerPC: Time Base Register (TBR) - SPR 268/269 */
    uint32_t tbu0, tbu1, tbl;
    __asm__ __volatile__(
        "mftbu %0\n\t"
        "mftb  %1\n\t"
        "mftbu %2\n\t"
        : "=r"(tbu0), "=r"(tbl), "=r"(tbu1));

    /* Check if upper word changed during read */
    if (tbu0 != tbu1) {
        /* Re-read lower word */
        __asm__ __volatile__("mftb %0" : "=r"(tbl));
    }
    return ((uint64_t)tbu1 << 32) | tbl;

#else
    /* Portable simulated monotonic counter for unknown architectures */
    static uint64_t gSim = 0;
    return (gSim += 1000000ULL);  /* ~1 MHz simulated frequency */
#endif
}

static uint64_t gBootCounter = 0;
static uint64_t gCounterFreq __attribute__((unused)) = 2000000000ULL; /* Default 2GHz, calibrated in TimeBase */

OSErr InitPlatformTimer(void) {
    gBootCounter = read_counter();
    return noErr;
}

void ShutdownPlatformTimer(void) {
    /* Nothing to do */
}

uint64_t PlatformCounterNow(void) {
    return read_counter();
}

OSErr GetPlatformTime(UnsignedWide *timeValue) {
    uint64_t now = read_counter();
    timeValue->hi = (UInt32)(now >> 32);
    timeValue->lo = (UInt32)(now & 0xFFFFFFFF);
    return noErr;
}
