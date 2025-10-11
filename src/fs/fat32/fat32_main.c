/* FAT32 Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for FAT32.
 * It provides detection, mounting, and basic I/O operations for FAT32 volumes.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* FAT32 Boot Sector offsets */
#define FAT32_SIGNATURE_OFFSET    510
#define FAT32_SIGNATURE           0xAA55
#define FAT32_FSTYPE_OFFSET       82
#define FAT32_BYTES_PER_SECTOR    11
#define FAT32_SECTORS_PER_CLUSTER 13
#define FAT32_TOTAL_SECTORS       32

/* Read 16-bit little-endian value */
static uint16_t le16_read(const uint8_t* buf) {
    return buf[0] | ((uint16_t)buf[1] << 8);
}

/* Read 32-bit little-endian value */
static uint32_t le32_read(const uint8_t* buf) {
    return buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* FAT32 private volume data */
typedef struct {
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t total_sectors;
    uint64_t total_bytes;
    uint64_t free_bytes;
    char     volume_label[12];
} FAT32Private;

/* Probe: Detect FAT32 filesystem */
static bool fat32_probe(BlockDevice* dev) {
    uint8_t sector[512];

    /* Read boot sector */
    if (!dev->read_block(dev, 0, sector)) {
        return false;
    }

    /* Check boot signature */
    uint16_t sig = le16_read(&sector[FAT32_SIGNATURE_OFFSET]);
    if (sig != FAT32_SIGNATURE) {
        return false;
    }

    /* Check for "FAT32   " filesystem type string */
    if (memcmp(&sector[FAT32_FSTYPE_OFFSET], "FAT32   ", 8) == 0) {
        serial_printf("[FAT32] FAT32 filesystem detected\n");
        return true;
    }

    return false;
}

/* Mount: Initialize FAT32 volume */
static void* fat32_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t sector[512];

    /* Read boot sector */
    if (!dev->read_block(dev, 0, sector)) {
        serial_printf("[FAT32] Failed to read boot sector\n");
        return NULL;
    }

    /* Allocate private data */
    FAT32Private* priv = (FAT32Private*)malloc(sizeof(FAT32Private));
    if (!priv) {
        serial_printf("[FAT32] Failed to allocate private data\n");
        return NULL;
    }

    /* Parse BPB fields */
    priv->bytes_per_sector = le16_read(&sector[FAT32_BYTES_PER_SECTOR]);
    priv->sectors_per_cluster = sector[FAT32_SECTORS_PER_CLUSTER];
    priv->total_sectors = le32_read(&sector[FAT32_TOTAL_SECTORS]);

    priv->total_bytes = (uint64_t)priv->total_sectors * priv->bytes_per_sector;
    priv->free_bytes = priv->total_bytes;  /* TODO: Calculate from FAT */

    /* Extract volume label (at offset 71 in FAT32) */
    memcpy(priv->volume_label, &sector[71], 11);
    priv->volume_label[11] = '\0';

    /* Trim trailing spaces */
    for (int i = 10; i >= 0; i--) {
        if (priv->volume_label[i] == ' ') {
            priv->volume_label[i] = '\0';
        } else {
            break;
        }
    }

    serial_printf("[FAT32] Mounted '%s' - %llu bytes\n",
                  priv->volume_label[0] ? priv->volume_label : "NO_LABEL",
                  priv->total_bytes);

    return priv;
}

/* Unmount: Cleanup FAT32 volume */
static void fat32_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[FAT32] Unmounted volume\n");
}

/* Get statistics */
static bool fat32_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    FAT32Private* priv = (FAT32Private*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = priv->free_bytes;
    return true;
}

/* Read operation - stub for now */
static bool fat32_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                       void* buffer, size_t length, size_t* bytes_read) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    /* TODO: Implement file reading via existing FAT32 code */
    serial_printf("[FAT32] Read operation not yet implemented\n");

    if (bytes_read) *bytes_read = 0;
    return false;
}

/* Write operation - stub for now */
static bool fat32_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                        const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[FAT32] Write operation not yet implemented\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation - stub for now */
static bool fat32_enumerate(VFSVolume* vol, uint64_t dir_id,
                            bool (*callback)(void* user_data, const char* name,
                                             uint64_t id, bool is_dir),
                            void* user_data) {
    (void)vol; (void)dir_id; (void)callback; (void)user_data;

    /* TODO: Implement directory enumeration */
    serial_printf("[FAT32] Enumerate operation not yet implemented\n");
    return false;
}

/* Lookup operation - stub for now */
static bool fat32_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                         uint64_t* entry_id, bool* is_dir) {
    (void)vol; (void)dir_id; (void)name;

    /* TODO: Implement lookup */
    serial_printf("[FAT32] Lookup operation not yet implemented\n");

    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* FAT32 Filesystem Operations */
static FileSystemOps FAT32_ops = {
    .fs_name = "FAT32",
    .fs_version = 1,
    .probe = fat32_probe,
    .mount = fat32_mount,
    .unmount = fat32_unmount,
    .read = fat32_read,
    .write = fat32_write,
    .enumerate = fat32_enumerate,
    .lookup = fat32_lookup,
    .get_stats = fat32_get_stats,
    /* Optional operations not implemented yet */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Get FAT32 operations structure (for registry) */
FileSystemOps* FAT32_GetOps(void) {
    return &FAT32_ops;
}
