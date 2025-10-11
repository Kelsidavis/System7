/* ISO 9660 Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for ISO 9660 (CD-ROM).
 * It provides detection, mounting, and read-only operations for ISO 9660 volumes.
 *
 * Note: This is a read-only implementation supporting basic ISO 9660 Level 1/2.
 * Rock Ridge and Joliet extensions are not currently supported.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/iso9660_types.h"
#include "../../../include/FS/hfs_diskio.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Read 16-bit both-endian value (use LSB) */
static uint16_t iso_read16(const uint8_t* lsb, const uint8_t* msb) {
    (void)msb;  /* Ignore MSB for now, use LSB */
    return lsb[0] | ((uint16_t)lsb[1] << 8);
}

/* Read 32-bit both-endian value (use LSB) */
static uint32_t iso_read32(const uint8_t* lsb, const uint8_t* msb) {
    (void)msb;  /* Ignore MSB for now, use LSB */
    return lsb[0] | ((uint32_t)lsb[1] << 8) |
           ((uint32_t)lsb[2] << 16) | ((uint32_t)lsb[3] << 24);
}

/* ISO 9660 private volume data */
typedef struct {
    ISO9660_PrimaryVolumeDescriptor pvd;  /* Primary volume descriptor */
    BlockDevice*     vfs_blockdev;        /* VFS block device */
    uint64_t         total_bytes;
    uint32_t         block_size;
    uint32_t         root_extent;         /* Root directory extent location */
    uint32_t         root_size;           /* Root directory size */
    bool             volume_initialized;
    char             volume_name[33];
} ISO9660Private;

/* Probe: Detect ISO 9660 filesystem */
static bool iso9660_probe(BlockDevice* dev) {
    uint8_t buffer[ISO_SECTOR_SIZE];

    /* ISO 9660 primary volume descriptor is at sector 16 */
    uint32_t sector = ISO_VD_START_SECTOR;
    uint32_t block = (sector * ISO_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, block, buffer)) {
        return false;
    }

    /* Check for primary volume descriptor */
    if (buffer[0] == ISO_VD_PRIMARY &&
        memcmp(&buffer[1], ISO_STANDARD_ID, 5) == 0 &&
        buffer[6] == ISO_STANDARD_VERSION) {
        serial_printf("[ISO9660] ISO 9660 filesystem detected\\n");
        return true;
    }

    return false;
}

/* Mount: Initialize ISO 9660 volume */
static void* iso9660_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t buffer[ISO_SECTOR_SIZE];

    /* Allocate private data */
    ISO9660Private* priv = (ISO9660Private*)malloc(sizeof(ISO9660Private));
    if (!priv) {
        serial_printf("[ISO9660] Failed to allocate private data\\n");
        return NULL;
    }

    memset(priv, 0, sizeof(ISO9660Private));
    priv->vfs_blockdev = dev;

    /* Read primary volume descriptor at sector 16 */
    uint32_t sector = ISO_VD_START_SECTOR;
    uint32_t blocks_needed = (ISO_SECTOR_SIZE + dev->block_size - 1) / dev->block_size;
    uint32_t start_block = (sector * ISO_SECTOR_SIZE) / dev->block_size;

    /* Read blocks containing the PVD */
    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (!dev->read_block(dev, start_block + i, &buffer[i * dev->block_size])) {
            serial_printf("[ISO9660] Failed to read primary volume descriptor\\n");
            free(priv);
            return NULL;
        }
    }

    /* Copy primary volume descriptor */
    memcpy(&priv->pvd, buffer, sizeof(ISO9660_PrimaryVolumeDescriptor));

    /* Verify magic */
    if (priv->pvd.type != ISO_VD_PRIMARY ||
        memcmp(priv->pvd.id, ISO_STANDARD_ID, 5) != 0 ||
        priv->pvd.version != ISO_STANDARD_VERSION) {
        serial_printf("[ISO9660] Invalid primary volume descriptor\\n");
        free(priv);
        return NULL;
    }

    /* Get block size */
    priv->block_size = iso_read16((uint8_t*)&priv->pvd.logical_block_size_lsb,
                                  (uint8_t*)&priv->pvd.logical_block_size_msb);

    /* Get volume size */
    uint32_t volume_blocks = iso_read32((uint8_t*)&priv->pvd.volume_space_size_lsb,
                                        (uint8_t*)&priv->pvd.volume_space_size_msb);
    priv->total_bytes = (uint64_t)volume_blocks * priv->block_size;

    /* Extract volume name */
    memcpy(priv->volume_name, priv->pvd.volume_id, 32);
    priv->volume_name[32] = '\\0';

    /* Trim trailing spaces */
    for (int i = 31; i >= 0 && (priv->volume_name[i] == ' ' || priv->volume_name[i] == '\\0'); i--) {
        priv->volume_name[i] = '\\0';
    }

    /* Extract root directory information */
    ISO9660_DirectoryRecord* root_rec = (ISO9660_DirectoryRecord*)priv->pvd.root_directory_record;
    priv->root_extent = iso_read32((uint8_t*)&root_rec->extent_lsb, (uint8_t*)&root_rec->extent_msb);
    priv->root_size = iso_read32((uint8_t*)&root_rec->size_lsb, (uint8_t*)&root_rec->size_msb);

    priv->volume_initialized = true;

    serial_printf("[ISO9660] Mounted '%s' - %llu bytes (read-only)\\n",
                  priv->volume_name[0] ? priv->volume_name : "NO_LABEL",
                  priv->total_bytes);
    serial_printf("[ISO9660]   Block size: %u bytes\\n", priv->block_size);
    serial_printf("[ISO9660]   Root directory: extent %u, size %u\\n",
                  priv->root_extent, priv->root_size);

    return priv;
}

