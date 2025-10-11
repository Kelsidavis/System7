/* System 7X Nanokernel - VFS Mount Table Extensions
 *
 * Extended mount table supporting both local and network filesystems.
 * Integrates with VFS-Net for transparent remote access.
 */

#ifndef NANOKERNEL_VFS_MOUNT_H
#define NANOKERNEL_VFS_MOUNT_H

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"
#include "vfs_net.h"

#define VFS_MOUNT_MAX 32
#define VFS_MOUNT_RDONLY    (1 << 0)
#define VFS_MOUNT_NETWORK   (1 << 1)
#define VFS_MOUNT_VIRTUAL   (1 << 2)

/* Unified mount table entry */
typedef struct {
    bool active;
    char mount_point[256];
    char source[256];              /* Device or URL */
    char fs_type[32];              /* "hfs", "fat32", "webdav", "proc", etc. */
    uint32_t flags;                /* Mount flags */

    /* Local volume (if VFS_MOUNT_NETWORK not set) */
    VFSVolume* volume;

    /* Network mount (if VFS_MOUNT_NETWORK set) */
    VFSNetMount* net_mount;

    /* Virtual filesystem handler (if VFS_MOUNT_VIRTUAL set) */
    void* vfs_ops;
} VFSMountEntry;

/* Mount table API */

/* Initialize mount table */
bool VFSMount_Initialize(void);

/* Add a mount entry */
bool VFSMount_Add(const char* source, const char* mount_point,
                  const char* fs_type, uint32_t flags, void* data);

/* Remove a mount entry */
bool VFSMount_Remove(const char* mount_point);

/* Find mount by path (longest prefix match) */
VFSMountEntry* VFSMount_FindByPath(const char* path);

/* Find mount by mount point (exact match) */
VFSMountEntry* VFSMount_FindByMountPoint(const char* mount_point);

/* List all mounts (similar to /proc/mounts) */
void VFSMount_ListAll(void);

/* Get mount table statistics */
void VFSMount_GetStats(int* total, int* local, int* network, int* virtual);

#endif /* NANOKERNEL_VFS_MOUNT_H */
