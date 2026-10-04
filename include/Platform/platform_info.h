/*
 * Platform Information Interface
 * Provides unified API for hardware detection across x86 and ARM platforms
 */

#ifndef PLATFORM_INFO_H
#define PLATFORM_INFO_H

#include <stddef.h>
#include <stdint.h>

/* Platform types */
typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_X86,
    PLATFORM_ARM_PI3,
    PLATFORM_ARM_PI4,
    PLATFORM_ARM_PI5,
    PLATFORM_PPC_GENERIC,
} platform_type_t;

/* Platform information structure */
typedef struct {
    platform_type_t type;
    const char *platform_name;      /* e.g., "Macintosh x86", "Raspberry Pi 4" */
    const char *model_string;       /* e.g., "Intel PC", "Raspberry Pi 4 Model B" */
    const char *cpu_name;           /* e.g., "Generic x86", "ARM Cortex-A72" */
    uint32_t memory_bytes;          /* Total system memory in bytes */
    uint32_t cpu_freq_mhz;          /* CPU frequency in MHz (0 if unknown) */
} platform_info_t;

/* Get current platform information */
const platform_info_t* platform_get_info(void);

/* Get platform name for display ("Macintosh x86" or "Raspberry Pi 4") */
const char* platform_get_display_name(void);

/* Get model string */
const char* platform_get_model_string(void);

/* Get memory in bytes */
uint32_t platform_get_memory_bytes(void);

/* Format memory as GB string (e.g., "4 GB", "1.5 GB")
 * Returns pointer to static buffer
 */
const char* platform_format_memory_gb(void);

/* Format memory in KB format for About dialog */
void platform_format_memory_kb(uint32_t bytes, char *buf, size_t buf_size);

#endif /* PLATFORM_INFO_H */
