/* HFS Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for HFS (Hierarchical File System).
 * It provides detection, mounting, and basic I/O operations for HFS volumes.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/hfs_types.h"
#include "../../../include/FS/hfs_volume.h"
#include "../../../include/FS/hfs_catalog.h"
#include "../../../include/FS/hfs_file.h"
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
    HFS_Volume   volume;      /* Full HFS volume structure */
    HFS_Catalog  catalog;     /* Catalog B-tree */
    uint64_t     total_bytes;
    uint64_t     free_bytes;
    char         volume_name[32];
    bool         catalog_initialized;
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

    /* Allocate private data */
    HFSPrivate* priv = (HFSPrivate*)malloc(sizeof(HFSPrivate));
    if (!priv) {
        serial_printf("[HFS] Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(HFSPrivate));

    /* Read MDB for basic volume info */
    if (!dev->read_block(dev, HFS_MDB_SECTOR, sector)) {
        serial_printf("[HFS] Failed to read MDB\n");
        free(priv);
        return NULL;
    }

    /* Parse basic MDB fields for volume stats */
    uint16_t num_alloc_blocks = be16_read(&sector[20]);
    uint32_t alloc_block_size = be32_read(&sector[22]);
    uint16_t free_blocks = be16_read(&sector[36]);

    priv->total_bytes = (uint64_t)num_alloc_blocks * alloc_block_size;
    priv->free_bytes = (uint64_t)free_blocks * alloc_block_size;

    /* Extract volume name (Pascal string at offset 38) */
    uint8_t name_len = sector[38];
    if (name_len > 27) name_len = 27;
    memcpy(priv->volume_name, &sector[39], name_len);
    priv->volume_name[name_len] = '\0';

    /* TODO: Initialize HFS_Volume and HFS_Catalog structures properly
     * For now, catalog operations will not be available until
     * full HFS volume initialization is implemented */
    priv->catalog_initialized = false;

    serial_printf("[HFS] Mounted '%s' - %llu bytes (%llu free)\n",
                  priv->volume_name, priv->total_bytes, priv->free_bytes);
    serial_printf("[HFS] Note: Catalog operations require full HFS volume initialization\n");

    return priv;
}

/* Unmount: Cleanup HFS volume */
static void hfs_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        HFSPrivate* priv = (HFSPrivate*)vol->fs_private;

        /* Close catalog if initialized */
        if (priv->catalog_initialized) {
            HFS_CatalogClose(&priv->catalog);
        }

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

/* Read operation */
static bool hfs_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                     void* buffer, size_t length, size_t* bytes_read) {
    HFSPrivate* priv = (HFSPrivate*)vol->fs_private;
    if (!priv || !priv->catalog_initialized) {
        serial_printf("[HFS] Read: Catalog not initialized\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Open file by CNID */
    HFSFile* file = HFS_FileOpen(&priv->catalog, (FileID)file_id, false);
    if (!file) {
        serial_printf("[HFS] Read: Failed to open file CNID %llu\n", file_id);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Seek to offset */
    if (offset > 0 && !HFS_FileSeek(file, (uint32_t)offset)) {
        serial_printf("[HFS] Read: Failed to seek to offset %llu\n", offset);
        HFS_FileClose(file);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Read data */
    uint32_t actual_read = 0;
    bool success = HFS_FileRead(file, buffer, (uint32_t)length, &actual_read);

    HFS_FileClose(file);

    if (bytes_read) *bytes_read = actual_read;

    if (success) {
        serial_printf("[HFS] Read: Read %u bytes from CNID %llu at offset %llu\n",
                      actual_read, file_id, offset);
    } else {
        serial_printf("[HFS] Read: Failed for CNID %llu\n", file_id);
    }

    return success;
}

/* Write operation - stub for now */
static bool hfs_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                      const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[HFS] Write operation not yet implemented\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation */
static bool hfs_enumerate(VFSVolume* vol, uint64_t dir_id,
                          bool (*callback)(void* user_data, const char* name,
                                           uint64_t id, bool is_dir),
                          void* user_data) {
    HFSPrivate* priv = (HFSPrivate*)vol->fs_private;
    if (!priv || !priv->catalog_initialized) {
        serial_printf("[HFS] Enumerate: Catalog not initialized\n");
        return false;
    }

    /* Enumerate directory entries */
    CatEntry entries[64];
    int count = 0;

    if (!HFS_CatalogEnumerate(&priv->catalog, (DirID)dir_id, entries, 64, &count)) {
        serial_printf("[HFS] Enumerate: Failed for dir %llu\n", dir_id);
        return false;
    }

    serial_printf("[HFS] Enumerate: Found %d entries in dir %llu\n", count, dir_id);

    /* Call callback for each entry */
    for (int i = 0; i < count; i++) {
        bool is_dir = (entries[i].kind == kNodeDir);
        if (callback && !callback(user_data, entries[i].name, entries[i].id, is_dir)) {
            break; /* Callback requested stop */
        }
    }

    return true;
}

/* Lookup operation */
static bool hfs_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                       uint64_t* entry_id, bool* is_dir) {
    HFSPrivate* priv = (HFSPrivate*)vol->fs_private;
    if (!priv || !priv->catalog_initialized) {
        serial_printf("[HFS] Lookup: Catalog not initialized\n");
        return false;
    }

    /* Use catalog lookup */
    CatEntry entry;
    if (HFS_CatalogLookup(&priv->catalog, (DirID)dir_id, name, &entry)) {
        if (entry_id) *entry_id = entry.id;
        if (is_dir) *is_dir = (entry.kind == kNodeDir);
        serial_printf("[HFS] Lookup: Found '%s' (CNID %u, is_dir=%d)\n",
                      name, entry.id, entry.kind == kNodeDir);
        return true;
    }

    serial_printf("[HFS] Lookup: Not found '%s' in dir %llu\n", name, dir_id);
    return false;
}

/* Get file/directory information */
static bool hfs_get_file_info(VFSVolume* vol, uint64_t entry_id,
                               uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    HFSPrivate* priv = (HFSPrivate*)vol->fs_private;

    /* Handle root directory specially */
    if (entry_id == 1 || entry_id == 2) {
        if (size) *size = 0;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        serial_printf("[HFS] get_file_info: root directory (CNID %llu)\n", entry_id);
        return true;
    }

    if (!priv || !priv->catalog_initialized) {
        serial_printf("[HFS] get_file_info: Catalog not initialized\n");
        return false;
    }

    /* Get entry by CNID */
    CatEntry entry;
    if (HFS_CatalogGetByID(&priv->catalog, (FileID)entry_id, &entry)) {
        if (size) *size = entry.size;
        if (is_dir) *is_dir = (entry.kind == kNodeDir);
        if (mod_time) *mod_time = entry.modTime;
        serial_printf("[HFS] get_file_info: CNID %llu '%s' (size=%u, is_dir=%d)\n",
                      entry_id, entry.name, entry.size, entry.kind == kNodeDir);
        return true;
    }

    serial_printf("[HFS] get_file_info: CNID %llu not found\n", entry_id);
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
    .get_file_info = hfs_get_file_info,
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
