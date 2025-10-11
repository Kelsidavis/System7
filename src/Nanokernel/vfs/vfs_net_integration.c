/* System 7X Nanokernel - VFS-Net Integration with Path Resolution
 *
 * Extends path resolution to support network filesystem mounts.
 */

#include "../../../include/Nanokernel/vfs_net.h"
#include "../../../include/Nanokernel/vfs_mount.h"
#include "../../../include/Nanokernel/vfs_path.h"
#include "../../../include/System71StdLib.h"
#include <string.h>

/* Check if path is under /net/ prefix (explicit network access) */
bool VFSNet_IsNetPath(const char* path) {
    return (path && strncmp(path, "/net/", 5) == 0);
}

/* Resolve network path to mount entry
 * Checks if path is under any network mount point
 */
VFSNetMount* VFSNet_ResolveNetPath(const char* path, char* rel_path, size_t rel_len) {
    if (!path || !rel_path) {
        return NULL;
    }

    /* Check mount table for network mounts */
    VFSMountEntry* mount_entry = VFSMount_FindByPath(path);
    if (!mount_entry || !(mount_entry->flags & VFS_MOUNT_NETWORK)) {
        return NULL;
    }

    /* Calculate relative path within mount */
    size_t mount_len = strlen(mount_entry->mount_point);
    const char* remainder = path + mount_len;

    /* Skip leading slash */
    while (*remainder == '/') {
        remainder++;
    }

    strncpy(rel_path, remainder, rel_len - 1);
    rel_path[rel_len - 1] = '\0';

    serial_printf("[VFS-NET] Resolved network path: '%s' → mount='%s', rel='%s'\n",
                  path, mount_entry->mount_point, rel_path);

    return mount_entry->net_mount;
}

/* Initialize VFS-Net integration */
void VFSNet_IntegrationInit(void) {
    serial_printf("[VFS-NET] Integration layer initialized\n");

    /* Mount /net as virtual root for explicit network access */
    /* In real implementation, /net would be a synthetic directory */
}
