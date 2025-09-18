/*
 * System 7.1 Boot Loader - Hardware Abstraction Layer
 *
 * This HAL provides a platform-neutral interface for boot services,
 * allowing the System 7.1 boot loader to run on modern x86_64 and ARM64
 * targets while preserving the original Mac OS boot sequence.
 *
 * Copyright (c) 2024 - System 7.1 Portable Project
 */

#ifndef BOOT_HAL_H
#define BOOT_HAL_H

#include "../../include/MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Detection and Identification
 * ============================================================================ */

typedef enum {
    kPlatformUnknown = 0,
    kPlatformX86_64 = 1,        /* Intel/AMD 64-bit */
    kPlatformARM64 = 2,         /* ARM 64-bit (Apple Silicon, etc.) */
    kPlatformRISCV = 3,         /* RISC-V 64-bit */
    kPlatformEmulated = 4       /* Running in emulation/VM */
} PlatformType;

typedef enum {
    kBootModeUEFI = 1,          /* UEFI boot environment */
    kBootModeBIOS = 2,          /* Legacy BIOS boot */
    kBootModeLinux = 3,         /* Linux userspace boot */
    kBootModeDirect = 4         /* Direct hardware access */
} BootMode;

typedef struct PlatformInfo {
    PlatformType platform;      /* Platform architecture */
    BootMode boot_mode;         /* Boot environment type */
    char vendor[32];            /* CPU vendor (Intel, AMD, Apple, etc.) */
    char model[64];             /* CPU model string */
    char platform_name[64];     /* Platform name (MacBook Pro, etc.) */
    UInt32 cpu_count;           /* Number of CPU cores */
    UInt64 memory_size;         /* Total system memory in bytes */
    UInt32 page_size;           /* Memory page size */
    Boolean has_uefi;           /* UEFI firmware available */
    Boolean has_acpi;           /* ACPI tables available */
    Boolean has_devicetree;     /* Device tree available */
    Boolean is_virtualized;     /* Running in VM */
    UInt32 reserved[4];         /* Reserved for future use */
} PlatformInfo;

/* ============================================================================
 * Modern Device Types and Information
 * ============================================================================ */

typedef enum {
    kModernDeviceUnknown = 0,
    kModernDeviceNVMe = 1,      /* NVMe SSD */
    kModernDeviceSATA = 2,      /* SATA drive (SSD/HDD) */
    kModernDeviceUSB = 3,       /* USB storage device */
    kModernDeviceSD = 4,        /* SD/MMC card */
    kModernDeviceNetwork = 5,   /* Network boot device */
    kModernDeviceVirtual = 6,   /* Virtual/container storage */
    kModernDeviceRAM = 7,       /* RAM disk */
    kModernDeviceOptical = 8    /* CD/DVD/Blu-ray */
} ModernDeviceType;

typedef struct ModernDeviceInfo {
    ModernDeviceType type;      /* Device type */
    char device_path[256];      /* System device path (/dev/nvme0n1, etc.) */
    char label[64];             /* Human-readable device label */
    char filesystem[16];        /* Filesystem type (HFS+, APFS, ext4, etc.) */
    UInt64 size_bytes;          /* Total device size */
    UInt64 free_bytes;          /* Available free space */
    UInt32 block_size;          /* Block size in bytes */
    Boolean is_removable;       /* Can be ejected/removed */
    Boolean is_bootable;        /* Contains boot signature */
    Boolean has_system_folder;  /* Contains Mac OS system files */
    Boolean is_mounted;         /* Currently mounted */
    UInt32 boot_priority;       /* Boot preference (1=highest) */
    void* platform_data;       /* Platform-specific device data */
    UInt32 reserved[4];         /* Reserved for future use */
} ModernDeviceInfo;

/* ============================================================================
 * Memory Management Interface
 * ============================================================================ */

typedef struct MemoryInfo {
    UInt64 total_memory;        /* Total system memory */
    UInt64 available_memory;    /* Available for allocation */
    UInt64 kernel_memory;       /* Used by kernel/system */
    UInt64 cached_memory;       /* Used for caching */
    UInt32 page_size;           /* Memory page size */
    Boolean has_virtual_memory; /* Virtual memory support */
    Boolean has_memory_protection; /* Memory protection support */
} MemoryInfo;

/* Memory allocation flags */
#define kMemoryFlagZero         0x01    /* Zero-initialize memory */
#define kMemoryFlagExecutable   0x02    /* Mark as executable */
#define kMemoryFlagReadOnly     0x04    /* Mark as read-only */
#define kMemoryFlagContiguous   0x08    /* Physically contiguous */

