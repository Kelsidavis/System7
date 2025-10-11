/* exFAT Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for exFAT.
 * It provides detection, mounting, and basic read-only operations for exFAT volumes.
 *
 * Note: This is a simplified read-only implementation. Full exFAT support would
 * require proper cluster chain following, file name handling, and write support.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/exfat_types.h"
#include "../../../include/FS/hfs_diskio.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Read 16-bit little-endian value */
static uint16_t le16_read(const uint8_t* buf) {
    return buf[0] | ((uint16_t)buf[1] << 8);
}

/* Read 32-bit little-endian value */
static uint32_t le32_read(const uint8_t* buf) {
    return buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* Read 64-bit little-endian value */
static uint64_t le64_read(const uint8_t* buf) {
    return (uint64_t)buf[0] |
           ((uint64_t)buf[1] << 8) |
           ((uint64_t)buf[2] << 16) |
           ((uint64_t)buf[3] << 24) |
           ((uint64_t)buf[4] << 32) |
           ((uint64_t)buf[5] << 40) |
           ((uint64_t)buf[6] << 48) |
           ((uint64_t)buf[7] << 56);
}

/* exFAT private volume data */
typedef struct {
    EXFAT_BootSector boot_sector;    /* Full exFAT boot sector */
    BlockDevice*     vfs_blockdev;   /* VFS block device */
    uint64_t         total_bytes;
    uint64_t         free_bytes;
    uint32_t         bytes_per_sector;
    uint32_t         sectors_per_cluster;
    uint32_t         bytes_per_cluster;
    uint32_t         cluster_heap_offset_sectors;
    uint32_t         root_dir_cluster;
    bool             volume_initialized;
    char             volume_name[23];  /* 11 UTF-16 chars = 22 bytes + null */
} EXFATPrivate;

/* Probe: Detect exFAT filesystem */
static bool exfat_probe(BlockDevice* dev) {
    uint8_t buffer[512];

    /* exFAT boot sector starts at byte 0 */
    if (!dev->read_block(dev, 0, buffer)) {
        return false;
    }

    /* Check file system name at offset 3 */
    if (memcmp(&buffer[3], EXFAT_SIGNATURE, 8) == 0) {
        /* Check boot signature at offset 510 */
        uint16_t boot_sig = le16_read(&buffer[510]);
        if (boot_sig == EXFAT_BOOT_SIGNATURE) {
            serial_printf("[exFAT] exFAT filesystem detected\\n");
            return true;
        }
    }

    return false;
}

/* Mount: Initialize exFAT volume */
static void* exfat_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t buffer[512];

    /* Allocate private data */
    EXFATPrivate* priv = (EXFATPrivate*)malloc(sizeof(EXFATPrivate));
    if (!priv) {
        serial_printf("[exFAT] Failed to allocate private data\\n");
        return NULL;
    }

    memset(priv, 0, sizeof(EXFATPrivate));
    priv->vfs_blockdev = dev;

    /* Read boot sector */
    if (!dev->read_block(dev, 0, buffer)) {
        serial_printf("[exFAT] Failed to read boot sector\\n");
        free(priv);
        return NULL;
    }

    /* Copy boot sector from buffer */
    memcpy(&priv->boot_sector, buffer, sizeof(EXFAT_BootSector));

    /* Verify signature */
    if (memcmp(priv->boot_sector.FileSystemName, EXFAT_SIGNATURE, 8) != 0) {
        serial_printf("[exFAT] Invalid file system signature\\n");
        free(priv);
        return NULL;
    }

    if (priv->boot_sector.BootSignature != EXFAT_BOOT_SIGNATURE) {
        serial_printf("[exFAT] Invalid boot signature: 0x%04X (expected 0x%04X)\\n",
                      priv->boot_sector.BootSignature, EXFAT_BOOT_SIGNATURE);
        free(priv);
        return NULL;
    }

    /* Calculate sector and cluster sizes */
    priv->bytes_per_sector = 1 << priv->boot_sector.BytesPerSectorShift;
    priv->sectors_per_cluster = 1 << priv->boot_sector.SectorsPerClusterShift;
    priv->bytes_per_cluster = priv->bytes_per_sector * priv->sectors_per_cluster;

    /* Calculate cluster heap offset */
    priv->cluster_heap_offset_sectors = priv->boot_sector.ClusterHeapOffset;

    /* Get root directory cluster */
    priv->root_dir_cluster = priv->boot_sector.FirstClusterOfRootDirectory;

    /* Calculate total bytes */
    uint64_t volume_sectors = priv->boot_sector.VolumeLength;
    priv->total_bytes = volume_sectors * priv->bytes_per_sector;

    /* Calculate free bytes (estimate based on percent in use) */
    uint32_t percent_used = priv->boot_sector.PercentInUse;
    if (percent_used <= 100) {
        uint32_t percent_free = 100 - percent_used;
        /* Avoid 64-bit division by using 32-bit arithmetic */
        uint32_t data_clusters = priv->boot_sector.ClusterCount;
        uint32_t free_clusters = (data_clusters * percent_free) / 100;
        priv->free_bytes = (uint64_t)free_clusters * priv->bytes_per_cluster;
    } else {
        priv->free_bytes = 0;  /* Invalid percent, assume full */
    }

    /* Extract volume name - will be done lazily when reading root directory */
    strcpy(priv->volume_name, "NO_LABEL");

    priv->volume_initialized = true;

    serial_printf("[exFAT] Mounted '%s' - %llu bytes (~%llu free)\\n",
                  priv->volume_name,
                  priv->total_bytes, priv->free_bytes);
    serial_printf("[exFAT]   Bytes/sector: %u, Sectors/cluster: %u\\n",
                  priv->bytes_per_sector, priv->sectors_per_cluster);
    serial_printf("[exFAT]   Root dir cluster: %u, Clusters: %u\\n",
                  priv->root_dir_cluster, priv->boot_sector.ClusterCount);
    serial_printf("[exFAT]   File system revision: %u.%u\\n",
                  (priv->boot_sector.FileSystemRevision >> 8) & 0xFF,
                  priv->boot_sector.FileSystemRevision & 0xFF);

    return priv;
}

