# System 7.1 Boot Loader Modernization Plan

## Overview

This document outlines the plan to modernize the System 7.1 Boot Loader for x86_64 and ARM64 targets while preserving the original 6-stage boot sequence and Mac OS compatibility.

## Current Architecture Analysis

### Existing Boot Stages
1. **ROM Stage** - Hardware ROM initialization (needs platform abstraction)
2. **System Load** - Load system files from disk (needs modern storage)
3. **Hardware Init** - Initialize hardware devices (needs HAL)
4. **Manager Init** - Initialize system managers (mostly portable)
5. **Startup Selection** - Select startup disk (needs modern device discovery)
6. **Finder Launch** - Launch Finder application (portable)

### Current Limitations for Modern Targets

1. **Hardware Dependencies**:
   - Mac-specific ROM calls (InitApplZone, MaxApplZone)
   - Classic Mac device types (Sony drives, EDisk)
   - Mac-specific resource system (GetResource)

2. **Platform Assumptions**:
   - Big-endian byte order (68k/PowerPC legacy)
   - Mac OS memory management model
   - Classic Mac file system assumptions

3. **Boot Environment**:
   - No UEFI/EFI support
   - No ACPI/device tree integration
   - No modern storage stack (NVMe, SATA, USB)

## Modernization Strategy

### Phase 1: Hardware Abstraction Layer (HAL)

Create a platform-agnostic HAL that provides:

```c
// Platform abstraction for boot services
typedef struct PlatformBootServices {
    // Memory management
    OSErr (*InitializeMemory)(void);
    OSErr (*AllocateMemory)(Size size, void** ptr);
    OSErr (*FreeMemory)(void* ptr);

    // Device discovery
    OSErr (*EnumerateStorageDevices)(DeviceInfo** devices, UInt32* count);
    OSErr (*InitializeDevice)(DeviceInfo* device);

    // Platform info
    OSErr (*GetPlatformInfo)(PlatformInfo* info);
    OSErr (*ValidatePlatformRequirements)(void);

    // Boot services
    OSErr (*LoadSystemImage)(const char* path, void** image, Size* size);
    OSErr (*LaunchApplication)(const char* path, void** handle);
} PlatformBootServices;
```

### Phase 2: Platform-Specific Implementations

#### x86_64 Implementation
- UEFI boot services integration
- ACPI device enumeration
- x86_64 memory management
- PC storage standards (SATA, NVMe, USB)

#### ARM64 Implementation
- Device tree parsing
- ARM64 memory management
- ARM-specific device discovery
- Modern ARM storage controllers

### Phase 3: Modern Device Support

Replace classic Mac device types with modern equivalents:

```c
typedef enum {
    kDeviceTypeNVMe = 1,        // NVMe SSD
    kDeviceTypeSATA = 2,        // SATA drive
    kDeviceTypeUSB = 3,         // USB storage
    kDeviceTypeSD = 4,          // SD card
    kDeviceTypeNetwork = 5,     // Network boot
    kDeviceTypeVirtual = 6      // Virtual/container storage
} ModernDeviceType;
```

## Implementation Plan

### 1. Create Hardware Abstraction Layer

**File**: `src/BootLoader/hal/boot_hal.h`
**File**: `src/BootLoader/hal/boot_hal.c`

Provides platform-neutral boot services interface.

### 2. Platform-Specific Implementations

**x86_64**:
- `src/BootLoader/platforms/x86_64/x86_64_boot.c`
- `src/BootLoader/platforms/x86_64/uefi_integration.c`
- `src/BootLoader/platforms/x86_64/acpi_discovery.c`

**ARM64**:
- `src/BootLoader/platforms/arm64/arm64_boot.c`
- `src/BootLoader/platforms/arm64/devicetree_parser.c`
- `src/BootLoader/platforms/arm64/arm_discovery.c`

### 3. Modern Storage Stack

**File**: `src/BootLoader/storage/modern_storage.h`
**File**: `src/BootLoader/storage/modern_storage.c`

Provides unified interface for:
- NVMe drives
- SATA/IDE drives
- USB storage
- Network storage
- Virtual storage

### 4. Cross-Platform Build System

**File**: `src/BootLoader/CMakeLists.txt`

CMake-based build system supporting:
- Cross-compilation for x86_64 and ARM64
- Platform-specific code selection
- Feature detection and conditional compilation
- Unit testing across platforms

## Detailed Implementation

### Boot HAL Interface

```c
// Platform detection
typedef enum {
    kPlatformUnknown = 0,
    kPlatformX86_64 = 1,
    kPlatformARM64 = 2,
    kPlatformRISCV = 3
} PlatformType;

typedef struct PlatformInfo {
    PlatformType platform;
    char vendor[32];
    char model[64];
    UInt32 cpu_count;
    UInt64 memory_size;
    Boolean has_uefi;
    Boolean has_acpi;
    Boolean has_devicetree;
} PlatformInfo;

// Boot services interface
OSErr HAL_InitializePlatform(void);
OSErr HAL_GetPlatformInfo(PlatformInfo* info);
OSErr HAL_EnumerateDevices(DeviceInfo** devices, UInt32* count);
OSErr HAL_LoadImage(const char* path, void** image, Size* size);
```

### Modern Device Discovery

