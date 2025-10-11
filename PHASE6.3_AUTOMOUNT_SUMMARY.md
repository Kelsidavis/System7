# SYSTEM 7X — Phase 6.3: Automatic Filesystem Detection and Mount Manager

**Date:** 2025-10-10
**Status:** ✅ COMPLETE
**Git Branch:** future

---

## Executive Summary

Phase 6.3 extends the modular Virtual File System (VFS) with **automatic device scanning, filesystem probing, and volume mounting at boot time**. This phase eliminates the need for manual mounting code and enables the system to automatically detect and mount filesystems on all discovered storage devices.

### Key Achievements

- ✅ **Block Device Registry** - Central registry for all discovered storage devices
- ✅ **MBR Partition Table Parsing** - Full support for Master Boot Record partition tables
- ✅ **Automatic Filesystem Detection** - Probes all partitions with registered filesystem drivers
- ✅ **Raw Device Mounting** - Falls back to raw device mounting when no partition table exists
- ✅ **Boot Integration** - Seamlessly integrated into main.c boot sequence
- ✅ **Comprehensive Logging** - Detailed [BLOCK] and [AUTOMOUNT] debug output

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      Boot Sequence (main.c)                      │
├─────────────────────────────────────────────────────────────────┤
│  1. MVFS_Initialize()           → Initialize VFS core           │
│  2. FS_RegisterFilesystems()    → Register HFS, FAT32 drivers   │
│  3. block_registry_init()       → Initialize block registry     │
│  4. vfs_autodetect_init()       → Initialize auto-mount system  │
│  5. block_register(ata0, ...)   → Register discovered devices   │
│  6. vfs_autodetect_mount()      → Auto-detect and mount all     │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Block Device Registry                         │
│                  (block_registry.c / block.h)                    │
├─────────────────────────────────────────────────────────────────┤
│  • BlockRegistry: Global registry with 16 device slots          │
│  • BlockDeviceEntry: Metadata (type, name, size, removable)     │
│  • BlockDeviceType: ATA, MEMORY, SCSI, USB, ISO, VIRTUAL        │
│                                                                  │
│  Functions:                                                      │
│    block_register()      → Add device to registry               │
│    block_enumerate()     → Get all registered devices           │
│    block_get_by_name()   → Find device by name                  │
│    block_get_by_index()  → Find device by index                 │
│    block_get_count()     → Get total device count               │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                   VFS Auto-Detection System                      │
│              (vfs_autodetect.c / vfs_autodetect.h)               │
├─────────────────────────────────────────────────────────────────┤
│  Phase 1: Device Enumeration                                    │
│    • Query block registry for all devices                       │
│    • Iterate through each discovered device                     │
│                                                                  │
│  Phase 2: MBR Parsing                                           │
│    • Read sector 0 from device                                  │
│    • Verify 0xAA55 signature                                    │
│    • Parse 4 partition table entries                            │
│    • Extract: type, start_lba, num_sectors                      │
│                                                                  │
│  Phase 3: Partition Validation                                  │
│    • Skip empty partitions (type = 0x00)                        │
│    • Skip zero-size partitions                                  │
│    • Identify partition type (FAT32, Linux, HFS, etc.)          │
│                                                                  │
│  Phase 4: Filesystem Probing                                    │
│    • For each valid partition:                                  │
│      - Call MVFS_Mount() with partition device                  │
│      - VFS probes all registered filesystem drivers             │
│      - First matching driver mounts the partition               │
│                                                                  │
│  Phase 5: Raw Device Fallback                                   │
│    • If no MBR found, try mounting raw device                   │
│    • Useful for unpartitioned disks (e.g., floppy, CD-ROM)      │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                         Result: Mounted Volumes                  │
│                    (visible via MVFS_ListVolumes)                │
└─────────────────────────────────────────────────────────────────┘
```

---

## File Structure

### New Headers

#### **include/Nanokernel/block.h** (75 lines)
Block device registry interface.

**Key Types:**
```c
typedef enum {
    BLOCK_TYPE_ATA,      // IDE/SATA hard drives
    BLOCK_TYPE_MEMORY,   // RAM disk
    BLOCK_TYPE_SCSI,     // SCSI devices
    BLOCK_TYPE_USB,      // USB mass storage
    BLOCK_TYPE_ISO,      // ISO9660 CD/DVD
    BLOCK_TYPE_VIRTUAL   // Virtual/loopback devices
} BlockDeviceType;

