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

static void cpuid(uint32_t leaf, uint32_t r[4]) {
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(leaf), "c"(0));
}

/*
 * The processor this is running on, as it names itself (CPUID), in place of
 * a fixed "Generic x86" and "Intel PC Compatible" whatever the machine.
 */
static char g_cpu_brand[49];
static char g_model[64];

static void detect_cpu(void) {
    uint32_t r[4];
    char vendor[13];
    cpuid(0, r);
    memcpy(vendor, &r[1], 4);
    memcpy(vendor + 4, &r[3], 4);
    memcpy(vendor + 8, &r[2], 4);
    vendor[12] = 0;

    cpuid(0x80000000u, r);
    if (r[0] >= 0x80000004u) {
        uint32_t b[12];
        cpuid(0x80000002u, b);
        cpuid(0x80000003u, b + 4);
        cpuid(0x80000004u, b + 8);
        memcpy(g_cpu_brand, b, 48);
        g_cpu_brand[48] = 0;
        /* Brand strings are often padded at the front */
        char* p = g_cpu_brand;
        while (*p == ' ') p++;
        memmove(g_cpu_brand, p, strlen(p) + 1);
        size_t n = strlen(g_cpu_brand);
        while (n > 0 && g_cpu_brand[n - 1] == ' ') g_cpu_brand[--n] = 0;
    }

    if (g_cpu_brand[0]) {
        g_platform_info.cpu_name = g_cpu_brand;
        g_platform_info.model_string = g_cpu_brand;
    } else {
        const char* maker = strcmp(vendor, "GenuineIntel") == 0 ? "Intel"
                          : strcmp(vendor, "AuthenticAMD") == 0 ? "AMD" : vendor;
        snprintf(g_model, sizeof g_model, "%s PC Compatible", maker);
        g_platform_info.model_string = g_model;
        g_platform_info.cpu_name = g_model;
    }
}

static void platform_info_init(void) {
    if (g_platform_info_initialized) return;
    detect_cpu();

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
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%lu GB", (unsigned long)gb);
    } else if (mb_remainder > 0) {
        /* Show decimal if there's remainder */
        uint32_t decimal = (mb_remainder * 10) / 1024;
        snprintf(g_memory_gb_str, sizeof(g_memory_gb_str), "%lu.%lu GB", (unsigned long)gb, (unsigned long)decimal);
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
        snprintf(buf, buf_size, "%lu,%03luK", (unsigned long)thousands, (unsigned long)remainder);
    }
}
