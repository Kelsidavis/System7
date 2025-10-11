/* System 7X Nanokernel - VFS Core Implementation
 *
 * This is the core VFS implementation that manages:
 * - Volume registry
 * - Filesystem driver registration
 * - Mount/unmount operations
 * - Block device abstraction
 */

#include "../../include/Nanokernel/vfs.h"
#include "../../include/Nanokernel/filesystem.h"
#include "../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Configuration */
#define VFS_MAX_FILESYSTEMS  8   /* Maximum registered filesystem types */
#define VFS_MAX_VOLUMES     16   /* Maximum mounted volumes */

/* VFS global state */
static struct {
    bool              initialized;
    FileSystemOps*    filesystems[VFS_MAX_FILESYSTEMS];
    int               fs_count;
    VFSVolume         volumes[VFS_MAX_VOLUMES];
    uint32_t          next_volume_id;
} g_vfs = { 0 };

/* Initialize the VFS subsystem */
bool MVFS_Initialize(void) {
    if (g_vfs.initialized) {
        serial_printf("[VFS] Already initialized\n");
        return true;
    }

    memset(&g_vfs, 0, sizeof(g_vfs));
    g_vfs.next_volume_id = 1;
    g_vfs.initialized = true;

    serial_printf("[VFS] Core initialized\n");
    return true;
}

/* Shutdown the VFS subsystem */
void MVFS_Shutdown(void) {
    if (!g_vfs.initialized) {
        return;
    }

    /* Unmount all volumes */
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (g_vfs.volumes[i].mounted) {
            MVFS_Unmount(&g_vfs.volumes[i]);
        }
    }

    g_vfs.initialized = false;
    serial_printf("[VFS] Shutdown complete\n");
}

/* Register a filesystem driver */
bool MVFS_RegisterFilesystem(FileSystemOps* fs_ops) {
    if (!g_vfs.initialized) {
        serial_printf("[VFS] ERROR: VFS not initialized\n");
        return false;
    }

    if (!fs_ops || !fs_ops->fs_name) {
        serial_printf("[VFS] ERROR: Invalid filesystem ops\n");
        return false;
    }

    if (g_vfs.fs_count >= VFS_MAX_FILESYSTEMS) {
        serial_printf("[VFS] ERROR: Too many filesystems registered\n");
        return false;
    }

    /* Check for duplicate registration */
    for (int i = 0; i < g_vfs.fs_count; i++) {
        if (strcmp(g_vfs.filesystems[i]->fs_name, fs_ops->fs_name) == 0) {
            serial_printf("[VFS] WARNING: Filesystem '%s' already registered\n", fs_ops->fs_name);
            return false;
        }
    }

    g_vfs.filesystems[g_vfs.fs_count++] = fs_ops;
    serial_printf("[VFS] Registered filesystem driver: %s (version %u)\n",
                  fs_ops->fs_name, fs_ops->fs_version);

    return true;
}

/* Find filesystem that can handle this block device */
static FileSystemOps* VFS_ProbeFilesystems(BlockDevice* dev) {
    for (int i = 0; i < g_vfs.fs_count; i++) {
        FileSystemOps* fs = g_vfs.filesystems[i];
        if (fs->probe && fs->probe(dev)) {
            serial_printf("[VFS] Filesystem '%s' detected on device\n", fs->fs_name);
            return fs;
        }
    }
    return NULL;
}

/* Find free volume slot */
static VFSVolume* VFS_AllocVolume(void) {
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (!g_vfs.volumes[i].mounted) {
            return &g_vfs.volumes[i];
        }
    }
    return NULL;
}

