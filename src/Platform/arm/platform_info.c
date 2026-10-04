/*
 * Platform Info Implementation for ARM (Raspberry Pi)
 */

#include <string.h>
#include "Platform/platform_info_internal.h"
#include "hardware_detect.h"

static char g_model_string[64];

static platform_info_t g_platform_info = {
    .type = PLATFORM_ARM_PI4,  /* Default, will be updated */
    .platform_name = "Raspberry Pi",
    .model_string = g_model_string,
    .cpu_name = "ARM Cortex",
    .memory_bytes = 0,
    .cpu_freq_mhz = 0,
};

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
        strncpy(g_model_string, model_str, sizeof(g_model_string) - 1);
        g_model_string[sizeof(g_model_string) - 1] = '\0';
    }

    /* Set memory */
    g_platform_info.memory_bytes = mem_bytes > 0 ? mem_bytes : (512 * 1024 * 1024);

    g_platform_info_initialized = 1;
}

const platform_info_t *platform_info_platform_data(void) {
    platform_info_init();
    return &g_platform_info;
}