/* ============================================================================
 * Boot Services Interface
 * ============================================================================ */

typedef struct BootServices {
    /* Platform information */
    OSErr (*GetPlatformInfo)(PlatformInfo* info);
    OSErr (*ValidatePlatformRequirements)(void);

    /* Memory management */
    OSErr (*GetMemoryInfo)(MemoryInfo* info);
    OSErr (*AllocateMemory)(Size size, UInt32 flags, void** ptr);
    OSErr (*FreeMemory)(void* ptr);
    OSErr (*ReallocateMemory)(void* old_ptr, Size new_size, void** new_ptr);

    /* Device discovery and management */
    OSErr (*EnumerateStorageDevices)(ModernDeviceInfo** devices, UInt32* count);
    OSErr (*MountDevice)(ModernDeviceInfo* device);
    OSErr (*UnmountDevice)(ModernDeviceInfo* device);
    OSErr (*ValidateBootDevice)(ModernDeviceInfo* device);

    /* File system operations */
    OSErr (*LoadFile)(const char* path, void** data, Size* size);
    OSErr (*FileExists)(const char* path, Boolean* exists);
    OSErr (*GetFileInfo)(const char* path, UInt64* size, Boolean* is_directory);

    /* System image loading */
    OSErr (*LoadSystemImage)(const char* path, void** image, Size* size);
    OSErr (*ValidateSystemImage)(void* image, Size size);
    OSErr (*LaunchApplication)(const char* path, void** handle);

    /* Resource management */
    OSErr (*LoadResource)(ResType type, ResID id, void** data, Size* size);
    OSErr (*EnumerateResources)(ResType type, ResID** ids, UInt32* count);

    /* Platform-specific services */
    OSErr (*ExitBootServices)(void);
    OSErr (*Reboot)(void);
    OSErr (*Shutdown)(void);

    /* Reserved for future expansion */
    void* reserved[8];
} BootServices;

/* ============================================================================
 * HAL Interface Functions
 * ============================================================================ */

/* Initialize the boot HAL for the current platform */
OSErr HAL_Initialize(void);

/* Get the boot services interface */
OSErr HAL_GetBootServices(BootServices** services);

/* Detect the current platform */
OSErr HAL_DetectPlatform(PlatformType* platform, BootMode* boot_mode);

/* Cleanup and shutdown HAL */
OSErr HAL_Shutdown(void);

/* ============================================================================
 * Platform-Specific Initialization Functions
 * ============================================================================ */

/* These functions are implemented by platform-specific modules */

#ifdef PLATFORM_X86_64
OSErr X86_64_InitializeBootServices(BootServices* services);
OSErr X86_64_InitializeUEFI(void);
OSErr X86_64_InitializeACPI(void);
#endif

#ifdef PLATFORM_ARM64
OSErr ARM64_InitializeBootServices(BootServices* services);
OSErr ARM64_ParseDeviceTree(void);
OSErr ARM64_InitializeDevices(void);
#endif

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Convert Mac OS device types to modern equivalents */
ModernDeviceType ConvertClassicDeviceType(UInt32 classic_type);

/* Convert modern device info to classic format */
OSErr ConvertToClassicDeviceInfo(ModernDeviceInfo* modern, DeviceSpec* classic);

/* Check if a path represents a valid Mac OS system folder */
Boolean IsValidSystemFolder(const char* path);

/* Get system folder path on a device */
OSErr GetSystemFolderPath(ModernDeviceInfo* device, char* path, Size path_size);

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define kHALNotInitialized      -6000   /* HAL not initialized */
#define kHALPlatformNotSupported -6001  /* Platform not supported */
#define kHALDeviceNotFound      -6002   /* Device not found */
#define kHALInvalidParameter    -6003   /* Invalid parameter */
#define kHALInsufficientMemory  -6004   /* Insufficient memory */
#define kHALOperationFailed     -6005   /* Operation failed */
#define kHALNotImplemented      -6006   /* Feature not implemented */

/* ============================================================================
 * Debug and Diagnostics
 * ============================================================================ */

typedef enum {
    kLogLevelError = 1,
    kLogLevelWarning = 2,
    kLogLevelInfo = 3,
    kLogLevelDebug = 4
} LogLevel;

/* Logging interface */
void HAL_Log(LogLevel level, const char* format, ...);
void HAL_SetLogLevel(LogLevel level);

/* Diagnostic functions */
OSErr HAL_RunDiagnostics(void);
OSErr HAL_DumpPlatformInfo(void);
OSErr HAL_DumpDeviceInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_HAL_H */