/* Mount a volume from a block device */
VFSVolume* MVFS_Mount(BlockDevice* dev, const char* volume_name) {
    if (!g_vfs.initialized) {
        serial_printf("[VFS] ERROR: VFS not initialized\n");
        return NULL;
    }

    if (!dev) {
        serial_printf("[VFS] ERROR: Invalid block device\n");
        return NULL;
    }

    /* Probe for filesystem type */
    FileSystemOps* fs_ops = VFS_ProbeFilesystems(dev);
    if (!fs_ops) {
        serial_printf("[VFS] ERROR: No filesystem detected on device\n");
        return NULL;
    }

    /* Allocate volume slot */
    VFSVolume* vol = VFS_AllocVolume();
    if (!vol) {
        serial_printf("[VFS] ERROR: No free volume slots\n");
        return NULL;
    }

    /* Initialize volume structure */
    memset(vol, 0, sizeof(VFSVolume));
    vol->volume_id = g_vfs.next_volume_id++;
    vol->fs_ops = fs_ops;
    vol->block_dev = dev;
    vol->read_only = false;

    if (volume_name) {
        strncpy(vol->name, volume_name, sizeof(vol->name) - 1);
        vol->name[sizeof(vol->name) - 1] = '\0';
    } else {
        snprintf(vol->name, sizeof(vol->name), "Volume_%u", vol->volume_id);
    }

    /* Call filesystem mount operation */
    if (fs_ops->mount) {
        vol->fs_private = fs_ops->mount(vol, dev);
        if (!vol->fs_private) {
            serial_printf("[VFS] ERROR: Filesystem mount failed\n");
            memset(vol, 0, sizeof(VFSVolume));
            return NULL;
        }
    }

    vol->mounted = true;

    serial_printf("[VFS] Mounted '%s' (type: %s, ID: %u)\n",
                  vol->name, fs_ops->fs_name, vol->volume_id);

    return vol;
}

/* Unmount a volume */
bool MVFS_Unmount(VFSVolume* vol) {
    if (!g_vfs.initialized || !vol || !vol->mounted) {
        return false;
    }

    serial_printf("[VFS] Unmounting '%s'\n", vol->name);

    /* Call filesystem unmount operation */
    if (vol->fs_ops && vol->fs_ops->unmount) {
        vol->fs_ops->unmount(vol);
    }

    /* Clean up volume structure */
    memset(vol, 0, sizeof(VFSVolume));

    serial_printf("[VFS] Unmount complete\n");
    return true;
}

/* List all mounted volumes */
void MVFS_ListVolumes(void) {
    if (!g_vfs.initialized) {
        serial_printf("[VFS] VFS not initialized\n");
        return;
    }

    serial_printf("[VFS] Mounted volumes:\n");
    bool found = false;

    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        VFSVolume* vol = &g_vfs.volumes[i];
        if (vol->mounted) {
            uint64_t total = 0, free = 0;
            if (vol->fs_ops && vol->fs_ops->get_stats) {
                vol->fs_ops->get_stats(vol, &total, &free);
            }

            serial_printf("  [%u] %s (%s) - %llu bytes (%llu free)%s\n",
                          vol->volume_id,
                          vol->name,
                          vol->fs_ops ? vol->fs_ops->fs_name : "unknown",
                          total,
                          free,
                          vol->read_only ? " [RO]" : "");
            found = true;
        }
    }

    if (!found) {
        serial_printf("  (no volumes mounted)\n");
    }
}

/* Get volume by ID */
VFSVolume* MVFS_GetVolumeByID(uint32_t volume_id) {
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (g_vfs.volumes[i].mounted && g_vfs.volumes[i].volume_id == volume_id) {
            return &g_vfs.volumes[i];
        }
    }
    return NULL;
}

/* Get volume by name */
VFSVolume* MVFS_GetVolumeByName(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (g_vfs.volumes[i].mounted &&
            strcmp(g_vfs.volumes[i].name, name) == 0) {
            return &g_vfs.volumes[i];
        }
    }
    return NULL;
}

/* === Block Device Helpers === */

/* Forward declarations for ATA types */
typedef struct ATADevice ATADevice;
typedef int16_t OSErr;
#define noErr 0

/* ATA block device private data */
typedef struct {
    ATADevice* device;
} ATABlockDevPrivate;

