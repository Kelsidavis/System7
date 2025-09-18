/*
 * System 7.1 Boot Loader - Hardware Abstraction Layer Implementation
 *
 * Platform-neutral HAL implementation that provides boot services
 * for the System 7.1 boot loader on modern x86_64 and ARM64 targets.
 */

#include "boot_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static Boolean g_hal_initialized = false;
static PlatformType g_platform_type = kPlatformUnknown;
static BootMode g_boot_mode = kBootModeDirect;
static BootServices g_boot_services = {0};
static LogLevel g_log_level = kLogLevelInfo;

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

OSErr HAL_DetectPlatform(PlatformType* platform, BootMode* boot_mode) {
    if (!platform || !boot_mode) {
        return paramErr;
    }

    /* Detect CPU architecture */
#if defined(__x86_64__) || defined(__amd64__)
    *platform = kPlatformX86_64;
#elif defined(__aarch64__) || defined(__arm64__)
    *platform = kPlatformARM64;
#elif defined(__riscv) && (__riscv_xlen == 64)
    *platform = kPlatformRISCV;
#else
    *platform = kPlatformUnknown;
    return kHALPlatformNotSupported;
#endif

    /* Detect boot environment */
    /* This is a simplified detection - real implementation would check
     * for UEFI runtime services, ACPI tables, etc. */
    if (getenv("UEFI_BOOT") != NULL) {
        *boot_mode = kBootModeUEFI;
    } else if (getenv("BIOS_BOOT") != NULL) {
        *boot_mode = kBootModeBIOS;
    } else {
        *boot_mode = kBootModeLinux; /* Default to Linux userspace */
    }

    HAL_Log(kLogLevelInfo, "Detected platform: %s, boot mode: %s",
            (*platform == kPlatformX86_64) ? "x86_64" :
            (*platform == kPlatformARM64) ? "ARM64" : "Unknown",
            (*boot_mode == kBootModeUEFI) ? "UEFI" :
            (*boot_mode == kBootModeBIOS) ? "BIOS" : "Linux");

    return noErr;
}

/* ============================================================================
 * Generic Boot Services Implementation
 * ============================================================================ */

static OSErr GetPlatformInfo_Generic(PlatformInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(PlatformInfo));

    info->platform = g_platform_type;
    info->boot_mode = g_boot_mode;

    /* Platform-specific information */
    switch (g_platform_type) {
        case kPlatformX86_64:
            strcpy(info->vendor, "Intel/AMD");
            strcpy(info->model, "x86_64 CPU");
            info->has_acpi = true;
            break;

        case kPlatformARM64:
            strcpy(info->vendor, "ARM");
            strcpy(info->model, "ARM64 CPU");
            info->has_devicetree = true;
            break;

        default:
            strcpy(info->vendor, "Unknown");
            strcpy(info->model, "Unknown CPU");
            break;
    }

    strcpy(info->platform_name, "Generic Platform");
    info->cpu_count = 1; /* Default to single core */
    info->memory_size = 1024 * 1024 * 1024; /* Default to 1GB */
    info->page_size = 4096; /* Standard page size */
    info->has_uefi = (g_boot_mode == kBootModeUEFI);
    info->is_virtualized = false; /* TODO: Detect virtualization */

    return noErr;
}

static OSErr ValidatePlatformRequirements_Generic(void) {
    PlatformInfo info;
    OSErr err = GetPlatformInfo_Generic(&info);
    if (err != noErr) return err;

    /* Check minimum requirements */
    if (info.memory_size < 64 * 1024 * 1024) { /* 64MB minimum */
        HAL_Log(kLogLevelError, "Insufficient memory: %llu MB required",
                info.memory_size / (1024 * 1024));
        return kHALInsufficientMemory;
    }

    if (info.platform == kPlatformUnknown) {
        HAL_Log(kLogLevelError, "Unsupported platform");
        return kHALPlatformNotSupported;
    }

    HAL_Log(kLogLevelInfo, "Platform requirements validated");
    return noErr;
}

static OSErr GetMemoryInfo_Generic(MemoryInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(MemoryInfo));

    /* Default memory information */
    info->total_memory = 1024 * 1024 * 1024; /* 1GB */
    info->available_memory = 768 * 1024 * 1024; /* 768MB */
    info->kernel_memory = 128 * 1024 * 1024; /* 128MB */
    info->cached_memory = 128 * 1024 * 1024; /* 128MB */
    info->page_size = 4096;
    info->has_virtual_memory = true;
    info->has_memory_protection = true;

    return noErr;
}

