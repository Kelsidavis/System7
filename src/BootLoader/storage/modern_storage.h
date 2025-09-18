/*
 * System 7.1 Boot Loader - Modern Storage Stack
 *
 * Unified interface for modern storage devices including NVMe, SATA,
 * USB, and virtual storage. Provides Mac OS compatible device discovery
 * and enumeration for x86_64 and ARM64 platforms.
 */

#ifndef MODERN_STORAGE_H
#define MODERN_STORAGE_H

#include "../hal/boot_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Storage Device Discovery
 * ============================================================================ */

/* Enhanced device information with modern capabilities */
typedef struct StorageCapabilities {
    Boolean supports_trim;          /* TRIM/UNMAP support */
    Boolean supports_encryption;    /* Hardware encryption */
    Boolean supports_secure_erase;  /* Secure erase capability */
    Boolean supports_smart;         /* SMART monitoring */
    Boolean is_ssd;                 /* Solid state device */
    Boolean is_nvme;                /* NVMe protocol */
    Boolean is_usb;                 /* USB attached */
    Boolean is_removable;           /* Removable media */
    UInt32 max_transfer_size;       /* Maximum transfer size */
    UInt32 optimal_transfer_size;   /* Optimal transfer size */
    UInt32 queue_depth;             /* Command queue depth */
} StorageCapabilities;

typedef struct ModernStorageDevice {
    ModernDeviceInfo base;          /* Base device information */
    StorageCapabilities caps;       /* Device capabilities */
    char vendor[32];                /* Device vendor */
    char model[64];                 /* Device model */
    char serial[64];                /* Serial number */
    char firmware[16];              /* Firmware version */
    UInt32 sector_size;             /* Sector size in bytes */
    UInt64 sector_count;            /* Total sectors */
    UInt32 temperature;             /* Device temperature (°C) */
    Boolean is_healthy;             /* Device health status */
    void* device_handle;            /* Platform-specific handle */
} ModernStorageDevice;

/* Storage subsystem types */
typedef enum {
    kStorageSubsystemNVMe = 1,      /* NVMe subsystem */
    kStorageSubsystemSATA = 2,      /* SATA/AHCI subsystem */
    kStorageSubsystemUSB = 3,       /* USB mass storage */
    kStorageSubsystemVirtual = 4,   /* Virtual/emulated storage */
    kStorageSubsystemNetwork = 5    /* Network storage (iSCSI, NFS) */
} StorageSubsystemType;

/* ============================================================================
 * Device Enumeration Functions
 * ============================================================================ */

/* Enumerate all storage devices on the system */
OSErr EnumerateAllStorageDevices(ModernStorageDevice** devices, UInt32* count);

/* Enumerate devices by subsystem type */
OSErr EnumerateStorageByType(StorageSubsystemType type,
                           ModernStorageDevice** devices, UInt32* count);

/* Get detailed device information */
OSErr GetStorageDeviceInfo(const char* device_path, ModernStorageDevice* device);

/* Check if device is bootable (has valid boot signature) */
OSErr ValidateBootableDevice(ModernStorageDevice* device, Boolean* is_bootable);

/* Find devices with Mac OS System folders */
OSErr FindSystemFolderDevices(ModernStorageDevice** devices, UInt32* count);

/* ============================================================================
 * NVMe Device Support
 * ============================================================================ */

typedef struct NVMeController {
    char controller_path[256];      /* /dev/nvme0 */
    char namespace_path[256];       /* /dev/nvme0n1 */
    UInt32 controller_id;           /* Controller ID */
    UInt32 namespace_id;            /* Namespace ID */
    UInt64 capacity;                /* Namespace capacity */
    UInt32 lba_size;                /* LBA size */
    Boolean supports_dsm;           /* Dataset Management */
    Boolean supports_write_uncor;   /* Write Uncorrectable */
    Boolean supports_compare;       /* Compare command */
    Boolean supports_write_zeros;   /* Write Zeroes */
} NVMeController;

/* Enumerate NVMe controllers */
OSErr EnumerateNVMeControllers(NVMeController** controllers, UInt32* count);

