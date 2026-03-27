/*
 * Platform Info Implementation for ARM (Raspberry Pi)
 */

#include <string.h>
#include "Platform/platform_info.h"
#include "System71StdLib.h"
#include "hardware_detect.h"

static platform_info_t g_platform_info = {
    .type = PLATFORM_ARM_PI4,  /* Default, will be updated */
    .platform_name = "Raspberry Pi",
    .model_string = "",
    .cpu_name = "ARM Cortex",
    .memory_bytes = 0,
    .cpu_freq_mhz = 0,
};

static char g_memory_gb_str[32] = {0};
static int g_platform_info_initialized = 0;

extern uint32_t device_tree_get_memory_size(void);

static void platform_info_init(void) {
    if (g_platform_info_initialized) return;

    /* Get model and memory from device tree */
    rpi_model_t model = hardware_get_model();
    const char *model_str = hardware_get_model_string();
    uint32_t mem_bytes = device_tree_get_memory_size();

    /* Update platform type based on model */
    switch (model) {
        case PI_MODEL_3:
            g_platform_info.type = PLATFORM_ARM_PI3;
            g_platform_info.platform_name = "Macintosh Raspberry Pi 3";
            g_platform_info.cpu_name = "ARM Cortex-A53";
            g_platform_info.cpu_freq_mhz = 1200;
            break;
        case PI_MODEL_4:
            g_platform_info.type = PLATFORM_ARM_PI4;
            g_platform_info.platform_name = "Macintosh Raspberry Pi 4";
            g_platform_info.cpu_name = "ARM Cortex-A72";
            g_platform_info.cpu_freq_mhz = 1500;
            break;
        case PI_MODEL_5:
            g_platform_info.type = PLATFORM_ARM_PI5;
            g_platform_info.platform_name = "Macintosh Raspberry Pi 5";
            g_platform_info.cpu_name = "ARM Cortex-A76";
            g_platform_info.cpu_freq_mhz = 2400;
            break;
        default:
            g_platform_info.type = PLATFORM_ARM_PI4;
            g_platform_info.platform_name = "Macintosh Raspberry Pi";
            g_platform_info.cpu_name = "ARM Cortex";
            g_platform_info.cpu_freq_mhz = 0;
            break;
    }

    /* Set model string */
    if (model_str && model_str[0] != '\0') {
        strncpy((char*)g_platform_info.model_string, model_str, 63);
        ((char*)g_platform_info.model_string)[63] = '\0';
    }

    /* Set memory */
    g_platform_info.memory_bytes = mem_bytes > 0 ? mem_bytes : (512 * 1024 * 1024);

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
