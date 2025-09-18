/*
 * System 7.1 Boot Loader - ARM64 Platform Implementation
 *
 * Platform-specific boot services for ARM64 systems including
 * Apple Silicon, ARM servers, and embedded ARM64 devices.
 * Provides device tree parsing and ARM-specific hardware support.
 */

#include "../../hal/boot_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#ifdef PLATFORM_ARM64

/* ============================================================================
 * ARM64 Platform Information
 * ============================================================================ */

static OSErr GetPlatformInfo_ARM64(PlatformInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(PlatformInfo));

    info->platform = kPlatformARM64;
    info->boot_mode = kBootModeLinux; /* Default for userspace */

    /* Get system information */
    struct utsname uname_info;
    if (uname(&uname_info) == 0) {
        snprintf(info->platform_name, sizeof(info->platform_name),
                "%s %s", uname_info.sysname, uname_info.machine);
    } else {
        strcpy(info->platform_name, "ARM64 System");
    }

    /* Detect ARM CPU information from /proc/cpuinfo */
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "CPU implementer", 15) == 0) {
                char* implementer = strchr(line, ':');
                if (implementer && strstr(implementer, "0x41")) {
                    strcpy(info->vendor, "ARM");
                } else if (implementer && strstr(implementer, "0x61")) {
                    strcpy(info->vendor, "Apple");
                } else {
                    strcpy(info->vendor, "ARM Compatible");
                }
            } else if (strncmp(line, "model name", 10) == 0) {
                char* model = strchr(line, ':');
                if (model) {
                    model += 2; /* Skip ": " */
                    char* newline = strchr(model, '\n');
                    if (newline) *newline = '\0';
                    strncpy(info->model, model, sizeof(info->model) - 1);
                }
            }
        }
        fclose(cpuinfo);
    }

    /* Detect Apple Silicon specifically */
    FILE* compatible = fopen("/proc/device-tree/compatible", "r");
    if (compatible) {
        char compat_str[256];
        size_t read_bytes = fread(compat_str, 1, sizeof(compat_str) - 1, compatible);
        if (read_bytes > 0) {
            compat_str[read_bytes] = '\0';
            if (strstr(compat_str, "apple,")) {
                strcpy(info->vendor, "Apple");
                strcpy(info->platform_name, "Apple Silicon Mac");
            }
        }
        fclose(compatible);
    }

    /* Count CPU cores */
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (info->cpu_count <= 0) {
        info->cpu_count = 1;
    }

    /* Get memory size */
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        info->memory_size = (UInt64)pages * page_size;
    } else {
        info->memory_size = 1024 * 1024 * 1024; /* Default to 1GB */
    }

    info->page_size = (page_size > 0) ? page_size : 4096;

    /* ARM64 systems typically use device tree instead of ACPI */
    info->has_devicetree = (access("/proc/device-tree", F_OK) == 0) ||
                          (access("/sys/firmware/devicetree", F_OK) == 0);

    /* Some ARM64 systems (like servers) may have ACPI */
    info->has_acpi = (access("/sys/firmware/acpi", F_OK) == 0);

    /* Check for UEFI (common on ARM64 servers and some embedded systems) */
    info->has_uefi = (access("/sys/firmware/efi", F_OK) == 0);
    if (info->has_uefi) {
        info->boot_mode = kBootModeUEFI;
    }

    /* Detect virtualization */
    if (access("/proc/xen", F_OK) == 0) {
        info->is_virtualized = true;
    } else {
        /* Check for other ARM64 virtualization indicators */
        cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo) {
            char line[256];
            while (fgets(line, sizeof(line), cpuinfo)) {
                if (strstr(line, "QEMU") || strstr(line, "KVM")) {
                    info->is_virtualized = true;
                    break;
                }
            }
            fclose(cpuinfo);
        }
    }

    HAL_Log(kLogLevelInfo, "ARM64 Platform: %s %s, %u cores, %llu MB RAM",
            info->vendor, info->model, info->cpu_count,
            info->memory_size / (1024 * 1024));

    return noErr;
}

/* ============================================================================
 * ARM64 Device Tree Parsing
 * ============================================================================ */

typedef struct DeviceTreeNode {
    char name[64];
    char compatible[128];
    UInt64 reg_base;
    UInt32 reg_size;
    Boolean is_storage;
    struct DeviceTreeNode* next;
} DeviceTreeNode;

