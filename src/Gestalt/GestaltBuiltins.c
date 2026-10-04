/*
 * GestaltBuiltins.c - Built-in Gestalt selectors
 * Based on Inside Macintosh: Operating System Utilities
 * Multi-architecture support for x86/ARM/RISC-V/PowerPC
 */

#include "SystemTypes.h"
#include "Gestalt/Gestalt.h"
#include "Gestalt/GestaltPriv.h"

#if defined(__powerpc__) || defined(__powerpc64__)
#include "Platform/include/boot.h"
#endif

#ifndef DEFAULT_GESTALT_MACHINE_TYPE
#define DEFAULT_GESTALT_MACHINE_TYPE 0
#endif

#if defined(__powerpc__) || defined(__powerpc64__)
static OSErr gestalt_mmap(long *response);
#endif

/* Global init bits for tracking subsystem initialization */
static UInt32 gGestaltInitBits = 0;
static UInt16 gGestaltMachineType = (UInt16)DEFAULT_GESTALT_MACHINE_TYPE;

/* Set an init bit when a subsystem comes up */
void Gestalt_SetInitBit(int bit) {
    if (bit >= 0 && bit < 32) {
        gGestaltInitBits |= (1UL << bit);
    }
}

void Gestalt_SetMachineType(UInt16 machineType) {
    gGestaltMachineType = machineType;
}

UInt16 Gestalt_GetMachineType(void) {
    return gGestaltMachineType;
}

/* Built-in selector: System version */
static OSErr gestalt_sysv(long *response) {
    if (!response) return paramErr;
    *response = 0x0710;  /* BCD for System 7.1 */
    return noErr;
}

/* Built-in selector: Time Manager version */
static OSErr gestalt_qtim(long *response) {
    if (!response) return paramErr;

    /* Check if Time Manager is initialized */
    if (gGestaltInitBits & (1UL << kGestaltInitBit_TimeMgr)) {
        *response = 0x00010000;  /* Version 1.0.0 */
    } else {
        return gestaltUnknownErr;  /* Not registered until ready */
    }

    return noErr;
}

/* Built-in selector: Resource Manager version */
static OSErr gestalt_rsrc(long *response) {
    if (!response) return paramErr;

    /* Check if Resource Manager is initialized */
    if (gGestaltInitBits & (1UL << kGestaltInitBit_ResourceMgr)) {
        *response = 0x00010000;  /* Version 1.0.0 */
    } else {
        *response = 0;  /* Not yet initialized */
    }

    return noErr;
}

/* Built-in selector: Machine type */
static OSErr gestalt_mach(long *response) {
    if (!response) return paramErr;

    if (gGestaltMachineType != 0) {
        *response = gGestaltMachineType;
        return noErr;
    }

    /* Machine family codes (mirrors gestaltMachineType examples documented in
     * Inside Macintosh, extended for our additional ports).
     *
     * NOTE: Many NewWorld ROMs report gestaltMachineType = 0x0196 (decimal 406)
     *       for Power Macintosh systems that dynamically adjust CRT rounding
     *       depending on ADC/DVI vs. VGA output. We currently return coarse
     *       architecture families; when we implement per-model behaviour the
     *       selector can key off that documented 0x0196 value. */
#if defined(__x86_64__) || defined(__i386__)
    *response = 0x0086;  /* x86 family */
#elif defined(__aarch64__) || defined(__arm__)
    *response = 0x00AA;  /* ARM family */
#elif defined(__riscv) || defined(__riscv__)
    *response = 0x00B5;  /* RISC-V family */
#elif defined(__powerpc__) || defined(__powerpc64__)
    *response = 0x0050;  /* PowerPC family */
#else
    *response = 0x0000;  /* Unknown */
#endif

    return noErr;
}

/* Built-in selector: Processor type */
static OSErr gestalt_proc(long *response) {
    if (!response) return paramErr;

    /* Processor subtype codes mirror the values the Finder expects (cf. TN
     * Gestalt Manager).
     * x86: 0x0300 (i386), 0x0600 (i686), 0x8664 (x86_64)
     * ARM: 0x0700 (ARMv7)
     * AArch64: 0x0A64
     * RISC-V: 0x5264 (RV64)
     * PowerPC: 0x5032 (32-bit), 0x5064 (64-bit)
     */
#if defined(__x86_64__)
    *response = 0x8664;  /* x86_64 */
#elif defined(__i686__)
    *response = 0x0600;  /* i686 */
#elif defined(__i386__)
    *response = 0x0300;  /* i386 */
#elif defined(__aarch64__)
    *response = 0x0A64;  /* AArch64 */
#elif defined(__arm__)
    *response = 0x0700;  /* ARMv7 */
#elif defined(__riscv) || defined(__riscv__)
    #if defined(__riscv_xlen) && (__riscv_xlen == 64)
    *response = 0x5264;  /* RV64 */
    #else
    *response = 0x5232;  /* RV32 */
    #endif
#elif defined(__powerpc64__)
    *response = 0x5064;  /* PowerPC 64-bit */
#elif defined(__powerpc__)
    *response = 0x5032;  /* PowerPC 32-bit */
#else
    *response = 0x0000;  /* Unknown */
#endif

    return noErr;
}