/* ATA read block */
static bool ata_read_block(BlockDevice* dev, uint64_t block, void* buffer) {
    ATABlockDevPrivate* priv = (ATABlockDevPrivate*)dev->private_data;
    extern OSErr ATA_ReadSectors(ATADevice* device, uint32_t lba, uint8_t count, void* buffer);
    return ATA_ReadSectors(priv->device, (uint32_t)block, 1, buffer) == noErr;
}

/* ATA write block */
static bool ata_write_block(BlockDevice* dev, uint64_t block, const void* buffer) {
    ATABlockDevPrivate* priv = (ATABlockDevPrivate*)dev->private_data;
    extern OSErr ATA_WriteSectors(ATADevice* device, uint32_t lba, uint8_t count, const void* buffer);
    return ATA_WriteSectors(priv->device, (uint32_t)block, 1, buffer) == noErr;
}

/* ATA flush */
static bool ata_flush(BlockDevice* dev) {
    (void)dev;
    /* ATA flush not yet implemented */
    return true;
}

/* Create ATA block device */
BlockDevice* MVFS_CreateATABlockDevice(int ata_device_index) {
    extern ATADevice* ATA_GetDevice(int index);

    ATADevice* ata_dev = ATA_GetDevice(ata_device_index);
    if (!ata_dev) {
        serial_printf("[VFS] ERROR: ATA device %d not found\n", ata_device_index);
        return NULL;
    }

    BlockDevice* dev = (BlockDevice*)malloc(sizeof(BlockDevice));
    if (!dev) {
        return NULL;
    }

    ATABlockDevPrivate* priv = (ATABlockDevPrivate*)malloc(sizeof(ATABlockDevPrivate));
    if (!priv) {
        free(dev);
        return NULL;
    }

    priv->device = ata_dev;

    dev->private_data = priv;
    dev->block_size = 512;
    dev->total_blocks = 0;  /* TODO: Get from ATA identify */
    dev->read_block = ata_read_block;
    dev->write_block = ata_write_block;
    dev->flush = ata_flush;

    serial_printf("[VFS] Created ATA block device (index %d)\n", ata_device_index);
    return dev;
}

/* Memory block device private data */
typedef struct {
    void*  buffer;
    size_t size;
} MemoryBlockDevPrivate;

/* Memory read block */
static bool mem_read_block(BlockDevice* dev, uint64_t block, void* buffer) {
    MemoryBlockDevPrivate* priv = (MemoryBlockDevPrivate*)dev->private_data;
    uint64_t offset = block * dev->block_size;

    if (offset + dev->block_size > priv->size) {
        return false;
    }

    memcpy(buffer, (uint8_t*)priv->buffer + offset, dev->block_size);
    return true;
}

/* Memory write block */
static bool mem_write_block(BlockDevice* dev, uint64_t block, const void* buffer) {
    MemoryBlockDevPrivate* priv = (MemoryBlockDevPrivate*)dev->private_data;
    uint64_t offset = block * dev->block_size;

    if (offset + dev->block_size > priv->size) {
        return false;
    }

    memcpy((uint8_t*)priv->buffer + offset, buffer, dev->block_size);
    return true;
}

/* Memory flush (no-op) */
static bool mem_flush(BlockDevice* dev) {
    (void)dev;
    return true;
}

/* Create memory block device */
BlockDevice* MVFS_CreateMemoryBlockDevice(void* buffer, size_t size) {
    if (!buffer || size == 0) {
        return NULL;
    }

    BlockDevice* dev = (BlockDevice*)malloc(sizeof(BlockDevice));
    if (!dev) {
        return NULL;
    }

    MemoryBlockDevPrivate* priv = (MemoryBlockDevPrivate*)malloc(sizeof(MemoryBlockDevPrivate));
    if (!priv) {
        free(dev);
        return NULL;
    }

    priv->buffer = buffer;
    priv->size = size;

    dev->private_data = priv;
    dev->block_size = 512;
    dev->total_blocks = size / 512;
    dev->read_block = mem_read_block;
    dev->write_block = mem_write_block;
    dev->flush = mem_flush;

    serial_printf("[VFS] Created memory block device (%zu bytes)\n", size);
    return dev;
}
