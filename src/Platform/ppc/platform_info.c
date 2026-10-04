/*
 * platform_info.c - PowerPC platform metadata
 *
 * Provides placeholder platform information so About boxes and Gestalt queries
 * have something sensible to report while the PowerPC port is under active
 * development.
 */

#include "Platform/platform_info_internal.h"
#include "Platform/include/boot.h"

static platform_info_t g_platform_info = {
    .type = PLATFORM_PPC_GENERIC,
    .platform_name = "Macintosh PowerPC",
    .model_string = "PowerPC Development Board",
    .cpu_name = "PowerPC 601",
    .memory_bytes = 0,
    .cpu_freq_mhz = 0,
};

static int g_platform_info_initialized = 0;

static void platform_info_init(void) {
    if (g_platform_info_initialized) {
        return;
    }

    #if defined(__powerpc__) || defined(__powerpc64__)
    ofw_memory_range_t ranges[OFW_MAX_MEMORY_RANGES];
    size_t count = hal_ppc_get_memory_ranges(ranges, OFW_MAX_MEMORY_RANGES);
    if (count > 0) {
        uint64_t total = 0;
        for (size_t i = 0; i < count; ++i) {
            total += ranges[i].size;
        }
        if (total > 0) {
            if (total > UINT32_MAX) {
                g_platform_info.memory_bytes = UINT32_MAX;
            } else {
                g_platform_info.memory_bytes = (uint32_t)total;
            }
        }
    } else
    #endif
    {
        g_platform_info.memory_bytes = 256 * 1024 * 1024; /* fallback */
    }

    g_platform_info_initialized = 1;
}

const platform_info_t *platform_info_platform_data(void) {
    platform_info_init();
    return &g_platform_info;
}
