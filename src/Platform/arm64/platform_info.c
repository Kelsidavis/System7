/*
 * ARM64 Platform Information
 * Provides platform-specific information for System7
 */

#include <stdint.h>
#include <stddef.h>
#include "System71StdLib.h"

/* Forward declaration */
extern uint32_t hal_get_memory_size(void);

/*
 * Get display name string
 */
const char* platform_get_display_name(void) {
#ifdef QEMU_BUILD
    return "QEMU ARM64 Virtual Display";
#else
    return "Raspberry Pi HDMI Display";
#endif
}

/*
 * Get model string
 */
const char* platform_get_model_string(void) {
#ifdef QEMU_BUILD
    return "QEMU ARM64 Virtual Machine";
#else
    return "Raspberry Pi";
#endif
}

/*
 * Format memory size in GB
 */
const char* platform_format_memory_gb(void) {
    static char buf[32];
    uint32_t mem_mb = hal_get_memory_size() / (1024 * 1024);

    if (mem_mb >= 1024) {
        uint32_t mem_gb = mem_mb / 1024;
        snprintf(buf, sizeof(buf), "%u GB", mem_gb);
    } else {
        snprintf(buf, sizeof(buf), "%u MB", mem_mb);
    }
    return buf;
}

/*
 * Get CPU name string
 */
const char* platform_get_cpu_name(void) {
#ifdef QEMU_BUILD
    return "ARM Cortex-A53 (QEMU)";
#else
    /* Read MIDR_EL1 to detect CPU */
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));

    uint32_t partnum = (midr >> 4) & 0xFFF;

    switch (partnum) {
        case 0xD03: return "ARM Cortex-A53";
        case 0xD08: return "ARM Cortex-A72";
        case 0xD0B: return "ARM Cortex-A76";
        default:    return "ARM Cortex";
    }
#endif
}

/*
 * Get platform type
 */
uint32_t platform_get_type(void) {
#ifdef QEMU_BUILD
    return 0x1000;  /* QEMU virt */
#else
    return 0x1001;  /* Raspberry Pi */
#endif
}

/*
 * Get machine Gestalt value
 */
int32_t platform_get_gestalt_machine(void) {
#ifdef QEMU_BUILD
    return 'qemu';  /* QEMU identifier */
#else
    return 'rasP';  /* Raspberry Pi identifier */
#endif
}