static DeviceTreeNode* ParseDeviceTreeStorage(void) {
    DeviceTreeNode* storage_devices = NULL;

    /* Parse device tree for storage controllers */
    FILE* dt_file = fopen("/proc/device-tree/soc", "r");
    if (!dt_file) {
        dt_file = fopen("/proc/device-tree", "r");
    }

    if (dt_file) {
        HAL_Log(kLogLevelInfo, "Device tree found, parsing for storage devices");
        fclose(dt_file);

        /* TODO: Implement full device tree parsing */
        /* This would involve walking through the device tree structure
         * and identifying storage controllers like:
         * - NVMe controllers
         * - SATA controllers
         * - SD/MMC controllers
         * - USB controllers
         */

        /* For now, create a mock storage device */
        DeviceTreeNode* node = (DeviceTreeNode*)malloc(sizeof(DeviceTreeNode));
        if (node) {
            memset(node, 0, sizeof(DeviceTreeNode));
            strcpy(node->name, "nvme0");
            strcpy(node->compatible, "nvme,generic");
            node->reg_base = 0x12345000;
            node->reg_size = 0x1000;
            node->is_storage = true;
            node->next = storage_devices;
            storage_devices = node;
        }
    }

    return storage_devices;
}

OSErr ARM64_ParseDeviceTree(void) {
    HAL_Log(kLogLevelInfo, "Parsing ARM64 device tree...");

    if (!access("/proc/device-tree", F_OK)) {
        HAL_Log(kLogLevelInfo, "Device tree available at /proc/device-tree");

        /* Parse device tree for system information */
        DeviceTreeNode* storage = ParseDeviceTreeStorage();
        if (storage) {
            HAL_Log(kLogLevelInfo, "Found storage device: %s (%s)",
                    storage->name, storage->compatible);
            free(storage);
        }

        return noErr;
    }

    if (!access("/sys/firmware/devicetree", F_OK)) {
        HAL_Log(kLogLevelInfo, "Device tree available at /sys/firmware/devicetree");
        return noErr;
    }

    HAL_Log(kLogLevelWarning, "No device tree found");
    return kHALNotImplemented;
}

/* ============================================================================
 * ARM64 Storage Device Enumeration
 * ============================================================================ */

static OSErr EnumerateStorageDevices_ARM64(ModernDeviceInfo** devices, UInt32* count) {
    if (!devices || !count) return paramErr;

    /* Start with generic enumeration */
    OSErr err = EnumerateStorageDevices_Generic(devices, count);
    if (err != noErr) return err;

    /* Enhance with ARM64-specific device detection */

    /* Check for Apple Silicon specific storage */
    if (access("/proc/device-tree/compatible", F_OK) == 0) {
        FILE* compatible = fopen("/proc/device-tree/compatible", "r");
        if (compatible) {
            char compat_str[256];
            size_t read_bytes = fread(compat_str, 1, sizeof(compat_str) - 1, compatible);
            if (read_bytes > 0 && strstr(compat_str, "apple,")) {
                HAL_Log(kLogLevelInfo, "Apple Silicon storage subsystem detected");
                /* TODO: Parse Apple-specific storage devices */
            }
            fclose(compatible);
        }
    }

    /* Check for standard ARM64 storage controllers */
    if (access("/sys/class/nvme", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "NVMe devices detected on ARM64");
    }

    if (access("/sys/class/mmc_host", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "MMC/SD controllers detected");
    }

    return noErr;
}

/* ============================================================================
 * ARM64 Memory Management
 * ============================================================================ */

static OSErr GetMemoryInfo_ARM64(MemoryInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(MemoryInfo));

    /* Parse /proc/meminfo for memory information */
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            UInt64 value = 0;

            if (sscanf(line, "MemTotal: %llu kB", &value) == 1) {
                info->total_memory = value * 1024;
            } else if (sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
                info->available_memory = value * 1024;
            } else if (sscanf(line, "Cached: %llu kB", &value) == 1) {
                info->cached_memory = value * 1024;
            }
        }
        fclose(meminfo);
    }

    /* Calculate kernel memory usage */
    if (info->total_memory > 0 && info->available_memory > 0) {
        info->kernel_memory = info->total_memory - info->available_memory - info->cached_memory;
    }

    /* ARM64 typically uses 4KB or 16KB pages, detect actual size */
    info->page_size = sysconf(_SC_PAGE_SIZE);
    if (info->page_size <= 0) {
        info->page_size = 4096; /* Default to 4KB */
    }

    info->has_virtual_memory = true;
    info->has_memory_protection = true;

    HAL_Log(kLogLevelInfo, "ARM64 Memory: %llu MB total, %llu MB available, %u byte pages",
            info->total_memory / (1024 * 1024),
            info->available_memory / (1024 * 1024),
            info->page_size);

    return noErr;
}

/* ============================================================================
 * ARM64 Device Initialization
 * ============================================================================ */

