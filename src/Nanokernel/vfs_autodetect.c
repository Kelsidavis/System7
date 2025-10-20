/* System 7X Nanokernel - VFS Autodetect Implementation
 *
 * Automatic filesystem detection and mounting at boot time.
 * Supports:
 * - MBR partition table parsing
 * - Multiple filesystem probing (HFS, FAT32, ext2, ISO9660)
 * - Raw device mounting (no partition table)
 */

#include "../../include/Nanokernel/vfs_autodetect.h"
#include "../../include/Nanokernel/vfs.h"
#include "../../include/Nanokernel/block.h"
#include "../../include/Nanokernel/filesystem.h"
#include "../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
extern FileSystemOps* HFS_GetOps(void);
extern FileSystemOps* FAT32_GetOps(void);

/* Helper: Convert little-endian 16-bit value */
static uint16_t le16_to_cpu(uint16_t val) {
    uint8_t* bytes = (uint8_t*)&val;
    return bytes[0] | ((uint16_t)bytes[1] << 8);
}

/* Helper: Convert little-endian 32-bit value */
static uint32_t le32_to_cpu(uint32_t val) {
    uint8_t* bytes = (uint8_t*)&val;
    return bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* Initialize autodetect subsystem */
void vfs_autodetect_init(void) {
    serial_printf("[AUTOMOUNT] Autodetect subsystem initialized\n");
}

/* Read and parse MBR from a block device */
bool vfs_read_mbr(void* block_device, MBR* mbr) {
    if (!block_device || !mbr) {
        return false;
    }

    BlockDevice* dev = (BlockDevice*)block_device;

    /* Read sector 0 (MBR) */
    uint8_t sector[512];
    if (!dev->read_block(dev, 0, sector)) {
        serial_printf("[AUTOMOUNT] Failed to read MBR from device\n");
        return false;
    }

    /* Copy MBR data */
    memcpy(mbr, sector, sizeof(MBR));

    /* Verify MBR signature */
    uint16_t sig = le16_to_cpu(mbr->signature);
    if (sig != MBR_SIGNATURE) {
        /* Not a valid MBR - might be raw filesystem or empty */
        return false;
    }

    return true;
}

/* Check if MBR partition entry is valid */
bool vfs_is_partition_valid(const MBRPartition* part) {
    if (!part) {
        return false;
    }

    /* Check if partition type is non-empty */
    if (part->type == PART_TYPE_EMPTY) {
        return false;
    }

    /* Check if partition has non-zero size */
    uint32_t num_sectors = le32_to_cpu(part->num_sectors);
    if (num_sectors == 0) {
        return false;
    }

    return true;
}

/* Get partition type name */
static const char* get_partition_type_name(uint8_t type) {
    switch (type) {
        case PART_TYPE_EMPTY:     return "Empty";
        case PART_TYPE_FAT16:     return "FAT16";
        case PART_TYPE_NTFS:      return "NTFS";
        case PART_TYPE_FAT32:     return "FAT32";
        case PART_TYPE_FAT32_LBA: return "FAT32-LBA";
        case PART_TYPE_FAT16_LBA: return "FAT16-LBA";
        case PART_TYPE_EXT:       return "Extended";
        case PART_TYPE_LINUX:     return "Linux";
        case PART_TYPE_HFS:       return "HFS";
        default:                  return "Unknown";
    }
}

/* Probe a specific block device for filesystems */
bool vfs_probe_device(const char* device_name) {
    const BlockDeviceEntry* entry = block_get_by_name(device_name);
    if (!entry) {
        serial_printf("[AUTOMOUNT] Device '%s' not found in registry\n", device_name);
        return false;
    }

    serial_printf("[AUTOMOUNT] Probing device: %s\n", device_name);

    /* Try to read MBR */
    MBR mbr;
    bool has_mbr = vfs_read_mbr(entry->device, &mbr);

    if (has_mbr) {
        serial_printf("[AUTOMOUNT] Valid MBR found on %s\n", device_name);

        /* Parse partition table */
        for (int i = 0; i < 4; i++) {
            MBRPartition* part = &mbr.partitions[i];

            if (!vfs_is_partition_valid(part)) {
                continue;
            }

            uint32_t start_lba = le32_to_cpu(part->start_lba);
            uint32_t num_sectors = le32_to_cpu(part->num_sectors);
            const char* type_name = get_partition_type_name(part->type);

            serial_printf("[AUTOMOUNT] Partition %d: type=0x%02x (%s), start=%u, size=%u sectors\n",
                          i + 1, part->type, type_name, start_lba, num_sectors);

            /* Create partition name */
            char part_name[64];
            snprintf(part_name, sizeof(part_name), "%sp%d", device_name, i + 1);

            /* Try to mount this partition */
            VFSVolume* vol = MVFS_Mount(entry->device, part_name);
            if (vol) {
                serial_printf("[AUTOMOUNT] Successfully mounted partition %s\n", part_name);
                return true;
            } else {
                serial_printf("[AUTOMOUNT] Failed to mount partition %s\n", part_name);
            }
        }
    } else {
        /* No MBR - try to mount as raw device */
        serial_printf("[AUTOMOUNT] No MBR found, trying raw device mount\n");

        VFSVolume* vol = MVFS_Mount(entry->device, device_name);
        if (vol) {
            serial_printf("[AUTOMOUNT] Successfully mounted raw device %s\n", device_name);
            return true;
        } else {
            serial_printf("[AUTOMOUNT] Failed to mount raw device %s\n", device_name);
        }
    }

    return false;
}

/* Main auto-mount function */
void vfs_autodetect_mount(void) {
    serial_printf("[AUTOMOUNT] Starting automatic filesystem detection...\n");

    size_t device_count = block_get_count();
    if (device_count == 0) {
        serial_printf("[AUTOMOUNT] No block devices registered\n");
        return;
    }

    serial_printf("[AUTOMOUNT] Found %zu registered block device(s)\n", device_count);

    /* Enumerate all registered devices */
    const BlockDeviceEntry* devices[BLOCK_MAX_DEVICES];
    size_t count = block_enumerate(devices, BLOCK_MAX_DEVICES);

    /* Try to mount each device */
    int mounted_count = 0;
    for (size_t i = 0; i < count; i++) {
        const BlockDeviceEntry* entry = devices[i];

        serial_printf("[AUTOMOUNT] === Probing %s ===\n", entry->name);

        if (vfs_probe_device(entry->name)) {
            mounted_count++;
        }
    }

    serial_printf("[AUTOMOUNT] Auto-mount complete: %d volume(s) mounted\n", mounted_count);

    /* List all mounted volumes */
    MVFS_ListVolumes();
}