/* Unmount: Cleanup ISO 9660 volume */
static void iso9660_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[ISO9660] Unmounted volume\\n");
}

/* Get statistics */
static bool iso9660_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = 0;  /* Read-only filesystem, no free space */
    return true;
}

/* Read operation - stub for now */
static bool iso9660_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                         void* buffer, size_t length, size_t* bytes_read) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ISO9660] Read: Volume not initialized\\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* TODO: Implement file reading via directory records */
    serial_printf("[ISO9660] Read operation not yet implemented\\n");

    if (bytes_read) *bytes_read = 0;
    return false;
}

/* Write operation - not supported (read-only) */
static bool iso9660_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                          const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[ISO9660] Write operation not supported (read-only filesystem)\\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation - stub for now */
static bool iso9660_enumerate(VFSVolume* vol, uint64_t dir_id,
                              bool (*callback)(void* user_data, const char* name,
                                               uint64_t id, bool is_dir),
                              void* user_data) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ISO9660] Enumerate: Volume not initialized\\n");
        return false;
    }

    (void)dir_id; (void)callback; (void)user_data;

    /* TODO: Implement directory enumeration */
    serial_printf("[ISO9660] Enumerate operation not yet implemented\\n");
    return false;
}

/* Lookup operation - stub for now */
static bool iso9660_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                           uint64_t* entry_id, bool* is_dir) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ISO9660] Lookup: Volume not initialized\\n");
        return false;
    }

    (void)dir_id; (void)name;

    /* TODO: Implement lookup */
    serial_printf("[ISO9660] Lookup operation not yet implemented\\n");

    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* Get file/directory information */
static bool iso9660_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                   uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;

    /* Handle root directory specially */
    if (priv && priv->volume_initialized && entry_id == priv->root_extent) {
        if (size) *size = priv->root_size;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        serial_printf("[ISO9660] get_file_info: root directory (extent %llu)\\n", entry_id);
        return true;
    }

    /* TODO: Implement via directory record lookup */
    if (size) *size = 0;
    if (is_dir) *is_dir = false;
    if (mod_time) *mod_time = 0;

    serial_printf("[ISO9660] get_file_info: extent %llu (limited info available)\\n", entry_id);
    return true;
}

/* ISO 9660 Filesystem Operations */
static FileSystemOps ISO9660_ops = {
    .fs_name = "iso9660",
    .fs_version = 1,
    .probe = iso9660_probe,
    .mount = iso9660_mount,
    .unmount = iso9660_unmount,
    .read = iso9660_read,
    .write = iso9660_write,
    .enumerate = iso9660_enumerate,
    .lookup = iso9660_lookup,
    .get_stats = iso9660_get_stats,
    .get_file_info = iso9660_get_file_info,
    /* Optional operations not implemented (read-only filesystem) */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Get ISO 9660 operations structure (for registry) */
FileSystemOps* ISO9660_GetOps(void) {
    return &ISO9660_ops;
}