typedef struct {
    BlockDevice*    device;      // Pointer to device structure
    BlockDeviceType type;        // Device type
    char            name[32];    // Device name (e.g., "ata0", "usb1")
    uint64_t        total_size;  // Total size in bytes
    bool            removable;   // Is device removable?
} BlockDeviceEntry;

typedef struct {
    BlockDeviceEntry devices[BLOCK_MAX_DEVICES];
    size_t           count;
    bool             initialized;
} BlockRegistry;
```

**Key Functions:**
- `block_registry_init()` - Initialize the registry
- `block_register()` - Add a device to the registry
- `block_enumerate()` - Get array of all registered devices
- `block_get_by_name()` - Find device by name
- `block_get_count()` - Get total number of registered devices

#### **include/Nanokernel/vfs_autodetect.h** (63 lines)
Auto-detection and MBR structures.

**MBR Structure:**
```c
typedef struct {
    uint8_t  boot_flag;       // 0x80 = bootable, 0x00 = not bootable
    uint8_t  start_chs[3];    // CHS start address (legacy)
    uint8_t  type;            // Partition type code
    uint8_t  end_chs[3];      // CHS end address (legacy)
    uint32_t start_lba;       // LBA start sector
    uint32_t num_sectors;     // Number of sectors
} __attribute__((packed)) MBRPartition;

typedef struct {
    uint8_t        boot_code[446];  // Boot loader code
    MBRPartition   partitions[4];   // 4 primary partition entries
    uint16_t       signature;       // Must be 0xAA55
} __attribute__((packed)) MBR;
```

**Partition Type Codes:**
```c
#define PART_TYPE_EMPTY     0x00  // Empty partition
#define PART_TYPE_FAT16     0x06  // FAT16
#define PART_TYPE_NTFS      0x07  // NTFS
#define PART_TYPE_FAT32     0x0B  // FAT32 (CHS)
#define PART_TYPE_FAT32_LBA 0x0C  // FAT32 (LBA)
#define PART_TYPE_FAT16_LBA 0x0E  // FAT16 (LBA)
#define PART_TYPE_EXT       0x05  // Extended partition
#define PART_TYPE_LINUX     0x83  // Linux native
#define PART_TYPE_HFS       0xAF  // Apple HFS/HFS+
```

**Key Functions:**
- `vfs_autodetect_init()` - Initialize auto-detection subsystem
- `vfs_autodetect_mount()` - Main auto-mount entry point
- `vfs_probe_device()` - Probe a specific device for filesystems
- `vfs_read_mbr()` - Read and validate MBR from device
- `vfs_is_partition_valid()` - Check if partition entry is valid

### New Implementation Files

#### **src/Nanokernel/block_registry.c** (194 lines)
Block device registry implementation.

**Global State:**
```c
static BlockRegistry g_block_registry = { 0 };
```

**Device Registration Flow:**
1. Check if registry is initialized
2. Verify device count < BLOCK_MAX_DEVICES (16)
3. Check for duplicate device names
4. Store device metadata (name, type, size)
5. Increment device count
6. Log registration with [BLOCK] prefix

**Example Output:**
```
[BLOCK] Block device registry initialized
[BLOCK] Registered device: ata0 (type=ATA, size=536870912 bytes)
```

#### **src/Nanokernel/vfs_autodetect.c** (198 lines)
Automatic filesystem detection and mounting.

**Key Implementation Details:**

1. **MBR Reading** (`vfs_read_mbr`)
   - Read sector 0 (512 bytes)
   - Verify signature = 0xAA55
   - Return false if invalid (not an error - device may have raw filesystem)

2. **Partition Validation** (`vfs_is_partition_valid`)
   - Check type != 0x00 (empty)
   - Check num_sectors > 0
   - Return false for invalid partitions (skips them silently)

3. **Device Probing** (`vfs_probe_device`)
   - Try to read MBR
   - If MBR found:
     - Parse all 4 partition entries
     - Skip invalid partitions
     - Create partition names: "ata0p1", "ata0p2", etc.
     - Call MVFS_Mount() for each partition
     - Return true on first successful mount
   - If no MBR:
     - Try mounting as raw device
     - Return true if successful

4. **Auto-Mount Main Loop** (`vfs_autodetect_mount`)
   - Enumerate all registered block devices
   - Call vfs_probe_device() for each
   - Count successful mounts
   - Call MVFS_ListVolumes() to display results

**Example Output:**
```
[AUTOMOUNT] Starting automatic filesystem detection...
[AUTOMOUNT] Found 1 registered block device(s)
[AUTOMOUNT] === Probing ata0 ===
[AUTOMOUNT] Probing device: ata0
[AUTOMOUNT] Valid MBR found on ata0
[AUTOMOUNT] Partition 1: type=0x0B (FAT32), start=2048, size=1048576 sectors
[AUTOMOUNT] Successfully mounted partition ata0p1
[AUTOMOUNT] Auto-mount complete: 1 volume(s) mounted
```

### Modified Files

#### **src/main.c** (lines 1953-1984)
Updated boot sequence to use auto-mount system.

**Before (Manual Mounting):**
```c
MVFS_Initialize();
FS_RegisterFilesystems();