```c
typedef struct ModernDeviceInfo {
    ModernDeviceType type;
    char device_path[256];      // /dev/nvme0n1, /dev/sda1, etc.
    char label[64];             // Human-readable name
    UInt64 size_bytes;          // Device size
    Boolean is_bootable;        // Has boot signature
    Boolean has_system_folder;  // Contains system files
    UInt32 boot_priority;       // Boot order preference
    void* platform_data;       // Platform-specific info
} ModernDeviceInfo;

OSErr EnumerateModernDevices(ModernDeviceInfo** devices, UInt32* count);
OSErr ValidateBootDevice(ModernDeviceInfo* device);
OSErr MountBootDevice(ModernDeviceInfo* device);
```

### UEFI Integration (x86_64)

```c
// UEFI boot services wrapper
typedef struct UEFIBootServices {
    EFI_BOOT_SERVICES* boot_services;
    EFI_RUNTIME_SERVICES* runtime_services;
    EFI_SYSTEM_TABLE* system_table;
} UEFIBootServices;

OSErr InitializeUEFI(UEFIBootServices* services);
OSErr UEFI_EnumerateBlockDevices(DeviceInfo** devices, UInt32* count);
OSErr UEFI_LoadImage(const char* path, void** image, Size* size);
OSErr UEFI_ExitBootServices(void);
```

### Device Tree Support (ARM64)

```c
// Device tree parser for ARM64
typedef struct DeviceTreeNode {
    char name[64];
    char compatible[128];
    UInt64 reg_base;
    UInt32 reg_size;
    struct DeviceTreeNode* children;
    struct DeviceTreeNode* next;
} DeviceTreeNode;

OSErr ParseDeviceTree(void* dtb, DeviceTreeNode** root);
OSErr FindStorageControllers(DeviceTreeNode* root, DeviceInfo** devices, UInt32* count);
OSErr InitializeARM64Storage(DeviceTreeNode* controller);
```

## Memory Management Modernization

### Replace Mac OS Memory Calls

```c
// Modern memory management
OSErr ModernMemoryInit(void);
Handle ModernNewHandle(Size size);
void ModernDisposeHandle(Handle h);
OSErr ModernGetHandleSize(Handle h, Size* size);

// Platform-specific memory
#ifdef __x86_64__
    // Use standard malloc/free with alignment
#elif defined(__aarch64__)
    // Use ARM64-optimized allocation
#endif
```

### Resource System Modernization

```c
// Modern resource loading
typedef struct ModernResourceManager {
    char resource_path[256];    // Path to resource directory
    void* resource_cache;       // Cached resources
    UInt32 cache_size;         // Cache size limit
} ModernResourceManager;

OSErr LoadModernResource(ResType type, ResID id, void** data, Size* size);
OSErr InitializeResourceManager(const char* resource_path);
```

## Build System Integration

### CMake Configuration

```cmake
# Platform detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(BOOT_PLATFORM "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(BOOT_PLATFORM "arm64")
endif()

# Platform-specific sources
if(BOOT_PLATFORM STREQUAL "x86_64")
    set(PLATFORM_SOURCES
        platforms/x86_64/x86_64_boot.c
        platforms/x86_64/uefi_integration.c
        platforms/x86_64/acpi_discovery.c
    )
elseif(BOOT_PLATFORM STREQUAL "arm64")
    set(PLATFORM_SOURCES
        platforms/arm64/arm64_boot.c
        platforms/arm64/devicetree_parser.c
        platforms/arm64/arm_discovery.c
    )
endif()
```

## Testing Strategy

### Cross-Platform Testing

1. **Unit Tests**: Test HAL interface on both platforms
2. **Integration Tests**: Test device discovery and enumeration
3. **Boot Tests**: Test actual boot sequence on real hardware
4. **VM Testing**: Test in QEMU for both x86_64 and ARM64

### Validation Framework

```c
typedef struct BootTestSuite {
    OSErr (*test_platform_detection)(void);
    OSErr (*test_device_enumeration)(void);
    OSErr (*test_memory_management)(void);
    OSErr (*test_resource_loading)(void);
    OSErr (*test_boot_sequence)(void);
} BootTestSuite;

OSErr RunCrossPlatformTests(PlatformType platform);
```

## Migration Timeline

### Week 1-2: HAL Foundation
- Create boot HAL interface
- Implement basic platform detection
- Set up cross-platform build system

### Week 3-4: x86_64 Implementation
- UEFI integration
- ACPI device discovery
- Modern storage support

### Week 5-6: ARM64 Implementation
- Device tree parsing
- ARM64 memory management
- ARM storage controllers

### Week 7: Integration & Testing
- Cross-platform validation
- Performance optimization
- Documentation updates

## Compatibility Considerations

### Preserve Original API
- Keep original boot stage sequence
- Maintain Mac OS error codes
- Preserve function signatures where possible

### Graceful Fallbacks
- Detect platform capabilities
- Fall back to generic implementations
- Maintain compatibility with legacy systems

## Conclusion

This modernization plan transforms the System 7.1 Boot Loader into a cross-platform boot system while preserving its original architecture and Mac OS compatibility. The HAL-based approach ensures portability while platform-specific implementations provide optimal performance on modern hardware.

The result will be a boot loader that can run natively on x86_64 and ARM64 systems while maintaining the classic Mac OS boot experience and supporting modern storage devices and boot standards.