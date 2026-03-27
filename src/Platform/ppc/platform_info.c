/*
 * platform_info.c - PowerPC platform metadata
 *
 * Provides placeholder platform information so About boxes and Gestalt queries
 * have something sensible to report while the PowerPC port is under active
 * development.
 */

#include <string.h>
#include "Platform/platform_info.h"
#include "System71StdLib.h"
#include "Platform/include/boot.h"

static platform_info_t g_platform_info = {
    .type = PLATFORM_PPC_GENERIC,
    .platform_name = "Macintosh PowerPC",
    .model_string = "PowerPC Development Board",
    .cpu_name = "PowerPC 601",
    .memory_bytes = 0,
    .cpu_freq_mhz = 0,
};

static char g_memory_gb_str[32] = {0};
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

const platform_info_t* platform_get_info(void) {
    platform_info_init();
    return &g_platform_info;
}

const char* platform_get_display_name(void) {
    platform_info_init();
    return g_platform_info.platform_name;
}

const char* platform_get_model_string(void) {
    platform_info_init();
    return g_platform_info.model_string;
}

uint32_t platform_get_memory_bytes(void) {
    platform_info_init();
    return g_platform_info.memory_bytes;
}

const char* platform_format_memory_gb(void) {
    platform_info_init();

    uint32_t bytes = g_platform_info.memory_bytes;
    uint32_t gb = bytes / (1024 * 1024 * 1024);
    uint32_t mb_remainder = (bytes % (1024 * 1024 * 1024)) / (1024 * 1024);

    if (gb == 0 && mb_remainder == 0) {
        strncpy(g_memory_gb_str, "0 GB", sizeof(g_memory_gb_str) - 1);
        g_memory_gb_str[sizeof(g_memory_gb_str) - 1] = '\0';
        return g_memory_gb_str;
    }

    if (mb_remainder > 512) {
        gb++;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u GB", gb);
    } else if (mb_remainder > 0) {
        uint32_t decimal = (mb_remainder * 10) / 1024;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u.%u GB", gb, decimal);
    } else {
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u GB", gb);
    }

    return g_memory_gb_str;
}

void platform_format_memory_kb(uint32_t bytes, char *buf, size_t buf_size) {
    if (!buf || buf_size < 16) {
        return;
    }

    uint32_t kb = bytes / 1024;
    if (kb < 1000) {
        snprintf(buf, buf_size, "%uK", kb);
    } else {
        uint32_t thousands = kb / 1000;
        uint32_t remainder = kb % 1000;
        snprintf(buf, buf_size, "%u,%03uK", thousands, remainder);
    }
}