/* Get NVMe device information */
OSErr GetNVMeDeviceInfo(const char* controller_path, NVMeController* controller);

/* ============================================================================
 * SATA Device Support
 * ============================================================================ */

typedef struct SATADevice {
    char device_path[256];          /* /dev/sda */
    char model[64];                 /* ATA model */
    char serial[32];                /* Serial number */
    char firmware[16];              /* Firmware revision */
    UInt64 capacity;                /* Device capacity */
    UInt32 sector_size;             /* Sector size */
    Boolean supports_ncq;           /* Native Command Queuing */
    Boolean supports_smart;         /* SMART support */
    Boolean supports_trim;          /* TRIM support */
    UInt32 rotation_rate;           /* RPM (0 for SSD) */
} SATADevice;

/* Enumerate SATA devices */
OSErr EnumerateSATADevices(SATADevice** devices, UInt32* count);

/* Get SATA device information */
OSErr GetSATADeviceInfo(const char* device_path, SATADevice* device);

/* ============================================================================
 * USB Storage Support
 * ============================================================================ */

typedef struct USBStorageDevice {
    char device_path[256];          /* /dev/sdb */
    char usb_path[256];             /* USB device path */
    UInt16 vendor_id;               /* USB vendor ID */
    UInt16 product_id;              /* USB product ID */
    char vendor_string[64];         /* Vendor string */
    char product_string[64];        /* Product string */
    char serial_string[32];         /* Serial string */
    UInt32 usb_version;             /* USB version (200, 300, etc.) */
    UInt64 capacity;                /* Storage capacity */
    Boolean is_removable;           /* Removable media */
    Boolean is_mounted;             /* Currently mounted */
} USBStorageDevice;

/* Enumerate USB storage devices */
OSErr EnumerateUSBStorageDevices(USBStorageDevice** devices, UInt32* count);

/* Get USB storage device information */
OSErr GetUSBStorageDeviceInfo(const char* device_path, USBStorageDevice* device);

/* ============================================================================
 * Virtual Storage Support
 * ============================================================================ */

typedef struct VirtualStorageDevice {
    char device_path[256];          /* /dev/loop0, /dev/dm-0 */
    char backing_file[512];         /* Backing file path */
    char fs_type[32];               /* Filesystem type */
    UInt64 capacity;                /* Virtual device size */
    Boolean is_encrypted;           /* Encrypted volume */
    Boolean is_compressed;          /* Compressed volume */
    Boolean is_snapshot;            /* Snapshot volume */
} VirtualStorageDevice;

/* Enumerate virtual storage devices */
OSErr EnumerateVirtualStorageDevices(VirtualStorageDevice** devices, UInt32* count);

/* ============================================================================
 * File System Detection
 * ============================================================================ */

typedef enum {
    kFilesystemUnknown = 0,
    kFilesystemHFS = 1,             /* Mac OS HFS */
    kFilesystemHFSPlus = 2,         /* Mac OS Extended (HFS+) */
    kFilesystemAPFS = 3,            /* Apple File System */
    kFilesystemFAT32 = 4,           /* FAT32 */
    kFilesystemExFAT = 5,           /* exFAT */
    kFilesystemNTFS = 6,            /* NTFS */
    kFilesystemExt4 = 7,            /* Linux ext4 */
    kFilesystemBtrfs = 8,           /* Linux Btrfs */
    kFilesystemZFS = 9              /* ZFS */
} FilesystemType;

typedef struct FilesystemInfo {
    FilesystemType type;            /* Filesystem type */
    char type_string[32];           /* Type string */
    char label[64];                 /* Volume label */
    char uuid[64];                  /* UUID/GUID */
    UInt64 total_space;             /* Total space */
    UInt64 free_space;              /* Free space */
    UInt64 used_space;              /* Used space */
    UInt32 block_size;              /* Block size */
    Boolean is_journaled;           /* Journaling enabled */
    Boolean is_case_sensitive;      /* Case sensitive */
    Boolean has_system_folder;      /* Contains Mac OS System */
} FilesystemInfo;

/* Detect filesystem on device */
OSErr DetectFilesystem(const char* device_path, FilesystemInfo* fs_info);

