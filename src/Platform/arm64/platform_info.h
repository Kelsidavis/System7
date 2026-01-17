/*
 * ARM64 Platform Information Interface
 * Provides platform-specific information for System7
 */

#ifndef ARM64_PLATFORM_INFO_H
#define ARM64_PLATFORM_INFO_H

#include <stdint.h>

/* Get display name string */
const char* platform_get_display_name(void);

/* Get model string */
const char* platform_get_model_string(void);

/* Format memory size in GB */
const char* platform_format_memory_gb(void);

/* Get CPU name string */
const char* platform_get_cpu_name(void);

/* Get platform type */
uint32_t platform_get_type(void);

/* Get machine Gestalt value */
int32_t platform_get_gestalt_machine(void);

#endif /* ARM64_PLATFORM_INFO_H */