/* Unmount: Cleanup exFAT volume */
static void exfat_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[exFAT] Unmounted volume\\n");
}

/* Get statistics */
static bool exfat_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    EXFATPrivate* priv = (EXFATPrivate*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = priv->free_bytes;
    return true;
}

/* Read operation - stub for now */
static bool exfat_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                       void* buffer, size_t length, size_t* bytes_read) {
    EXFATPrivate* priv = (EXFATPrivate*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[exFAT] Read: Volume not initialized\\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* TODO: Implement file reading via cluster chain following */
    serial_printf("[exFAT] Read operation not yet implemented\\n");

    if (bytes_read) *bytes_read = 0;
    return false;
}

/* Write operation - not supported (read-only driver) */
static bool exfat_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                        const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[exFAT] Write operation not supported (read-only filesystem)\\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation - stub for now */
static bool exfat_enumerate(VFSVolume* vol, uint64_t dir_id,
                            bool (*callback)(void* user_data, const char* name,
                                             uint64_t id, bool is_dir),
                            void* user_data) {
    EXFATPrivate* priv = (EXFATPrivate*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[exFAT] Enumerate: Volume not initialized\\n");
        return false;
    }

    (void)dir_id; (void)callback; (void)user_data;

    /* TODO: Implement directory enumeration */
    serial_printf("[exFAT] Enumerate operation not yet implemented\\n");
    return false;
}

/* Lookup operation - stub for now */
static bool exfat_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                         uint64_t* entry_id, bool* is_dir) {
    EXFATPrivate* priv = (EXFATPrivate*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[exFAT] Lookup: Volume not initialized\\n");
        return false;
    }

    (void)dir_id; (void)name;

    /* TODO: Implement lookup */
    serial_printf("[exFAT] Lookup operation not yet implemented\\n");

    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* Get file/directory information */
static bool exfat_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                 uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    EXFATPrivate* priv = (EXFATPrivate*)vol->fs_private;

    /* Handle root directory specially */
    if (priv && priv->volume_initialized && entry_id == priv->root_dir_cluster) {
        if (size) *size = 0;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        serial_printf("[exFAT] get_file_info: root directory (cluster %llu)\\n", entry_id);
        return true;
    }

    /* TODO: Implement via directory entry lookup */
    if (size) *size = 0;
    if (is_dir) *is_dir = false;
    if (mod_time) *mod_time = 0;

    serial_printf("[exFAT] get_file_info: cluster %llu (limited info available)\\n", entry_id);
    return true;
}

/* exFAT Filesystem Operations */
static FileSystemOps EXFAT_ops = {
    .fs_name = "exfat",
    .fs_version = 1,
    .probe = exfat_probe,
    .mount = exfat_mount,
    .unmount = exfat_unmount,
    .read = exfat_read,
    .write = exfat_write,
    .enumerate = exfat_enumerate,
    .lookup = exfat_lookup,
    .get_stats = exfat_get_stats,
    .get_file_info = exfat_get_file_info,
    /* Optional operations not implemented */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Get exFAT operations structure (for registry) */
FileSystemOps* EXFAT_GetOps(void) {
    return &EXFAT_ops;
}