if (hal_storage_get_drive_count() > 0) {
    BlockDevice* ata0 = MVFS_CreateATABlockDevice(0);
    if (ata0) {
        VFSVolume* vol = MVFS_Mount(ata0, "Macintosh HD");
        if (vol) {
            serial_puts("  Modular VFS: Successfully mounted ATA device 0\n");
        }
    }
}
```

**After (Automatic Mounting):**
```c
MVFS_Initialize();
FS_RegisterFilesystems();
block_registry_init();
vfs_autodetect_init();

serial_puts("  Block registry and auto-mount initialized\n");

if (hal_storage_get_drive_count() > 0) {
    BlockDevice* ata0 = MVFS_CreateATABlockDevice(0);
    if (ata0) {
        block_register(ata0, 0, "ata0");  /* 0 = BLOCK_TYPE_ATA */
        serial_puts("  Registered ATA device 0\n");
    }
}

serial_puts("  Starting automatic filesystem detection...\n");
vfs_autodetect_mount();
```

**Key Changes:**
- Added block_registry_init() and vfs_autodetect_init() calls
- Replaced manual MVFS_Mount() with block_register() + vfs_autodetect_mount()
- Device registration separates discovery from mounting
- Automatic probing handles partitions and raw devices

#### **Makefile** (lines 182-187)
Added new source files.

```makefile
NANOKERNEL_SRCS = \
    src/Nanokernel/mvfs.c \
    src/Nanokernel/block_registry.c \
    src/Nanokernel/vfs_autodetect.c \
    ...