/* Built-in selector: FPU type */
static OSErr gestalt_fpu(long *response) {
    if (!response) return paramErr;

    /* The current 68K execution path does not provide an emulated FPU. */
    *response = gestaltNoFPU;

    return noErr;
}

/* Built-in selector: Init bits */
static OSErr gestalt_init(long *response) {
    if (!response) return paramErr;

    /* Return the current init bits */
    *response = (long)gGestaltInitBits;

    return noErr;
}

/* Built-in selector: Event Manager features */
static OSErr gestalt_evnt(long *response) {
    if (!response) return paramErr;

    /* Event feature bits:
     * bit 0: Event queue present
     * bit 1: Mouse synthesis
     * bit 2: Keyboard synthesis
     */
    *response = 0;

    *response |= 0x01;  /* Event queue present */
    *response |= 0x02;  /* Mouse synthesis */

    return noErr;
}

/* Built-in selector: Process Manager cooperative features */
static OSErr gestalt_pcop(long *response) {
    if (!response) return paramErr;

    /* Process coop feature bits:
     * bit 0: Cooperative scheduler
     * bit 1: Process sleep
     * bit 2: Block on event
     */
    *response = 0;

    *response |= 0x01;  /* Coop scheduler present */
    *response |= 0x02;  /* Process sleep supported */
    *response |= 0x04;  /* Block on event supported */

    return noErr;
}

/* Register all built-in selectors */
void Gestalt_Register_Builtins(void) {
    OSErr err;

    /* System version - always register */
    err = NewGestalt(gestaltSystemVersion, gestalt_sysv);
    /* Ignore error - may already be registered */

    /* Machine type - always register */
    err = NewGestalt(gestaltMachineType, gestalt_mach);

    /* Processor type - always register */
    err = NewGestalt(gestaltProcessorType, gestalt_proc);

    /* FPU type - always register */
    err = NewGestalt(gestaltFPUType, gestalt_fpu);

    /* Init bits - always register */
    err = NewGestalt(gestaltInitBits, gestalt_init);

#if defined(__powerpc__) || defined(__powerpc64__)
    err = NewGestalt(gestaltMemoryMap, gestalt_mmap);
#endif

    /* Time Manager - only register if initialized */
    if (gGestaltInitBits & (1UL << kGestaltInitBit_TimeMgr)) {
        err = NewGestalt(gestaltTimeMgrVersion, gestalt_qtim);
    }

    /* Resource Manager - only register if compiled in */
    err = NewGestalt(gestaltResourceMgrVers, gestalt_rsrc);

    /* Event Manager features */
    err = NewGestalt(gestaltEventFeatures, gestalt_evnt);

    /* Process Manager cooperative features */
    err = NewGestalt(gestaltProcessFeatures, gestalt_pcop);

    /* Unused variable warning suppression */
    (void)err;
}
#if defined(__powerpc__) || defined(__powerpc64__)
#define MMAP_VALUE_COUNT (1 + OFW_MAX_MEMORY_RANGES * 4)
static long g_ppc_memory_map_cache[MMAP_VALUE_COUNT] = {0};
static int g_ppc_memory_map_cached = 0;

static void populate_ppc_memory_map_cache(void) {
    if (g_ppc_memory_map_cached) {
        return;
    }

    size_t count = hal_ppc_memory_range_count();
    if (count == 0) {
        g_ppc_memory_map_cache[0] = 0;
        g_ppc_memory_map_cached = 1;
        return;
    }

    ofw_memory_range_t ranges[OFW_MAX_MEMORY_RANGES];
    size_t copied = hal_ppc_get_memory_ranges(ranges, OFW_MAX_MEMORY_RANGES);

    if (copied == 0) {
        g_ppc_memory_map_cache[0] = 0;
        g_ppc_memory_map_cached = 1;
        return;
    }

    g_ppc_memory_map_cache[0] = (long)copied;
    long *cursor = &g_ppc_memory_map_cache[1];
    for (size_t i = 0; i < copied; ++i) {
        uint64_t base = ranges[i].base;
        uint64_t size = ranges[i].size;
        *cursor++ = (long)(uint32_t)(base >> 32);
        *cursor++ = (long)(uint32_t)(base & 0xFFFFFFFFu);
        *cursor++ = (long)(uint32_t)(size >> 32);
        *cursor++ = (long)(uint32_t)(size & 0xFFFFFFFFu);
    }

    g_ppc_memory_map_cached = 1;
}

static OSErr gestalt_mmap(long *response) {
    if (!response) return paramErr;

    populate_ppc_memory_map_cache();
    *response = (long)(uintptr_t)g_ppc_memory_map_cache;
    return noErr;
}
#endif