static OSErr AllocateMemory_Generic(Size size, UInt32 flags, void** ptr) {
    if (!ptr || size == 0) return paramErr;

    void* memory = malloc(size);
    if (!memory) {
        HAL_Log(kLogLevelError, "Failed to allocate %u bytes", size);
        return memFullErr;
    }

    if (flags & kMemoryFlagZero) {
        memset(memory, 0, size);
    }

    *ptr = memory;
    HAL_Log(kLogLevelDebug, "Allocated %u bytes at %p", size, memory);
    return noErr;
}

static OSErr FreeMemory_Generic(void* ptr) {
    if (!ptr) return paramErr;

    free(ptr);
    HAL_Log(kLogLevelDebug, "Freed memory at %p", ptr);
    return noErr;
}

static OSErr ReallocateMemory_Generic(void* old_ptr, Size new_size, void** new_ptr) {
    if (!new_ptr || new_size == 0) return paramErr;

    void* memory = realloc(old_ptr, new_size);
    if (!memory) {
        HAL_Log(kLogLevelError, "Failed to reallocate to %u bytes", new_size);
        return memFullErr;
    }

    *new_ptr = memory;
    HAL_Log(kLogLevelDebug, "Reallocated from %p to %p (%u bytes)", old_ptr, memory, new_size);
    return noErr;
}

static OSErr EnumerateStorageDevices_Generic(ModernDeviceInfo** devices, UInt32* count) {
    if (!devices || !count) return paramErr;

    /* Create a simple mock device list for testing */
    *count = 2;
    *devices = (ModernDeviceInfo*)malloc(sizeof(ModernDeviceInfo) * 2);
    if (!*devices) return memFullErr;

    /* Mock device 1: NVMe SSD */
    ModernDeviceInfo* dev1 = &(*devices)[0];
    memset(dev1, 0, sizeof(ModernDeviceInfo));
    dev1->type = kModernDeviceNVMe;
    strcpy(dev1->device_path, "/dev/nvme0n1");
    strcpy(dev1->label, "System SSD");
    strcpy(dev1->filesystem, "APFS");
    dev1->size_bytes = 256ULL * 1024 * 1024 * 1024; /* 256GB */
    dev1->free_bytes = 128ULL * 1024 * 1024 * 1024; /* 128GB free */
    dev1->block_size = 4096;
    dev1->is_bootable = true;
    dev1->has_system_folder = true;
    dev1->boot_priority = 1;

    /* Mock device 2: USB drive */
    ModernDeviceInfo* dev2 = &(*devices)[1];
    memset(dev2, 0, sizeof(ModernDeviceInfo));
    dev2->type = kModernDeviceUSB;
    strcpy(dev2->device_path, "/dev/sdb1");
    strcpy(dev2->label, "USB Drive");
    strcpy(dev2->filesystem, "HFS+");
    dev2->size_bytes = 32ULL * 1024 * 1024 * 1024; /* 32GB */
    dev2->free_bytes = 16ULL * 1024 * 1024 * 1024; /* 16GB free */
    dev2->block_size = 4096;
    dev2->is_removable = true;
    dev2->is_bootable = false;
    dev2->boot_priority = 2;

    HAL_Log(kLogLevelInfo, "Enumerated %u storage devices", *count);
    return noErr;
}

static OSErr LoadFile_Generic(const char* path, void** data, Size* size) {
    if (!path || !data || !size) return paramErr;

    FILE* file = fopen(path, "rb");
    if (!file) {
        HAL_Log(kLogLevelError, "Failed to open file: %s", path);
        return fnfErr;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return ioErr;
    }

    /* Allocate memory */
    void* buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        return memFullErr;
    }

    /* Read file */
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        free(buffer);
        HAL_Log(kLogLevelError, "Failed to read file: %s", path);
        return ioErr;
    }

    *data = buffer;
    *size = file_size;
    HAL_Log(kLogLevelInfo, "Loaded file: %s (%ld bytes)", path, file_size);
    return noErr;
}

static OSErr FileExists_Generic(const char* path, Boolean* exists) {
    if (!path || !exists) return paramErr;

    FILE* file = fopen(path, "r");
    *exists = (file != NULL);
    if (file) fclose(file);

    return noErr;
}

/* ============================================================================
 * HAL Initialization
 * ============================================================================ */

