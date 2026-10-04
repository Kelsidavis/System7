#include "Platform/platform_info_internal.h"
#include "System71StdLib.h"

static char g_memory_gb_str[32];

const platform_info_t *platform_get_info(void) {
    return platform_info_platform_data();
}

const char *platform_get_display_name(void) {
    return platform_info_platform_data()->platform_name;
}

const char *platform_get_model_string(void) {
    return platform_info_platform_data()->model_string;
}

uint32_t platform_get_memory_bytes(void) {
    return platform_info_platform_data()->memory_bytes;
}

const char *platform_format_memory_gb(void) {
    uint32_t bytes = platform_info_platform_data()->memory_bytes;
    uint32_t gb = bytes / (1024 * 1024 * 1024);
    uint32_t mb_remainder = (bytes % (1024 * 1024 * 1024)) / (1024 * 1024);

    if (mb_remainder > 512) {
        gb++;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%lu GB", (unsigned long)gb);
    } else if (mb_remainder > 0) {
        uint32_t decimal = (mb_remainder * 10) / 1024;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%lu.%lu GB",
                 (unsigned long)gb, (unsigned long)decimal);
    } else {
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%lu GB", (unsigned long)gb);
    }

    return g_memory_gb_str;
}

void platform_format_memory_kb(uint32_t bytes, char *buf, size_t buf_size) {
    if (!buf || buf_size < 16) return;

    uint32_t kb = bytes / 1024;
    if (kb < 1000) {
        snprintf(buf, buf_size, "%luK", (unsigned long)kb);
    } else {
        uint32_t thousands = kb / 1000;
        uint32_t remainder = kb % 1000;
        snprintf(buf, buf_size, "%lu,%03luK",
                 (unsigned long)thousands, (unsigned long)remainder);
    }
}