```

---

## API Reference

### Block Device Registry API

#### `void block_registry_init(void)`
Initialize the block device registry. Must be called before any block device operations.

**Usage:**
```c
block_registry_init();
```

#### `bool block_register(BlockDevice* dev, BlockDeviceType type, const char* name)`
Register a block device with the registry.

**Parameters:**
- `dev` - Pointer to BlockDevice structure
- `type` - Device type (BLOCK_TYPE_ATA, BLOCK_TYPE_MEMORY, etc.)
- `name` - Unique device name (e.g., "ata0", "usb1")

**Returns:** true on success, false if registry is full or name is duplicate

**Usage:**
```c
BlockDevice* ata0 = MVFS_CreateATABlockDevice(0);
block_register(ata0, BLOCK_TYPE_ATA, "ata0");
```

#### `size_t block_enumerate(const BlockDeviceEntry** out, size_t max)`
Get array of all registered devices.

**Parameters:**
- `out` - Output array of BlockDeviceEntry pointers
- `max` - Maximum number of entries to return

**Returns:** Number of devices returned

**Usage:**
```c
const BlockDeviceEntry* devices[BLOCK_MAX_DEVICES];
size_t count = block_enumerate(devices, BLOCK_MAX_DEVICES);
for (size_t i = 0; i < count; i++) {
    serial_printf("Device: %s\n", devices[i]->name);
}
```

#### `const BlockDeviceEntry* block_get_by_name(const char* name)`
Find a device by name.

**Returns:** Pointer to BlockDeviceEntry, or NULL if not found

**Usage:**
```c
const BlockDeviceEntry* entry = block_get_by_name("ata0");
if (entry) {
    serial_printf("Size: %llu bytes\n", entry->total_size);
}
```

#### `size_t block_get_count(void)`
Get total number of registered devices.

**Returns:** Device count

### Auto-Detection API

#### `void vfs_autodetect_init(void)`
Initialize the auto-detection subsystem. Must be called after MVFS_Initialize().

**Usage:**
```c
vfs_autodetect_init();
```

#### `void vfs_autodetect_mount(void)`
Main auto-mount entry point. Enumerates all registered block devices, probes for filesystems, and mounts detected volumes.

**Usage:**
```c
vfs_autodetect_mount();  // Automatically mounts all detected filesystems
```

#### `bool vfs_probe_device(const char* device_name)`
Probe a specific device for filesystems. Reads MBR, parses partitions, and attempts to mount.

**Parameters:**
- `device_name` - Name of device to probe (e.g., "ata0")

**Returns:** true if at least one partition/volume was mounted

**Usage:**
```c
if (vfs_probe_device("ata0")) {
    serial_printf("Successfully mounted filesystem from ata0\n");
}
```

#### `bool vfs_read_mbr(void* block_device, MBR* mbr)`
Read and validate MBR from a block device.

**Parameters:**
- `block_device` - Pointer to BlockDevice
- `mbr` - Output MBR structure

**Returns:** true if valid MBR found (signature = 0xAA55), false otherwise

**Usage:**
```c
MBR mbr;
if (vfs_read_mbr(dev, &mbr)) {
    serial_printf("Valid MBR signature: 0x%04x\n", mbr.signature);
}
```

#### `bool vfs_is_partition_valid(const MBRPartition* part)`
Check if a partition table entry is valid.

**Parameters:**
- `part` - Pointer to MBRPartition entry

**Returns:** true if partition is non-empty and has non-zero size

---

## Boot Sequence Flow

```
main() starts
    ↓
[VFS INITIALIZATION]
    MVFS_Initialize()
    FS_RegisterFilesystems()  → Register HFS, FAT32 drivers
    block_registry_init()     → Initialize block device registry
    vfs_autodetect_init()     → Initialize auto-mount subsystem
    ↓
[DEVICE DISCOVERY]
    hal_storage_get_drive_count() → Query HAL for ATA devices
    For each ATA device:
        MVFS_CreateATABlockDevice(index) → Create BlockDevice wrapper
        block_register(dev, BLOCK_TYPE_ATA, "ata0") → Add to registry
    ↓
[AUTOMATIC MOUNTING]
    vfs_autodetect_mount()
        ↓
        block_enumerate() → Get all registered devices
        ↓
        For each device:
            vfs_probe_device(device_name)
                ↓
                vfs_read_mbr(device) → Read sector 0
                ↓
                If MBR valid:
                    For each partition (0-3):
                        vfs_is_partition_valid() → Check type and size
                        ↓
                        If valid:
                            MVFS_Mount(device, "ata0p1") → Try to mount
                            ↓
                            VFS_ProbeFilesystems() → Try all registered drivers
                            ↓
                            First matching driver mounts partition
                Else (no MBR):
                    MVFS_Mount(device, "ata0") → Try raw device mount
        ↓
        MVFS_ListVolumes() → Display all mounted volumes
    ↓
