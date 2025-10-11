/* ext4 Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for ext4.
 * It provides detection, mounting, and basic read-only operations for ext4 volumes.
 *
 * Note: This is a simplified read-only implementation. Full ext4 support would
 * require extent tree parsing, journal support, and many other features.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/ext4_types.h"
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

/* ext4 private volume data */
typedef struct {
    EXT4_Superblock superblock;    /* Full ext4 superblock */
    BlockDevice*    vfs_blockdev;  /* VFS block device */
    uint64_t        total_bytes;
    uint64_t        free_bytes;
    uint32_t        block_size;
    bool            volume_initialized;
    char            volume_name[17];
} EXT4Private;

/* Probe: Detect ext4 filesystem */
static bool ext4_probe(BlockDevice* dev) {
    uint8_t buffer[EXT4_SUPERBLOCK_SIZE];

    /* ext4 superblock starts at byte 1024 (block 2 for 512-byte blocks) */
    uint32_t sb_block = EXT4_SUPERBLOCK_OFFSET / dev->block_size;
    uint32_t sb_offset = EXT4_SUPERBLOCK_OFFSET % dev->block_size;

    /* Read the block containing the superblock */
    if (!dev->read_block(dev, sb_block, buffer)) {
        return false;
    }

    /* Check magic number at offset within block */
    uint16_t magic = le16_read(&buffer[sb_offset + 56]); /* s_magic offset is 56 */

    if (magic == EXT4_SUPER_MAGIC) {
        serial_printf("[ext4] ext4 filesystem detected (magic: 0x%04X)\n", magic);
        return true;
    }

    return false;
}

/* Mount: Initialize ext4 volume */
static void* ext4_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t buffer[2048];  /* Large enough for superblock */

    /* Allocate private data */
    EXT4Private* priv = (EXT4Private*)malloc(sizeof(EXT4Private));
    if (!priv) {
        serial_printf("[ext4] Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(EXT4Private));
    priv->vfs_blockdev = dev;

    /* Read superblock (starts at byte 1024) */
    uint32_t sb_block = EXT4_SUPERBLOCK_OFFSET / dev->block_size;
    uint32_t blocks_needed = (EXT4_SUPERBLOCK_OFFSET + EXT4_SUPERBLOCK_SIZE + dev->block_size - 1) / dev->block_size;

    /* Read enough blocks to contain the full superblock */
    for (uint32_t i = 0; i < blocks_needed - sb_block; i++) {
        if (!dev->read_block(dev, sb_block + i, &buffer[i * dev->block_size])) {
            serial_printf("[ext4] Failed to read superblock blocks\n");
            free(priv);
            return NULL;
        }
    }

    /* Copy superblock from buffer */
    uint32_t sb_offset = EXT4_SUPERBLOCK_OFFSET % dev->block_size;
    memcpy(&priv->superblock, &buffer[sb_offset], sizeof(EXT4_Superblock));

    /* Verify magic number */
    if (priv->superblock.s_magic != EXT4_SUPER_MAGIC) {
        serial_printf("[ext4] Invalid magic number: 0x%04X (expected 0x%04X)\n",
                      priv->superblock.s_magic, EXT4_SUPER_MAGIC);
        free(priv);
        return NULL;
    }

    /* Calculate block size */
    priv->block_size = 1024 << priv->superblock.s_log_block_size;

    /* Calculate total and free bytes */
    uint64_t total_blocks = priv->superblock.s_blocks_count_lo;
    if (priv->superblock.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        total_blocks |= ((uint64_t)priv->superblock.s_blocks_count_hi << 32);
    }

    uint64_t free_blocks = priv->superblock.s_free_blocks_count_lo;
    if (priv->superblock.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        free_blocks |= ((uint64_t)priv->superblock.s_free_blocks_count_hi << 32);
    }

    priv->total_bytes = total_blocks * priv->block_size;
    priv->free_bytes = free_blocks * priv->block_size;

    /* Extract volume name */
    memcpy(priv->volume_name, priv->superblock.s_volume_name, 16);
    priv->volume_name[16] = '\0';

    /* Trim trailing spaces/nulls */
    for (int i = 15; i >= 0 && (priv->volume_name[i] == ' ' || priv->volume_name[i] == '\0'); i--) {
        priv->volume_name[i] = '\0';
    }

    priv->volume_initialized = true;

    serial_printf("[ext4] Mounted '%s' - %llu bytes (%llu free)\n",
                  priv->volume_name[0] ? priv->volume_name : "NO_LABEL",
                  priv->total_bytes, priv->free_bytes);
    serial_printf("[ext4]   Block size: %u bytes\n", priv->block_size);
    serial_printf("[ext4]   Inodes: %u (%u free)\n",
                  priv->superblock.s_inodes_count,
                  priv->superblock.s_free_inodes_count);

    return priv;
}

/* Unmount: Cleanup ext4 volume */
static void ext4_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[ext4] Unmounted volume\n");
}