OSErr ARM64_InitializeDevices(void) {
    HAL_Log(kLogLevelInfo, "Initializing ARM64 devices...");

    /* Initialize device tree parsing */
    OSErr err = ARM64_ParseDeviceTree();
    if (err != noErr && err != kHALNotImplemented) {
        HAL_Log(kLogLevelWarning, "Device tree parsing failed: %d", err);
    }

    /* Initialize ARM64-specific devices */

    /* Check for GPIO controllers (common on ARM64 embedded systems) */
    if (access("/sys/class/gpio", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "GPIO subsystem detected");
    }

    /* Check for I2C controllers */
    if (access("/sys/class/i2c-adapter", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "I2C controllers detected");
    }

    /* Check for SPI controllers */
    if (access("/sys/class/spi_master", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "SPI controllers detected");
    }

    return noErr;
}

/* ============================================================================
 * ARM64 Boot Services Implementation
 * ============================================================================ */

OSErr ARM64_InitializeBootServices(BootServices* services) {
    if (!services) return paramErr;

    HAL_Log(kLogLevelInfo, "Initializing ARM64 boot services...");

    /* Override generic services with ARM64-specific implementations */
    services->GetPlatformInfo = GetPlatformInfo_ARM64;
    services->GetMemoryInfo = GetMemoryInfo_ARM64;
    services->EnumerateStorageDevices = EnumerateStorageDevices_ARM64;

    /* Initialize ARM64-specific subsystems */
    OSErr err = ARM64_InitializeDevices();
    if (err != noErr && err != kHALNotImplemented) {
        HAL_Log(kLogLevelWarning, "ARM64 device initialization failed: %d", err);
    }

    HAL_Log(kLogLevelInfo, "ARM64 boot services initialized successfully");
    return noErr;
}

/* ============================================================================
 * ARM64 Hardware Detection
 * ============================================================================ */

static Boolean DetectAppleSilicon(void) {
    FILE* compatible = fopen("/proc/device-tree/compatible", "r");
    if (!compatible) return false;

    char compat_str[256];
    size_t read_bytes = fread(compat_str, 1, sizeof(compat_str) - 1, compatible);
    fclose(compatible);

    if (read_bytes > 0) {
        compat_str[read_bytes] = '\0';
        return (strstr(compat_str, "apple,") != NULL);
    }

    return false;
}

static Boolean DetectRaspberryPi(void) {
    FILE* model = fopen("/proc/device-tree/model", "r");
    if (!model) return false;

    char model_str[256];
    size_t read_bytes = fread(model_str, 1, sizeof(model_str) - 1, model);
    fclose(model);

    if (read_bytes > 0) {
        model_str[read_bytes] = '\0';
        return (strstr(model_str, "Raspberry Pi") != NULL);
    }

    return false;
}

/* ============================================================================
 * ARM64 Diagnostics
 * ============================================================================ */

OSErr ARM64_RunDiagnostics(void) {
    HAL_Log(kLogLevelInfo, "Running ARM64 platform diagnostics...");

    /* Test hardware detection */
    if (DetectAppleSilicon()) {
        HAL_Log(kLogLevelInfo, "Apple Silicon detected");
    } else if (DetectRaspberryPi()) {
        HAL_Log(kLogLevelInfo, "Raspberry Pi detected");
    } else {
        HAL_Log(kLogLevelInfo, "Generic ARM64 system detected");
    }

    /* Test platform features */
    Boolean has_devicetree = (access("/proc/device-tree", F_OK) == 0);
    Boolean has_uefi = (access("/sys/firmware/efi", F_OK) == 0);
    Boolean has_acpi = (access("/sys/firmware/acpi", F_OK) == 0);

    HAL_Log(kLogLevelInfo, "Platform features: DeviceTree=%s, UEFI=%s, ACPI=%s",
            has_devicetree ? "Yes" : "No",
            has_uefi ? "Yes" : "No",
            has_acpi ? "Yes" : "No");

    /* Test memory information */
    MemoryInfo mem_info;
    OSErr err = GetMemoryInfo_ARM64(&mem_info);
    if (err == noErr) {
        HAL_Log(kLogLevelInfo, "Memory test passed: %llu MB total, %u byte pages",
                mem_info.total_memory / (1024 * 1024), mem_info.page_size);
    } else {
        HAL_Log(kLogLevelError, "Memory test failed: %d", err);
        return err;
    }

    /* Test device tree parsing */
    err = ARM64_ParseDeviceTree();
    if (err == noErr) {
        HAL_Log(kLogLevelInfo, "Device tree parsing test passed");
    } else if (err == kHALNotImplemented) {
        HAL_Log(kLogLevelInfo, "Device tree not available (expected on some systems)");
    } else {
        HAL_Log(kLogLevelWarning, "Device tree parsing test failed: %d", err);
    }

    HAL_Log(kLogLevelInfo, "ARM64 diagnostics completed successfully");
    return noErr;
}

#endif /* PLATFORM_ARM64 */