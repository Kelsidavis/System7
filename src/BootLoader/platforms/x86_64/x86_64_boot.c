/*
 * System 7.1 Boot Loader - x86_64 Platform Implementation
 *
 * Platform-specific boot services for Intel/AMD x86_64 systems.
 * Provides UEFI integration, ACPI device discovery, and modern
 * x86_64 hardware support.
 */

#include "../../hal/boot_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#ifdef PLATFORM_X86_64

/* ============================================================================
 * x86_64 Platform Information
 * ============================================================================ */

static OSErr GetPlatformInfo_X86_64(PlatformInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(PlatformInfo));

    info->platform = kPlatformX86_64;
    info->boot_mode = kBootModeLinux; /* Default for userspace */

    /* Get system information */
    struct utsname uname_info;
    if (uname(&uname_info) == 0) {
        snprintf(info->platform_name, sizeof(info->platform_name),
                "%s %s", uname_info.sysname, uname_info.machine);
    } else {
        strcpy(info->platform_name, "x86_64 System");
    }

    /* Detect CPU vendor from /proc/cpuinfo */
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "vendor_id", 9) == 0) {
                char* vendor = strchr(line, ':');
                if (vendor) {
                    vendor += 2; /* Skip ": " */
                    /* Remove newline */
                    char* newline = strchr(vendor, '\n');
                    if (newline) *newline = '\0';

                    if (strstr(vendor, "GenuineIntel")) {
                        strcpy(info->vendor, "Intel");
                    } else if (strstr(vendor, "AuthenticAMD")) {
                        strcpy(info->vendor, "AMD");
                    } else {
                        strncpy(info->vendor, vendor, sizeof(info->vendor) - 1);
                    }
                }
                break;
            }
        }
        fclose(cpuinfo);
    }

    /* Get CPU model */
    cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* model = strchr(line, ':');
                if (model) {
                    model += 2; /* Skip ": " */
                    /* Remove newline */
                    char* newline = strchr(model, '\n');
                    if (newline) *newline = '\0';
                    strncpy(info->model, model, sizeof(info->model) - 1);
                }
                break;
            }
        }
        fclose(cpuinfo);
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

    /* Check for UEFI */
    info->has_uefi = (access("/sys/firmware/efi", F_OK) == 0);
    if (info->has_uefi) {
        info->boot_mode = kBootModeUEFI;
    }

    /* Check for ACPI */
    info->has_acpi = (access("/sys/firmware/acpi", F_OK) == 0) ||
                     (access("/proc/acpi", F_OK) == 0);

    /* Check for virtualization */
    cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strstr(line, "hypervisor") || strstr(line, "VMware") ||
                strstr(line, "VirtualBox") || strstr(line, "QEMU")) {
                info->is_virtualized = true;
                break;
            }
        }
        fclose(cpuinfo);
    }

    HAL_Log(kLogLevelInfo, "x86_64 Platform: %s %s, %u cores, %llu MB RAM",
            info->vendor, info->model, info->cpu_count,
            info->memory_size / (1024 * 1024));

    return noErr;
}

/* ============================================================================
 * x86_64 Storage Device Enumeration
 * ============================================================================ */

static OSErr EnumerateStorageDevices_X86_64(ModernDeviceInfo** devices, UInt32* count) {
    if (!devices || !count) return paramErr;

    /* Initialize with generic enumeration */
    OSErr err = EnumerateStorageDevices_Generic(devices, count);
    if (err != noErr) return err;

    /* Enhance with x86_64-specific device detection */

    /* Check for NVMe devices */
    if (access("/sys/class/nvme", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "NVMe devices detected");
        /* TODO: Parse /sys/class/nvme for actual NVMe devices */
    }

    /* Check for SATA devices */
    if (access("/sys/class/block", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "Block devices detected");
        /* TODO: Parse /sys/class/block for SATA/IDE devices */
    }

    /* Check for USB storage */
    if (access("/sys/bus/usb", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "USB subsystem detected");
        /* TODO: Parse USB devices for storage */
    }

    return noErr;
}

/* ============================================================================
 * x86_64 Memory Management
 * ============================================================================ */

static OSErr GetMemoryInfo_X86_64(MemoryInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(MemoryInfo));

    /* Parse /proc/meminfo for accurate memory information */
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

    info->page_size = sysconf(_SC_PAGE_SIZE);
    if (info->page_size <= 0) {
        info->page_size = 4096;
    }

    info->has_virtual_memory = true;
    info->has_memory_protection = true;

    HAL_Log(kLogLevelInfo, "x86_64 Memory: %llu MB total, %llu MB available",
            info->total_memory / (1024 * 1024),
            info->available_memory / (1024 * 1024));

    return noErr;
}

/* ============================================================================
 * x86_64 ACPI Support
 * ============================================================================ */