OSErr HAL_Initialize(void) {
    if (g_hal_initialized) {
        return noErr; /* Already initialized */
    }

    HAL_Log(kLogLevelInfo, "Initializing Boot HAL...");

    /* Detect platform */
    OSErr err = HAL_DetectPlatform(&g_platform_type, &g_boot_mode);
    if (err != noErr) {
        HAL_Log(kLogLevelError, "Platform detection failed: %d", err);
        return err;
    }

    /* Initialize generic boot services */
    memset(&g_boot_services, 0, sizeof(BootServices));
    g_boot_services.GetPlatformInfo = GetPlatformInfo_Generic;
    g_boot_services.ValidatePlatformRequirements = ValidatePlatformRequirements_Generic;
    g_boot_services.GetMemoryInfo = GetMemoryInfo_Generic;
    g_boot_services.AllocateMemory = AllocateMemory_Generic;
    g_boot_services.FreeMemory = FreeMemory_Generic;
    g_boot_services.ReallocateMemory = ReallocateMemory_Generic;
    g_boot_services.EnumerateStorageDevices = EnumerateStorageDevices_Generic;
    g_boot_services.LoadFile = LoadFile_Generic;
    g_boot_services.FileExists = FileExists_Generic;

    /* Platform-specific initialization */
#ifdef PLATFORM_X86_64
    if (g_platform_type == kPlatformX86_64) {
        err = X86_64_InitializeBootServices(&g_boot_services);
        if (err != noErr) {
            HAL_Log(kLogLevelWarning, "x86_64 initialization failed, using generic services");
        }
    }
#endif

#ifdef PLATFORM_ARM64
    if (g_platform_type == kPlatformARM64) {
        err = ARM64_InitializeBootServices(&g_boot_services);
        if (err != noErr) {
            HAL_Log(kLogLevelWarning, "ARM64 initialization failed, using generic services");
        }
    }
#endif

    g_hal_initialized = true;
    HAL_Log(kLogLevelInfo, "Boot HAL initialized successfully");
    return noErr;
}

OSErr HAL_GetBootServices(BootServices** services) {
    if (!services) return paramErr;
    if (!g_hal_initialized) return kHALNotInitialized;

    *services = &g_boot_services;
    return noErr;
}

OSErr HAL_Shutdown(void) {
    if (!g_hal_initialized) {
        return noErr; /* Already shutdown */
    }

    HAL_Log(kLogLevelInfo, "Shutting down Boot HAL...");

    /* Cleanup platform-specific resources */
    memset(&g_boot_services, 0, sizeof(BootServices));
    g_hal_initialized = false;
    g_platform_type = kPlatformUnknown;
    g_boot_mode = kBootModeDirect;

    HAL_Log(kLogLevelInfo, "Boot HAL shutdown complete");
    return noErr;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

ModernDeviceType ConvertClassicDeviceType(UInt32 classic_type) {
    switch (classic_type) {
        case 1: /* kDiskTypeSony */
            return kModernDeviceUSB; /* Map floppy to USB */
        case 2: /* kDiskTypeEDisk */
            return kModernDeviceRAM; /* Map EDisk to RAM disk */
        case 3: /* kDiskTypeHardDisk */
            return kModernDeviceNVMe; /* Map hard disk to NVMe */
        default:
            return kModernDeviceUnknown;
    }
}

Boolean IsValidSystemFolder(const char* path) {
    if (!path) return false;

    char system_path[512];
    snprintf(system_path, sizeof(system_path), "%s/System", path);

    Boolean exists = false;
    FileExists_Generic(system_path, &exists);
    return exists;
}

/* ============================================================================
 * Logging and Diagnostics
 * ============================================================================ */

void HAL_Log(LogLevel level, const char* format, ...) {
    if (level > g_log_level) return;

    const char* level_str[] = {"", "ERROR", "WARN", "INFO", "DEBUG"};
    printf("[HAL-%s] ", level_str[level]);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

void HAL_SetLogLevel(LogLevel level) {
    g_log_level = level;
    HAL_Log(kLogLevelInfo, "Log level set to %d", level);
}

OSErr HAL_RunDiagnostics(void) {
    HAL_Log(kLogLevelInfo, "Running Boot HAL diagnostics...");

    /* Test platform detection */
    PlatformType platform;
    BootMode boot_mode;
    OSErr err = HAL_DetectPlatform(&platform, &boot_mode);
    if (err != noErr) {
        HAL_Log(kLogLevelError, "Platform detection failed");
        return err;
    }

    /* Test memory allocation */
    void* test_ptr = NULL;
    err = AllocateMemory_Generic(1024, kMemoryFlagZero, &test_ptr);
    if (err != noErr) {
        HAL_Log(kLogLevelError, "Memory allocation test failed");
        return err;
    }
    FreeMemory_Generic(test_ptr);

    /* Test device enumeration */
    ModernDeviceInfo* devices = NULL;
    UInt32 count = 0;
    err = EnumerateStorageDevices_Generic(&devices, &count);
    if (err != noErr) {
        HAL_Log(kLogLevelError, "Device enumeration test failed");
        return err;
    }
    if (devices) free(devices);

    HAL_Log(kLogLevelInfo, "All diagnostics passed");
    return noErr;
}