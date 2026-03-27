/*
 * Platform Info Implementation for x86
 */

#include <string.h>
#include "Platform/platform_info.h"
#include "System71StdLib.h"

/* Memory detection - expects g_total_memory_kb from multiboot */
extern uint32_t g_total_memory_kb;

static platform_info_t g_platform_info = {
    .type = PLATFORM_X86,
    .platform_name = "Macintosh x86",
    .model_string = "Intel PC Compatible",
    .cpu_name = "Generic x86",
    .memory_bytes = 0,  /* Will be set from multiboot */
    .cpu_freq_mhz = 0,
};

static char g_memory_gb_str[32] = {0};

/* Initialize platform info on first call */
static int g_platform_info_initialized = 0;

static void platform_info_init(void) {
    if (g_platform_info_initialized) return;

    /* Get memory size from multiboot detection (in KB) */
    if (g_total_memory_kb > 0) {
        g_platform_info.memory_bytes = g_total_memory_kb * 1024;
    } else {
        /* Fallback default */
        g_platform_info.memory_bytes = 512 * 1024 * 1024;  /* 512 MB */
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

    if (mb_remainder > 512) {
        /* Round up if >= 512 MB */
        gb++;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u GB", gb);
    } else if (mb_remainder > 0) {
        /* Show decimal if there's remainder */
        uint32_t decimal = (mb_remainder * 10) / 1024;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u.%u GB", gb, decimal);
    } else {
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%u GB", gb);
    }

    return g_memory_gb_str;
}

void platform_format_memory_kb(uint32_t bytes, char *buf, size_t buf_size) {
    if (!buf || buf_size < 16) return;

    uint32_t kb = bytes / 1024;
    if (kb < 1000) {
        snprintf(buf, buf_size, "%uK", kb);
    } else {
        uint32_t thousands = kb / 1000;
        uint32_t remainder = kb % 1000;
        snprintf(buf, buf_size, "%u,%03uK", thousands, remainder);
    }
}
