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
    BlockDevice* vfs_blockdev; /* VFS block device for reading */
    uint64_t     total_bytes;
    uint64_t     free_bytes;
    char         volume_name[32];
    bool         catalog_initialized;
} HFSPrivate;

/* Initialize HFS_BlockDev to wrap VFS BlockDevice */
static bool hfs_bd_init_vfs(HFS_BlockDev* bd, BlockDevice* vfs_dev) {
    if (!bd || !vfs_dev) return false;

    memset(bd, 0, sizeof(HFS_BlockDev));
    bd->type = HFS_BD_TYPE_VFS;  /* Use VFS block device type */
    bd->data = vfs_dev;  /* Store VFS BlockDevice pointer */
    bd->size = vfs_dev->total_blocks * vfs_dev->block_size;
    bd->sectorSize = vfs_dev->block_size;
    bd->readonly = false;
    bd->ata_device = -1;

    return true;
}

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
    uint8_t mdbBuffer[512];

    /* Allocate private data */
    HFSPrivate* priv = (HFSPrivate*)malloc(sizeof(HFSPrivate));
    if (!priv) {
        serial_printf("[HFS] Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(HFSPrivate));
    priv->vfs_blockdev = dev;

    /* Read MDB from sector 2 */
    if (!dev->read_block(dev, HFS_MDB_SECTOR, mdbBuffer)) {
        serial_printf("[HFS] Failed to read MDB\n");
        free(priv);
        return NULL;
    }

    /* Verify HFS signature */
    uint16_t sig = be16_read(&mdbBuffer[0]);
    if (sig != HFS_SIGNATURE) {
        serial_printf("[HFS] Invalid HFS signature: 0x%04x (expected 0x4244)\n", sig);
        free(priv);
        return NULL;
    }

    serial_printf("[HFS] Valid HFS signature detected\n");

    /* Initialize HFS_Volume structure manually by parsing MDB */
    HFS_Volume* hfs_vol = &priv->volume;
    memset(hfs_vol, 0, sizeof(HFS_Volume));

    /* Initialize wrapped block device */
    if (!hfs_bd_init_vfs(&hfs_vol->bd, dev)) {
        serial_printf("[HFS] Failed to init block device wrapper\n");
        free(priv);
        return NULL;
    }

    /* Parse MDB fields into volume structure */
    HFS_MDB* mdb = &hfs_vol->mdb;
    mdb->drSigWord = sig;
    mdb->drCrDate = be32_read(&mdbBuffer[4]);
    mdb->drLsMod = be32_read(&mdbBuffer[8]);
    mdb->drAtrb = be16_read(&mdbBuffer[12]);
    mdb->drNmFls = be16_read(&mdbBuffer[14]);
    mdb->drVBMSt = be16_read(&mdbBuffer[16]);
    mdb->drAllocPtr = be16_read(&mdbBuffer[18]);
    mdb->drNmAlBlks = be16_read(&mdbBuffer[20]);
    mdb->drAlBlkSiz = be32_read(&mdbBuffer[22]);
    mdb->drClpSiz = be32_read(&mdbBuffer[26]);
    mdb->drAlBlSt = be16_read(&mdbBuffer[30]);
    mdb->drNxtCNID = be32_read(&mdbBuffer[32]);
    mdb->drFreeBks = be16_read(&mdbBuffer[36]);

    /* Volume name */
    uint8_t name_len = mdbBuffer[38];
    if (name_len > 27) name_len = 27;
    memcpy(priv->volume_name, &mdbBuffer[39], name_len);
    priv->volume_name[name_len] = '\0';
    memcpy(hfs_vol->volName, priv->volume_name, sizeof(hfs_vol->volName));

    /* Catalog file extents */
    mdb->drCTFlSize = be32_read(&mdbBuffer[142]);
    for (int i = 0; i < 3; i++) {
        mdb->drCTExtRec[i].startBlock = be16_read(&mdbBuffer[146 + i * 4]);
        mdb->drCTExtRec[i].blockCount = be16_read(&mdbBuffer[148 + i * 4]);
    }

    /* Extents file */
    mdb->drXTFlSize = be32_read(&mdbBuffer[126]);
    for (int i = 0; i < 3; i++) {
        mdb->drXTExtRec[i].startBlock = be16_read(&mdbBuffer[130 + i * 4]);
        mdb->drXTExtRec[i].blockCount = be16_read(&mdbBuffer[132 + i * 4]);
    }

    /* Cache frequently used values in volume structure */
    hfs_vol->alBlkSize = mdb->drAlBlkSiz;
    hfs_vol->alBlSt = mdb->drAlBlSt;
    hfs_vol->vbmStart = mdb->drVBMSt;
    hfs_vol->numAlBlks = mdb->drNmAlBlks;
    hfs_vol->catFileSize = mdb->drCTFlSize;
    memcpy(hfs_vol->catExtents, mdb->drCTExtRec, sizeof(hfs_vol->catExtents));
    hfs_vol->extFileSize = mdb->drXTFlSize;
    memcpy(hfs_vol->extExtents, mdb->drXTExtRec, sizeof(hfs_vol->extExtents));
    hfs_vol->rootDirID = 2;  /* Standard HFS root CNID */
    hfs_vol->nextCNID = mdb->drNxtCNID;
    hfs_vol->mounted = true;
    hfs_vol->vRefNum = 0;  /* VFS layer manages volume IDs */

    priv->total_bytes = (uint64_t)mdb->drNmAlBlks * mdb->drAlBlkSiz;
    priv->free_bytes = (uint64_t)mdb->drFreeBks * mdb->drAlBlkSiz;

    /* Try to initialize catalog B-tree */
    if (HFS_CatalogInit(&priv->catalog, hfs_vol)) {
        priv->catalog_initialized = true;
        serial_printf("[HFS] Mounted '%s' - %llu bytes (%llu free)\n",
                      priv->volume_name, priv->total_bytes, priv->free_bytes);
        serial_printf("[HFS] Catalog B-tree initialized (%u blocks)\n",
                      hfs_vol->catExtents[0].blockCount);
    } else {
        serial_printf("[HFS] Mounted '%s' - %llu bytes (%llu free)\n",
                      priv->volume_name, priv->total_bytes, priv->free_bytes);
        serial_printf("[HFS] WARNING: Catalog B-tree initialization failed - read-only mode\n");
        priv->catalog_initialized = false;
    }

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