OSErr X86_64_InitializeACPI(void) {
    HAL_Log(kLogLevelInfo, "Initializing x86_64 ACPI support...");

    /* Check for ACPI tables */
    if (access("/sys/firmware/acpi/tables", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "ACPI tables found in sysfs");

        /* TODO: Parse ACPI tables for device information */
        /* Common tables: DSDT, SSDT, FADT, MADT, etc. */

        return noErr;
    }

    if (access("/proc/acpi", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "ACPI information found in procfs");
        return noErr;
    }

    HAL_Log(kLogLevelWarning, "No ACPI support detected");
    return kHALNotImplemented;
}

/* ============================================================================
 * x86_64 UEFI Support
 * ============================================================================ */

OSErr X86_64_InitializeUEFI(void) {
    HAL_Log(kLogLevelInfo, "Initializing x86_64 UEFI support...");

    /* Check for UEFI runtime services */
    if (access("/sys/firmware/efi", F_OK) == 0) {
        HAL_Log(kLogLevelInfo, "UEFI firmware detected");

        /* Check for EFI variables */
        if (access("/sys/firmware/efi/efivars", F_OK) == 0) {
            HAL_Log(kLogLevelInfo, "EFI variables accessible");
        }

        /* Check for EFI system table */
        if (access("/sys/firmware/efi/systab", F_OK) == 0) {
            HAL_Log(kLogLevelInfo, "EFI system table found");
        }

        return noErr;
    }

    HAL_Log(kLogLevelInfo, "No UEFI firmware detected (Legacy BIOS boot)");
    return kHALNotImplemented;
}

/* ============================================================================
 * x86_64 Boot Services Implementation
 * ============================================================================ */

OSErr X86_64_InitializeBootServices(BootServices* services) {
    if (!services) return paramErr;

    HAL_Log(kLogLevelInfo, "Initializing x86_64 boot services...");

    /* Override generic services with x86_64-specific implementations */
    services->GetPlatformInfo = GetPlatformInfo_X86_64;
    services->GetMemoryInfo = GetMemoryInfo_X86_64;
    services->EnumerateStorageDevices = EnumerateStorageDevices_X86_64;

    /* Initialize platform-specific subsystems */
    OSErr err = X86_64_InitializeUEFI();
    if (err != noErr && err != kHALNotImplemented) {
        HAL_Log(kLogLevelWarning, "UEFI initialization failed: %d", err);
    }

    err = X86_64_InitializeACPI();
    if (err != noErr && err != kHALNotImplemented) {
        HAL_Log(kLogLevelWarning, "ACPI initialization failed: %d", err);
    }

    HAL_Log(kLogLevelInfo, "x86_64 boot services initialized successfully");
    return noErr;
}

/* ============================================================================
 * x86_64 Hardware Detection
 * ============================================================================ */

static Boolean DetectIntelCPU(void) {
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) return false;

    char line[256];
    Boolean is_intel = false;

    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            if (strstr(line, "GenuineIntel")) {
                is_intel = true;
            }
            break;
        }
    }

    fclose(cpuinfo);
    return is_intel;
}

static Boolean DetectAMDCPU(void) {
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) return false;

    char line[256];
    Boolean is_amd = false;

    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            if (strstr(line, "AuthenticAMD")) {
                is_amd = true;
            }
            break;
        }
    }

    fclose(cpuinfo);
    return is_amd;
}

/* ============================================================================
 * x86_64 Diagnostics
 * ============================================================================ */

OSErr X86_64_RunDiagnostics(void) {
    HAL_Log(kLogLevelInfo, "Running x86_64 platform diagnostics...");

    /* Test CPU detection */
    if (DetectIntelCPU()) {
        HAL_Log(kLogLevelInfo, "Intel CPU detected");
    } else if (DetectAMDCPU()) {
        HAL_Log(kLogLevelInfo, "AMD CPU detected");
    } else {
        HAL_Log(kLogLevelWarning, "Unknown x86_64 CPU vendor");
    }

    /* Test platform features */
    Boolean has_uefi = (access("/sys/firmware/efi", F_OK) == 0);
    Boolean has_acpi = (access("/sys/firmware/acpi", F_OK) == 0);

    HAL_Log(kLogLevelInfo, "Platform features: UEFI=%s, ACPI=%s",
            has_uefi ? "Yes" : "No", has_acpi ? "Yes" : "No");

    /* Test memory information */
    MemoryInfo mem_info;
    OSErr err = GetMemoryInfo_X86_64(&mem_info);
    if (err == noErr) {
        HAL_Log(kLogLevelInfo, "Memory test passed: %llu MB total",
                mem_info.total_memory / (1024 * 1024));
    } else {
        HAL_Log(kLogLevelError, "Memory test failed: %d", err);
        return err;
    }

    HAL_Log(kLogLevelInfo, "x86_64 diagnostics completed successfully");
    return noErr;
}

#endif /* PLATFORM_X86_64 */