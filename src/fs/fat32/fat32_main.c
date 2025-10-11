/* FAT32 Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for FAT32.
 * It provides detection, mounting, and basic I/O operations for FAT32 volumes.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/fat32.h"
#include "../../../include/FS/fat32_types.h"
#include "../../../include/FS/hfs_diskio.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* FAT32 Boot Sector offsets for probe */
#define FAT32_SIGNATURE_OFFSET    510
#define FAT32_SIGNATURE           0xAA55
#define FAT32_FSTYPE_OFFSET       82

/* Read 16-bit little-endian value */
static uint16_t le16_read(const uint8_t* buf) {
    return buf[0] | ((uint16_t)buf[1] << 8);
}

/* Read 32-bit little-endian value */
static uint32_t le32_read(const uint8_t* buf) {
    return buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* Simple metadata cache entry for cluster->direntry mapping */
typedef struct {
    uint32_t cluster;             /* Cluster number (0 = unused) */
    FAT32_DirEntry entry;         /* Directory entry */
} FAT32_MetadataCache;

/* FAT32 private volume data */
typedef struct {
    FAT32_Volume volume;           /* Full FAT32 volume structure */
    BlockDevice* vfs_blockdev;     /* VFS block device */
    uint64_t     total_bytes;
    uint64_t     free_bytes;
    bool         volume_initialized;

    /* Metadata cache for cluster->direntry mapping */
    FAT32_MetadataCache cache[64]; /* Cache up to 64 entries */
    int cache_count;
} FAT32Private;

/* Add entry to metadata cache */
static void fat32_cache_add(FAT32Private* priv, uint32_t cluster, const FAT32_DirEntry* entry) {
    if (!priv || cluster == 0 || !entry) return;

    /* Check if already cached */
    for (int i = 0; i < priv->cache_count; i++) {
        if (priv->cache[i].cluster == cluster) {
            /* Update existing entry */
            priv->cache[i].entry = *entry;
            return;
        }
    }

    /* Add new entry if space available */
    if (priv->cache_count < 64) {
        priv->cache[priv->cache_count].cluster = cluster;
        priv->cache[priv->cache_count].entry = *entry;
        priv->cache_count++;
    }
    /* If cache full, oldest entries are not replaced - simple LRU would be better */
}

/* Lookup entry in metadata cache */
static bool fat32_cache_lookup(FAT32Private* priv, uint32_t cluster, FAT32_DirEntry* entry) {
    if (!priv || cluster == 0) return false;

    for (int i = 0; i < priv->cache_count; i++) {
        if (priv->cache[i].cluster == cluster) {
            if (entry) *entry = priv->cache[i].entry;
            return true;
        }
    }

    return false;
}

/* Initialize FAT32_Volume from VFS BlockDevice (similar to FAT32_VolumeInit but for VFS) */
static bool fat32_volume_init_vfs(FAT32_Volume* vol, BlockDevice* vfs_dev) {
    if (!vol || !vfs_dev) {
        return false;
    }

    memset(vol, 0, sizeof(FAT32_Volume));

    /* Initialize wrapped block device for VFS */
    vol->bd.type = HFS_BD_TYPE_VFS;
    vol->bd.data = vfs_dev;
    vol->bd.size = vfs_dev->total_blocks * vfs_dev->block_size;
    vol->bd.sectorSize = vfs_dev->block_size;
    vol->bd.readonly = false;
    vol->bd.ata_device = -1;

    /* Read boot sector */
    if (!HFS_BD_ReadSector(&vol->bd, 0, &vol->boot)) {
        serial_printf("[FAT32] Failed to read boot sector\n");
        return false;
    }

    /* Verify FAT32 signature */
    if (vol->boot.BS_BootSig != FAT32_BOOT_SIG) {
        serial_printf("[FAT32] Invalid boot signature: 0x%02X (expected 0x%02X)\n",
                      vol->boot.BS_BootSig, FAT32_BOOT_SIG);
        return false;
    }

    /* Verify it's FAT32 (not FAT16 or FAT12) */
    uint16_t fatSz16 = le16_read((uint8_t*)&vol->boot.BPB_FATSz16);
    uint16_t rootEntCnt = le16_read((uint8_t*)&vol->boot.BPB_RootEntCnt);

    if (fatSz16 != 0 || rootEntCnt != 0) {
        serial_printf("[FAT32] Not a FAT32 volume (appears to be FAT12/FAT16)\n");
        return false;
    }

    /* Calculate important values */
    vol->fatStartSector = le16_read((uint8_t*)&vol->boot.BPB_RsvdSecCnt);

    uint32_t fatSize;
    memcpy(&fatSize, &vol->boot.BPB_FATSz32, 4);
    fatSize = le32_read((uint8_t*)&fatSize);

    uint32_t rootDirSectors = 0;  /* FAT32 has no fixed root dir */

    uint32_t totSec32;
    memcpy(&totSec32, &vol->boot.BPB_TotSec32, 4);
    totSec32 = le32_read((uint8_t*)&totSec32);

    uint32_t dataSectors = totSec32 -
                          (vol->fatStartSector +
                           (vol->boot.BPB_NumFATs * fatSize) +
                           rootDirSectors);

    vol->dataStartSector = vol->fatStartSector +
                          (vol->boot.BPB_NumFATs * fatSize);

    uint32_t rootClus;
    memcpy(&rootClus, &vol->boot.BPB_RootClus, 4);
    vol->rootDirCluster = le32_read((uint8_t*)&rootClus);

    vol->clusterSize = vol->boot.BPB_SecPerClus * FAT32_SECTOR_SIZE;
    vol->totalClusters = dataSectors / vol->boot.BPB_SecPerClus;

    /* Extract volume label */
    memcpy(vol->volumeLabel, vol->boot.BS_VolLab, 11);
    vol->volumeLabel[11] = '\0';
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && vol->volumeLabel[i] == ' '; i--) {
        vol->volumeLabel[i] = '\0';
    }

    vol->fatCacheValid = false;
    vol->mounted = true;

    serial_printf("[FAT32] Initialized volume: '%s'\n",
                  vol->volumeLabel[0] ? vol->volumeLabel : "NO_LABEL");
    serial_printf("[FAT32]   Cluster size: %u bytes\n", vol->clusterSize);
    serial_printf("[FAT32]   Total clusters: %u\n", vol->totalClusters);
    serial_printf("[FAT32]   Root dir cluster: %u\n", vol->rootDirCluster);

    return true;
}

