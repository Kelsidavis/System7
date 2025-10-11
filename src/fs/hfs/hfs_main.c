/* HFS Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for HFS (Hierarchical File System).
 * It provides detection, mounting, and basic I/O operations for HFS volumes.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/hfs_types.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* HFS signature in MDB */
#define HFS_SIGNATURE 0x4244  /* 'BD' */
#define HFS_MDB_SECTOR 2

/* Read 16-bit big-endian value */
static uint16_t be16_read(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

/* Read 32-bit big-endian value */
static uint32_t be32_read(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

/* HFS private volume data */
typedef struct {
    uint32_t total_blocks;
    uint32_t block_size;
    uint64_t total_bytes;
    uint64_t free_bytes;
    char     volume_name[32];
} HFSPrivate;

/* Probe: Detect HFS filesystem */
static bool hfs_probe(BlockDevice* dev) {
    uint8_t sector[512];

    /* Read MDB sector */
    if (!dev->read_block(dev, HFS_MDB_SECTOR, sector)) {
        return false;
    }

    /* Check HFS signature */
    uint16_t sig = be16_read(&sector[0]);
    if (sig == HFS_SIGNATURE) {
        serial_printf("[HFS] HFS filesystem detected (signature: 0x%04x)\n", sig);
        return true;
    }

    return false;
}

/* Mount: Initialize HFS volume */
static void* hfs_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t sector[512];

    /* Read MDB */
    if (!dev->read_block(dev, HFS_MDB_SECTOR, sector)) {
        serial_printf("[HFS] Failed to read MDB\n");
        return NULL;
    }

    /* Allocate private data */
    HFSPrivate* priv = (HFSPrivate*)malloc(sizeof(HFSPrivate));
    if (!priv) {
        serial_printf("[HFS] Failed to allocate private data\n");
        return NULL;
    }

    /* Parse MDB fields */
    uint16_t num_alloc_blocks = be16_read(&sector[20]);
    uint32_t alloc_block_size = be32_read(&sector[22]);
    uint16_t free_blocks = be16_read(&sector[36]);

    priv->total_blocks = num_alloc_blocks;
    priv->block_size = alloc_block_size;
    priv->total_bytes = (uint64_t)num_alloc_blocks * alloc_block_size;
    priv->free_bytes = (uint64_t)free_blocks * alloc_block_size;

    /* Extract volume name (Pascal string at offset 38) */
    uint8_t name_len = sector[38];
    if (name_len > 27) name_len = 27;
    memcpy(priv->volume_name, &sector[39], name_len);
    priv->volume_name[name_len] = '\0';

    serial_printf("[HFS] Mounted '%s' - %llu bytes (%llu free)\n",
                  priv->volume_name, priv->total_bytes, priv->free_bytes);

    return priv;
}

/* Unmount: Cleanup HFS volume */
static void hfs_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[HFS] Unmounted volume\n");
}

/* Get statistics */
static bool hfs_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    HFSPrivate* priv = (HFSPrivate*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = priv->free_bytes;
    return true;
}

/* Read operation - stub for now */
static bool hfs_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                     void* buffer, size_t length, size_t* bytes_read) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    /* TODO: Implement file reading via existing HFS code */
    serial_printf("[HFS] Read operation not yet implemented\n");

    if (bytes_read) *bytes_read = 0;
    return false;
}

/* Write operation - stub for now */
static bool hfs_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                      const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[HFS] Write operation not yet implemented\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation - stub for now */
static bool hfs_enumerate(VFSVolume* vol, uint64_t dir_id,
                          bool (*callback)(void* user_data, const char* name,
                                           uint64_t id, bool is_dir),
                          void* user_data) {
    (void)vol; (void)dir_id; (void)callback; (void)user_data;

    /* TODO: Implement directory enumeration via existing HFS catalog */
    serial_printf("[HFS] Enumerate operation not yet implemented\n");
    return false;
}

/* Lookup operation - stub for now */
static bool hfs_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                       uint64_t* entry_id, bool* is_dir) {
    (void)vol; (void)dir_id; (void)name;

    /* TODO: Implement lookup via existing HFS catalog */
    serial_printf("[HFS] Lookup operation not yet implemented\n");

    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* HFS Filesystem Operations */
static FileSystemOps HFS_ops = {
    .fs_name = "HFS",
    .fs_version = 1,
    .probe = hfs_probe,
    .mount = hfs_mount,
    .unmount = hfs_unmount,
    .read = hfs_read,
    .write = hfs_write,
    .enumerate = hfs_enumerate,
    .lookup = hfs_lookup,
    .get_stats = hfs_get_stats,
    /* Optional operations not implemented yet */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Get HFS operations structure (for registry) */
FileSystemOps* HFS_GetOps(void) {
    return &HFS_ops;
}