[SYSTEM READY]
```

---

## Serial Log Examples

### Successful MBR-Based Mount

```
[BLOCK] Block device registry initialized
[BLOCK] Registered device: ata0 (type=ATA, size=536870912 bytes)
[AUTOMOUNT] Autodetect subsystem initialized
[AUTOMOUNT] Starting automatic filesystem detection...
[AUTOMOUNT] Found 1 registered block device(s)
[AUTOMOUNT] === Probing ata0 ===
[AUTOMOUNT] Probing device: ata0
[AUTOMOUNT] Valid MBR found on ata0
[AUTOMOUNT] Partition 1: type=0x0B (FAT32), start=2048, size=1048576 sectors
[PLATFORM] ATA: Read complete
[FAT32] FAT32 filesystem detected
[VFS] Calling mount handler for filesystem: FAT32
[VFS] Volume 'ata0p1' mounted successfully (ID: 1)
[AUTOMOUNT] Successfully mounted partition ata0p1
[AUTOMOUNT] Auto-mount complete: 1 volume(s) mounted
[VFS] === Mounted Volumes ===
[VFS] Volume 1: 'ata0p1' (FAT32, mounted, read-write)
[VFS] Total volumes: 1
```

### Raw Device Mount (No MBR)

```
[AUTOMOUNT] Probing device: cdrom0
[AUTOMOUNT] No MBR found, trying raw device mount
[PLATFORM] CDROM: Read complete
[VFS] Probing device with filesystem: ISO9660
[ISO9660] ISO9660 filesystem detected
[VFS] Calling mount handler for filesystem: ISO9660
[VFS] Volume 'cdrom0' mounted successfully (ID: 2)
[AUTOMOUNT] Successfully mounted raw device cdrom0
```

### No Filesystem Detected

```
[AUTOMOUNT] Probing device: ata1
[AUTOMOUNT] Valid MBR found on ata1
[AUTOMOUNT] Partition 1: type=0x07 (NTFS), start=2048, size=2097152 sectors
[VFS] ERROR: No filesystem detected on device
[AUTOMOUNT] Failed to mount partition ata1p1
[AUTOMOUNT] Partition 2: type=0x00 (Empty), start=0, size=0 sectors
[AUTOMOUNT] Auto-mount complete: 0 volume(s) mounted
```

---

## MBR Partition Table Reference

### Standard Partition Type Codes

| Code | Type | Description |
|------|------|-------------|
| 0x00 | Empty | Unused partition entry |
| 0x01 | FAT12 | FAT12 filesystem |
| 0x04 | FAT16 <32MB | FAT16 with CHS addressing |
| 0x05 | Extended | Extended partition container |
| 0x06 | FAT16 | FAT16 with CHS addressing (>32MB) |
| 0x07 | NTFS/exFAT | Windows NT filesystem |
| 0x0B | FAT32 | FAT32 with CHS addressing |
| 0x0C | FAT32 LBA | FAT32 with LBA addressing |
| 0x0E | FAT16 LBA | FAT16 with LBA addressing |
| 0x0F | Extended LBA | Extended partition with LBA |
| 0x82 | Linux swap | Linux swap partition |
| 0x83 | Linux | Linux native filesystem (ext2/3/4) |
| 0xAF | HFS/HFS+ | Apple Hierarchical File System |
| 0xEE | GPT Protective | GPT partition table present |

### MBR Structure Layout

```
Offset  Size  Description
------  ----  -----------
0x000   446   Boot loader code (can include partition code)
0x1BE    16   Partition entry 1
0x1CE    16   Partition entry 2
0x1DE    16   Partition entry 3
0x1EE    16   Partition entry 4
0x1FE     2   Boot signature (0x55 0xAA)

