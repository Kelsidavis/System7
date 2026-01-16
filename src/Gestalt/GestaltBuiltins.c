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

/* Define selector constants using canonical FOURCC. The ROM stored these as
 * four-byte ASCII codes; keeping the character spelling here aids cross-
 * referencing with Inside Macintosh docs. */
#ifndef DEFAULT_GESTALT_MACHINE_TYPE
#define DEFAULT_GESTALT_MACHINE_TYPE 0
#endif

#if defined(__powerpc__) || defined(__powerpc64__)
static OSErr gestalt_mmap(long *response);
#endif

#ifndef DEFAULT_BEZEL_STYLE
#define DEFAULT_BEZEL_STYLE 0
#endif

static const OSType kSel_sysv = FOURCC('s','y','s','v');
static const OSType kSel_qtim = FOURCC('q','t','i','m');
static const OSType kSel_rsrc = FOURCC('r','s','r','c');
static const OSType kSel_mach = FOURCC('m','a','c','h');
static const OSType kSel_proc = FOURCC('p','r','o','c');
static const OSType kSel_fpu_ = FOURCC('f','p','u',' ');
static const OSType kSel_init = FOURCC('i','n','i','t');
static const OSType kSel_evnt = FOURCC('e','v','n','t');
static const OSType kSel_pcop = FOURCC('p','c','o','p');  /* Process coop */
#if defined(__powerpc__) || defined(__powerpc64__)
static const OSType kSel_mmap = FOURCC('m','m','a','p');
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

/* Architecture-agnostic FPU detection
 * -------------------------------
 * Classic System 7 would poke 68k coprocessor state; on modern hardware we
 * have to probe per CPU family.  We avoid libc and only touch registers that
 * are architecturally safe in freestanding mode. */
static int probe_fpu_present(void) {
#if defined(__x86_64__) || defined(__i386__)
    /* CPUID leaf 1, EDX bit 0 (x87 FPU) */
    unsigned int a = 1, b = 0, c = 0, d = 0;

    #if defined(__i386__) && defined(__PIC__)
    /* inline asm clobbers ebx; save/restore for PIC */
    __asm__ __volatile__(
        "xchgl %%ebx, %1\n\t"
        "cpuid\n\t"
        "xchgl %%ebx, %1"
        : "+a"(a), "+r"(b), "+c"(c), "+d"(d));
    #else
    __asm__ __volatile__("cpuid" : "+a"(a), "+b"(b), "+c"(c), "+d"(d) : "0"(1));
    #endif

    return (d & 0x00000001u) ? 1 : 0;

#elif defined(__aarch64__)
    /* AArch64 mandates FP/ASIMD */
    return 1;

#elif defined(__arm__)
    /* Don't touch coprocessor regs in freestanding; default off */
    return 0;

#elif defined(__riscv) || defined(__riscv__)
    /* Try reading misa CSR if available */
    #if defined(__GNUC__) || defined(__clang__)
        unsigned long misa = 0;
        #if defined(__riscv_xlen) && (__riscv_xlen == 64)
        __asm__ __volatile__("csrr %0, misa" : "=r"(misa));
        return ((misa && ((misa & (1UL<<(5))) || (misa & (1UL<<(3))))) ? 1 : 0); /* F=5, D=3 */
        #else
        return 0; /* 32-bit RISC-V: conservatively assume no FPU */
        #endif
    #else
    return 0;
    #endif

#elif defined(__powerpc__) || defined(__powerpc64__)
    /* Conservative: PPC usually has FPU in our targets */
    return 1;

#else
    /* Unknown architecture */
    return 0;
#endif
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

#ifdef ENABLE_RESOURCES
    /* Check if Resource Manager is initialized */
    if (gGestaltInitBits & (1UL << kGestaltInitBit_ResourceMgr)) {
        *response = 0x00010000;  /* Version 1.0.0 */
    } else {
        *response = 0;  /* Not yet initialized */
    }
#else
    return gestaltUnknownErr;  /* Not compiled in */
#endif

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

    /* Call architecture-specific FPU probe */
    *response = probe_fpu_present();

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

#ifdef ENABLE_PROCESS_COOP
    *response |= 0x01;  /* Event queue present */
    *response |= 0x02;  /* Mouse synthesis */
#endif

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

#ifdef ENABLE_PROCESS_COOP
    *response |= 0x01;  /* Coop scheduler present */
    *response |= 0x02;  /* Process sleep supported */
    *response |= 0x04;  /* Block on event supported */
#endif

    return noErr;
}

/* Register all built-in selectors */
void Gestalt_Register_Builtins(void) {
    OSErr err;

    /* System version - always register */
    err = NewGestalt(kSel_sysv, gestalt_sysv);
    /* Ignore error - may already be registered */

    /* Machine type - always register */
    err = NewGestalt(kSel_mach, gestalt_mach);

    /* Processor type - always register */
    err = NewGestalt(kSel_proc, gestalt_proc);

    /* FPU type - always register */
    err = NewGestalt(kSel_fpu_, gestalt_fpu);

    /* Init bits - always register */
    err = NewGestalt(kSel_init, gestalt_init);

#if defined(__powerpc__) || defined(__powerpc64__)
    err = NewGestalt(kSel_mmap, gestalt_mmap);
#endif

    /* Time Manager - only register if initialized */
    if (gGestaltInitBits & (1UL << kGestaltInitBit_TimeMgr)) {
        err = NewGestalt(kSel_qtim, gestalt_qtim);
    }

#ifdef ENABLE_RESOURCES
    /* Resource Manager - only register if compiled in */
    err = NewGestalt(kSel_rsrc, gestalt_rsrc);
#endif

    /* Event Manager features */
    err = NewGestalt(kSel_evnt, gestalt_evnt);

    /* Process Manager cooperative features */
    err = NewGestalt(kSel_pcop, gestalt_pcop);

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