/* Get statistics */
static bool ext4_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    EXT4Private* priv = (EXT4Private*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = priv->free_bytes;
    return true;
}

/* Read operation - stub for now */
static bool ext4_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                      void* buffer, size_t length, size_t* bytes_read) {
    EXT4Private* priv = (EXT4Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ext4] Read: Volume not initialized\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* TODO: Implement file reading via extent tree parsing */
    serial_printf("[ext4] Read operation not yet implemented\n");

    if (bytes_read) *bytes_read = 0;
    return false;
}

/* Write operation - not supported (read-only driver) */
static bool ext4_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                       const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[ext4] Write operation not supported (read-only filesystem)\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation - stub for now */
static bool ext4_enumerate(VFSVolume* vol, uint64_t dir_id,
                           bool (*callback)(void* user_data, const char* name,
                                            uint64_t id, bool is_dir),
                           void* user_data) {
    EXT4Private* priv = (EXT4Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ext4] Enumerate: Volume not initialized\n");
        return false;
    }

    (void)dir_id; (void)callback; (void)user_data;

    /* TODO: Implement directory enumeration */
    serial_printf("[ext4] Enumerate operation not yet implemented\n");
    return false;
}

/* Lookup operation - stub for now */
static bool ext4_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                        uint64_t* entry_id, bool* is_dir) {
    EXT4Private* priv = (EXT4Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[ext4] Lookup: Volume not initialized\n");
        return false;
    }

    (void)dir_id; (void)name;

    /* TODO: Implement lookup */
    serial_printf("[ext4] Lookup operation not yet implemented\n");

    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* Get file/directory information */
static bool ext4_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    EXT4Private* priv = (EXT4Private*)vol->fs_private;

    /* Handle root directory specially */
    if (priv && priv->volume_initialized && entry_id == EXT4_ROOT_INO) {
        if (size) *size = 0;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        serial_printf("[ext4] get_file_info: root directory (inode %llu)\n", entry_id);
        return true;
    }

    /* TODO: Implement via inode lookup */
    if (size) *size = 0;
    if (is_dir) *is_dir = false;
    if (mod_time) *mod_time = 0;

    serial_printf("[ext4] get_file_info: inode %llu (limited info available)\n", entry_id);
    return true;
}

/* ext4 Filesystem Operations */
static FileSystemOps EXT4_ops = {
    .fs_name = "ext4",
    .fs_version = 1,
    .probe = ext4_probe,
    .mount = ext4_mount,
    .unmount = ext4_unmount,
    .read = ext4_read,
    .write = ext4_write,
    .enumerate = ext4_enumerate,
    .lookup = ext4_lookup,
    .get_stats = ext4_get_stats,
    .get_file_info = ext4_get_file_info,
    /* Optional operations not implemented */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Get ext4 operations structure (for registry) */
FileSystemOps* EXT4_GetOps(void) {
    return &EXT4_ops;
}