/* Forward declaration */
static bool fat32_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                 uint64_t* size, bool* is_dir, uint64_t* mod_time);

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
    /* Allocate private data */
    FAT32Private* priv = (FAT32Private*)malloc(sizeof(FAT32Private));
    if (!priv) {
        serial_printf("[FAT32] Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(FAT32Private));
    priv->vfs_blockdev = dev;
    priv->cache_count = 0;  /* Initialize cache */

    /* Try to initialize FAT32 volume */
    if (fat32_volume_init_vfs(&priv->volume, dev)) {
        priv->volume_initialized = true;
        priv->total_bytes = (uint64_t)priv->volume.totalClusters * priv->volume.clusterSize;
        priv->free_bytes = priv->total_bytes;  /* TODO: Calculate from FSInfo */

        serial_printf("[FAT32] Mounted '%s' - %llu bytes (%llu free)\n",
                      priv->volume.volumeLabel[0] ? priv->volume.volumeLabel : "NO_LABEL",
                      priv->total_bytes, priv->free_bytes);
    } else {
        serial_printf("[FAT32] WARNING: Volume initialization failed - read-only mode\n");
        priv->volume_initialized = false;
        priv->total_bytes = 0;
        priv->free_bytes = 0;
    }

    return priv;
}

/* Unmount: Cleanup FAT32 volume */
static void fat32_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        FAT32Private* priv = (FAT32Private*)vol->fs_private;

        /* Unmount FAT32 volume if initialized */
        if (priv->volume_initialized) {
            FAT32_VolumeUnmount(&priv->volume);
        }

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

/* Read operation */
static bool fat32_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                       void* buffer, size_t length, size_t* bytes_read) {
    FAT32Private* priv = (FAT32Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[FAT32] Read: Volume not initialized\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* file_id is the cluster number of the file
     * To read a file, we need to:
     * 1. Get file size from directory entry (we'll need to look this up)
     * 2. Open file with FAT32_FileOpen
     * 3. Seek if offset > 0
     * 4. Read data
     * 5. Close file
     *
     * For now, we'll assume the caller has already looked up the file size
     * and use a simple approach: open, seek, read, close
     */

    /* Get file info to determine size - use catalog lookup */
    /* Note: For proper implementation, we'd need parent directory + name lookup
     * For now, we'll use a simplified approach assuming the file_id lookup worked */

    /* We need file size - let's get it from get_file_info */
    uint64_t file_size = 0;
    bool is_dir = false;
    if (!fat32_get_file_info(vol, file_id, &file_size, &is_dir, NULL)) {
        serial_printf("[FAT32] Read: Failed to get file info for cluster %llu\n", file_id);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    if (is_dir) {
        serial_printf("[FAT32] Read: Cannot read directory as file\n");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Open file */
    FAT32_File* file = FAT32_FileOpen(&priv->volume, (uint32_t)file_id, (uint32_t)file_size);
    if (!file) {
        serial_printf("[FAT32] Read: Failed to open file cluster %llu\n", file_id);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Seek to offset if needed */
    if (offset > 0 && !FAT32_FileSeek(file, (uint32_t)offset)) {
        serial_printf("[FAT32] Read: Failed to seek to offset %llu\n", offset);
        FAT32_FileClose(file);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* Read data */
    uint32_t actual_read = 0;
    bool success = FAT32_FileRead(file, buffer, (uint32_t)length, &actual_read);

    FAT32_FileClose(file);

    if (bytes_read) *bytes_read = actual_read;

    if (success) {
        serial_printf("[FAT32] Read: Read %u bytes from cluster %llu at offset %llu\n",
                      actual_read, file_id, offset);
    } else {
        serial_printf("[FAT32] Read: Failed for cluster %llu\n", file_id);
    }

    return success;
}

/* Write operation - stub for now */
static bool fat32_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                        const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;

    serial_printf("[FAT32] Write operation not yet implemented\n");

    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate operation */
static bool fat32_enumerate(VFSVolume* vol, uint64_t dir_id,
                            bool (*callback)(void* user_data, const char* name,
                                             uint64_t id, bool is_dir),
                            void* user_data) {
    FAT32Private* priv = (FAT32Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[FAT32] Enumerate: Volume not initialized\n");
        return false;
    }

    /* Enumerate directory entries */
    CatEntry entries[64];
    int count = 0;

    if (!FAT32_ReadDirEntries(&priv->volume, (uint32_t)dir_id, entries, 64, &count)) {
        serial_printf("[FAT32] Enumerate: Failed for dir cluster %llu\n", dir_id);
        return false;
    }

    serial_printf("[FAT32] Enumerate: Found %d entries in dir cluster %llu\n", count, dir_id);

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
static bool fat32_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                         uint64_t* entry_id, bool* is_dir) {
    FAT32Private* priv = (FAT32Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        serial_printf("[FAT32] Lookup: Volume not initialized\n");
        return false;
    }

    /* Use FAT32 lookup */
    FAT32_DirEntry entry;
    uint32_t cluster = 0;

    if (FAT32_Lookup(&priv->volume, (uint32_t)dir_id, name, &entry, &cluster)) {
        /* Cache the directory entry for later use by get_file_info and read */
        fat32_cache_add(priv, cluster, &entry);

        if (entry_id) *entry_id = cluster;
        if (is_dir) *is_dir = (entry.DIR_Attr & FAT32_ATTR_DIRECTORY) != 0;
        serial_printf("[FAT32] Lookup: Found '%s' (cluster %u, is_dir=%d)\n",
                      name, cluster, (entry.DIR_Attr & FAT32_ATTR_DIRECTORY) != 0);
        return true;
    }

    serial_printf("[FAT32] Lookup: Not found '%s' in dir cluster %llu\n", name, dir_id);
    return false;
}

/* Get file/directory information */
static bool fat32_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                 uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    FAT32Private* priv = (FAT32Private*)vol->fs_private;

    /* Handle root directory specially */
    if (priv && priv->volume_initialized && entry_id == priv->volume.rootDirCluster) {
        if (size) *size = 0;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        serial_printf("[FAT32] get_file_info: root directory (cluster %llu)\n", entry_id);
        return true;
    }

    /* Try to get info from metadata cache */
    FAT32_DirEntry entry;
    if (priv && fat32_cache_lookup(priv, (uint32_t)entry_id, &entry)) {
        uint32_t fileSize;
        memcpy(&fileSize, &entry.DIR_FileSize, 4);
        fileSize = le16_read((uint8_t*)&fileSize) |
                   (le16_read((uint8_t*)&fileSize + 2) << 16);

        if (size) *size = fileSize;
        if (is_dir) *is_dir = (entry.DIR_Attr & FAT32_ATTR_DIRECTORY) != 0;
        if (mod_time) *mod_time = 0;  /* TODO: Convert FAT time to Mac time */

        serial_printf("[FAT32] get_file_info: cluster %llu (size=%u, is_dir=%d) from cache\n",
                      entry_id, fileSize, (entry.DIR_Attr & FAT32_ATTR_DIRECTORY) != 0);
        return true;
    }

    /* Not in cache - return minimal info */
    if (size) *size = 0;
    if (is_dir) *is_dir = false;
    if (mod_time) *mod_time = 0;

    serial_printf("[FAT32] get_file_info: cluster %llu (not in cache - limited info)\n", entry_id);
    return true;
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
    .get_file_info = fat32_get_file_info,
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