Partition Entry Layout (16 bytes):
Offset  Size  Description
------  ----  -----------
0x00      1   Boot flag (0x80 = bootable, 0x00 = not bootable)
0x01      3   CHS start address (legacy)
0x04      1   Partition type code
0x05      3   CHS end address (legacy)
0x08      4   LBA start sector (little-endian)
0x0C      4   Number of sectors (little-endian)
```

---

## Testing and Verification

### Test Case 1: ATA Disk with MBR
**Setup:** Create disk image with MBR and FAT32 partition
```bash
dd if=/dev/zero of=test.img bs=1M count=512
fdisk test.img  # Create partition table
mkfs.vfat -F 32 test.img
```

**Expected Behavior:**
1. Block registry detects ATA device
2. Auto-mount reads MBR successfully
3. Partition 1 is identified as FAT32 (type 0x0B)
4. FAT32 driver probes and mounts partition
5. Volume appears in MVFS_ListVolumes()

### Test Case 2: Raw Device (No MBR)
**Setup:** Create ISO9660 CD-ROM image
```bash
mkisofs -o test.iso files/
```

**Expected Behavior:**
1. Auto-mount fails to read valid MBR
2. Falls back to raw device mounting
3. ISO9660 driver probes and mounts
4. Volume appears with device name

### Test Case 3: Multiple Partitions
**Setup:** Disk with 3 partitions (FAT32, Linux, HFS)

**Expected Behavior:**
1. MBR parsed successfully
2. All 3 partitions identified
3. Each partition probed with all drivers
4. Partitions named: ata0p1, ata0p2, ata0p3
5. Only partitions with matching drivers mounted

---

## Design Decisions

### 1. Little-Endian Conversion Helpers
**Rationale:** MBR uses little-endian byte order for multi-byte fields. x86 is natively little-endian, but we add conversion helpers for portability to big-endian architectures.

```c
static uint16_t le16_to_cpu(uint16_t val) {
    uint8_t* bytes = (uint8_t*)&val;
    return bytes[0] | ((uint16_t)bytes[1] << 8);
}
```

### 2. Separate Block Registry
**Rationale:** Decouples device discovery from filesystem mounting. Allows:
- Hotplug support in future (register devices dynamically)
- User-space device query tools
- Multi-stage boot process (discover first, mount later)

### 3. Partition Naming Convention
**Rationale:** Use Linux-style naming (ata0p1, ata0p2) for clarity:
- Base device name + "p" + partition number
- Distinguishes partitions from raw devices
- Consistent with industry standards

### 4. Fail-Silent for Invalid Partitions
**Rationale:** Skip invalid partitions silently rather than logging errors:
- Empty partition entries (type = 0x00) are normal
- Reduces log noise
- Only log actual mount failures

### 5. First-Match Mounting Strategy
**Rationale:** Stop probing device after first successful mount:
- Prevents mounting same device multiple times
- Reduces boot time
- User can manually mount additional partitions later

---

## Future Enhancements

### Phase 6.4: Extended Partition Support
- Parse extended partition tables (type 0x05, 0x0F)
- Support >4 logical partitions
- Recursive partition parsing

### Phase 6.5: GPT Partition Table
- Detect GPT protective MBR (type 0xEE)
- Parse GUID Partition Table
- Support 128 partition entries
- UUID-based volume identification

### Phase 6.6: Hotplug Device Management
- Dynamic device registration at runtime
- USB device enumeration
- Auto-mount on device insertion
- Safe unmount before removal

### Phase 6.7: Partition Caching
- Cache partition table in memory
- Avoid re-reading MBR on every boot
- Validate cache with disk signature
- Invalidate cache on device change

### Phase 6.8: User-Space Mount Manager
- System call interface for mounting/unmounting
- Device enumeration API
- Partition query API
- Mount options (read-only, no-exec, etc.)

### Phase 6.9: Filesystem-Specific Probing
- Volume label detection
- UUID/GUID extraction
- Filesystem health checks
- Mount by UUID instead of device name

### Phase 6.10: Boot Device Priority
- Prioritize boot device (where kernel loaded from)
- Mount root filesystem first
- Fallback device ordering
- Boot configuration file

---

## Build and Deployment

### Build Commands
```bash
make clean
make -j4          # Build kernel
make iso          # Create bootable ISO
make run          # Test in QEMU
```

### Expected Build Output
```
Building kernel...
Compiling src/Nanokernel/block_registry.c
Compiling src/Nanokernel/vfs_autodetect.c
Linking build/kernel.elf
Creating build/cdrom/boot/kernel
Building bootable ISO...
Done. Boot ISO: build/os.iso
```

### Deployment Steps
1. Build kernel with auto-mount support
2. Create bootable ISO
3. Boot in QEMU or on real hardware
4. Observe serial log for [BLOCK] and [AUTOMOUNT] messages
5. Verify volumes are mounted automatically
6. Check MVFS_ListVolumes() output

---

## Conclusion

Phase 6.3 successfully implements automatic filesystem detection and mounting for System 7X. The system now:

- **Automatically discovers** all registered block devices at boot
- **Parses MBR partition tables** to identify partitions
- **Probes filesystems** using registered drivers (HFS, FAT32, ext2, ISO9660)
- **Mounts detected volumes** without manual intervention
- **Falls back gracefully** to raw device mounting when no MBR exists

This infrastructure provides a solid foundation for future enhancements including hotplug support, GPT partitions, and user-space mount management.

---

**Next Phase:** Phase 6.4 will implement extended partition support for >4 logical partitions.

**Git Commit:** Ready to commit Phase 6.3 changes to branch `future`.