/* Check for Mac OS System folder */
OSErr CheckForSystemFolder(const char* mount_point, Boolean* has_system);

/* ============================================================================
 * Device Health and Monitoring
 * ============================================================================ */

typedef struct DeviceHealth {
    Boolean is_healthy;             /* Overall health status */
    UInt32 temperature;             /* Temperature in Celsius */
    UInt32 power_on_hours;          /* Power-on hours */
    UInt64 data_written;            /* Total data written */
    UInt64 data_read;               /* Total data read */
    UInt32 error_count;             /* Error count */
    UInt32 reallocated_sectors;     /* Reallocated sector count */
    UInt32 pending_sectors;         /* Pending sector count */
    float wear_level;               /* Wear level percentage */
} DeviceHealth;

/* Get device health information */
OSErr GetDeviceHealth(const char* device_path, DeviceHealth* health);

/* Monitor device health changes */
OSErr StartHealthMonitoring(const char* device_path);
OSErr StopHealthMonitoring(const char* device_path);

/* ============================================================================
 * Boot Device Selection
 * ============================================================================ */

typedef struct BootCandidate {
    ModernStorageDevice device;     /* Storage device */
    FilesystemInfo filesystem;      /* Filesystem information */
    char system_folder_path[512];   /* Path to System folder */
    char finder_path[512];          /* Path to Finder */
    char system_file_path[512];     /* Path to System file */
    UInt32 system_version;          /* System version */
    UInt32 boot_score;              /* Boot priority score */
    Boolean is_valid;               /* Valid boot candidate */
} BootCandidate;

/* Find all valid boot candidates */
OSErr FindBootCandidates(BootCandidate** candidates, UInt32* count);

/* Select best boot candidate */
OSErr SelectBootDevice(BootCandidate* candidates, UInt32 count, BootCandidate* selected);

/* Validate boot candidate */
OSErr ValidateBootCandidate(BootCandidate* candidate);

/* ============================================================================
 * Storage Subsystem Initialization
 * ============================================================================ */

/* Initialize modern storage stack */
OSErr InitializeModernStorage(void);

/* Shutdown storage stack */
OSErr ShutdownModernStorage(void);

/* Rescan for device changes */
OSErr RescanStorageDevices(void);

/* Get storage subsystem statistics */
OSErr GetStorageStatistics(UInt32* total_devices, UInt32* bootable_devices,
                          UInt64* total_capacity, UInt64* available_capacity);

/* ============================================================================
 * Platform-Specific Storage Functions
 * ============================================================================ */

#ifdef PLATFORM_X86_64
/* x86_64 specific storage functions */
OSErr X86_64_InitializeStorageSubsystem(void);
OSErr X86_64_EnumerateNVMeDevices(ModernStorageDevice** devices, UInt32* count);
OSErr X86_64_EnumerateSATADevices(ModernStorageDevice** devices, UInt32* count);
OSErr X86_64_DetectACPIStorageDevices(ModernStorageDevice** devices, UInt32* count);
#endif

#ifdef PLATFORM_ARM64
/* ARM64 specific storage functions */
OSErr ARM64_InitializeStorageSubsystem(void);
OSErr ARM64_ParseDeviceTreeStorage(ModernStorageDevice** devices, UInt32* count);
OSErr ARM64_EnumerateMMCDevices(ModernStorageDevice** devices, UInt32* count);
OSErr ARM64_DetectAppleStorageDevices(ModernStorageDevice** devices, UInt32* count);
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define kStorageNotInitialized      -7000   /* Storage not initialized */
#define kStorageDeviceNotFound      -7001   /* Storage device not found */
#define kStorageDeviceNotBootable   -7002   /* Device not bootable */
#define kStorageFilesystemNotSupported -7003 /* Filesystem not supported */
#define kStorageDeviceUnhealthy     -7004   /* Device health issues */
#define kStorageInsufficientSpace   -7005   /* Insufficient space */
#define kStorageDeviceBusy          -7006   /* Device busy */
#define kStoragePermissionDenied    -7007   /* Permission denied */

#ifdef __cplusplus
}
#endif

#endif /* MODERN_STORAGE_H */