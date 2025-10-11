/* System 7X Nanokernel - Virtual File System Core Interface
 *
 * This is the modular VFS layer that sits at the nanokernel level and provides
 * a clean abstraction for multiple filesystem backends (HFS, FAT32, ext2, ISO9660).
 *
 * The VFS provides:
 * - Volume registry and management
 * - Filesystem driver registration
 * - Mount/unmount operations
 * - Block device abstraction
 */

#ifndef NK_VFS_H
#define NK_VFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct BlockDevice BlockDevice;
typedef struct VFSVolume VFSVolume;
typedef struct FileSystemOps FileSystemOps;

/* Block device abstraction - represents physical storage */
typedef struct BlockDevice {
    void*    private_data;   /* Device-specific private data */
    uint64_t total_blocks;   /* Total number of blocks */
    uint32_t block_size;     /* Block size in bytes (typically 512) */

    /* Block I/O operations */
    bool (*read_block)(BlockDevice* dev, uint64_t block, void* buffer);
    bool (*write_block)(BlockDevice* dev, uint64_t block, const void* buffer);
    bool (*flush)(BlockDevice* dev);
} BlockDevice;

/* VFS Volume - represents a mounted filesystem instance */
struct VFSVolume {
    char              name[64];        /* Volume name */
    uint32_t          volume_id;       /* Unique volume ID */
    FileSystemOps*    fs_ops;          /* Filesystem driver operations */
    BlockDevice*      block_dev;       /* Underlying block device */
    void*             fs_private;      /* Filesystem-specific private data */
    bool              mounted;         /* Mount status */
    bool              read_only;       /* Read-only flag */
};

/* VFS Core API - using MVFS_ prefix to avoid conflicts with legacy VFS */

/* Initialize the VFS subsystem */
bool MVFS_Initialize(void);

/* Shutdown the VFS subsystem */
void MVFS_Shutdown(void);

/* Register a filesystem driver */
bool MVFS_RegisterFilesystem(FileSystemOps* fs_ops);

/* Mount a volume from a block device */
VFSVolume* MVFS_Mount(BlockDevice* dev, const char* volume_name);

/* Unmount a volume */
bool MVFS_Unmount(VFSVolume* vol);

/* List all mounted volumes */
void MVFS_ListVolumes(void);

/* Get volume by ID */
VFSVolume* MVFS_GetVolumeByID(uint32_t volume_id);

/* Get volume by name */
VFSVolume* MVFS_GetVolumeByName(const char* name);

/* Helper: Create ATA block device */
BlockDevice* MVFS_CreateATABlockDevice(int ata_device_index);

/* Helper: Create memory block device (for testing) */
BlockDevice* MVFS_CreateMemoryBlockDevice(void* buffer, size_t size);

/* VFS File Operations (Phase 6.4 - routed through daemons if available) */
bool MVFS_ReadFile(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                   void* buffer, size_t length, size_t* bytes_read);

bool MVFS_WriteFile(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                    const void* buffer, size_t length, size_t* bytes_written);

bool MVFS_Lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                 uint64_t* entry_id, bool* is_dir);

#endif /* NK_VFS_H */
