/* System 7X Nanokernel - VFS Mount Table Implementation
 *
 * Unified mount table supporting local, network, and virtual filesystems.
 */

#include "../../../include/Nanokernel/vfs_mount.h"
#include "../../../include/System71StdLib.h"
#include <string.h>

/* Global mount table */
static struct {
    bool initialized;
    VFSMountEntry entries[VFS_MOUNT_MAX];
    int mount_count;
} g_mount_table = { 0 };

/* Initialize mount table */
bool VFSMount_Initialize(void) {
    if (g_mount_table.initialized) {
        serial_printf("[VFS-MOUNT] Already initialized\n");
        return true;
    }

    memset(&g_mount_table, 0, sizeof(g_mount_table));
    g_mount_table.initialized = true;

    serial_printf("[VFS-MOUNT] Mount table initialized\n");
    return true;
}

/* Add a mount entry */
bool VFSMount_Add(const char* source, const char* mount_point,
                  const char* fs_type, uint32_t flags, void* data) {
    if (!g_mount_table.initialized || !source || !mount_point || !fs_type) {
        return false;
    }

    /* Check for duplicate mount point */
    if (VFSMount_FindByMountPoint(mount_point)) {
        serial_printf("[VFS-MOUNT] ERROR: Mount point '%s' already in use\n",
                      mount_point);
        return false;
    }

    /* Find free slot */
    VFSMountEntry* entry = NULL;
    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!g_mount_table.entries[i].active) {
            entry = &g_mount_table.entries[i];
            break;
        }
    }

    if (!entry) {
        serial_printf("[VFS-MOUNT] ERROR: Mount table full\n");
        return false;
    }

    /* Initialize entry */
    memset(entry, 0, sizeof(VFSMountEntry));
    entry->active = true;
    strncpy(entry->mount_point, mount_point, sizeof(entry->mount_point) - 1);
    strncpy(entry->source, source, sizeof(entry->source) - 1);
    strncpy(entry->fs_type, fs_type, sizeof(entry->fs_type) - 1);
    entry->flags = flags;

    /* Store type-specific data */
    if (flags & VFS_MOUNT_NETWORK) {
        entry->net_mount = (VFSNetMount*)data;
    } else if (flags & VFS_MOUNT_VIRTUAL) {
        entry->vfs_ops = data;
    } else {
        entry->volume = (VFSVolume*)data;
    }

    g_mount_table.mount_count++;

    serial_printf("[VFS-MOUNT] Mounted '%s' on '%s' (type: %s, flags: 0x%x)\n",
                  source, mount_point, fs_type, flags);

    return true;
}

/* Remove a mount entry */
bool VFSMount_Remove(const char* mount_point) {
    if (!g_mount_table.initialized || !mount_point) {
        return false;
    }

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        VFSMountEntry* entry = &g_mount_table.entries[i];
        if (entry->active && strcmp(entry->mount_point, mount_point) == 0) {
            serial_printf("[VFS-MOUNT] Unmounting '%s'\n", mount_point);

            memset(entry, 0, sizeof(VFSMountEntry));
            g_mount_table.mount_count--;

            return true;
        }
    }

    serial_printf("[VFS-MOUNT] ERROR: Mount point '%s' not found\n", mount_point);
    return false;
}

/* Find mount by path (longest prefix match) */
VFSMountEntry* VFSMount_FindByPath(const char* path) {
    if (!g_mount_table.initialized || !path) {
        return NULL;
    }

    VFSMountEntry* best_match = NULL;
    size_t best_len = 0;

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        VFSMountEntry* entry = &g_mount_table.entries[i];
        if (!entry->active) continue;

        size_t mount_len = strlen(entry->mount_point);

        /* Check if path starts with mount point */
        if (strncmp(path, entry->mount_point, mount_len) == 0) {
            /* Ensure it's a proper prefix */
            if (path[mount_len] == '\0' || path[mount_len] == '/') {
                if (mount_len > best_len) {
                    best_match = entry;
                    best_len = mount_len;
                }
            }
        }
    }

    return best_match;
}

/* Find mount by mount point (exact match) */
VFSMountEntry* VFSMount_FindByMountPoint(const char* mount_point) {
    if (!g_mount_table.initialized || !mount_point) {
        return NULL;
    }

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        VFSMountEntry* entry = &g_mount_table.entries[i];
        if (entry->active && strcmp(entry->mount_point, mount_point) == 0) {
            return entry;
        }
    }

    return NULL;
}

/* List all mounts */
void VFSMount_ListAll(void) {
    serial_printf("[VFS-MOUNT] === Mount Table (/proc/mounts) ===\n");

    bool found = false;
    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        VFSMountEntry* entry = &g_mount_table.entries[i];
        if (entry->active) {
            const char* type_str = "local";
            if (entry->flags & VFS_MOUNT_NETWORK) {
                type_str = "network";
            } else if (entry->flags & VFS_MOUNT_VIRTUAL) {
                type_str = "virtual";
            }

            serial_printf("[VFS-MOUNT] %s on %s type %s (%s)%s\n",
                          entry->source,
                          entry->mount_point,
                          entry->fs_type,
                          type_str,
                          (entry->flags & VFS_MOUNT_RDONLY) ? " [RO]" : "");
            found = true;
        }
    }

    if (!found) {
        serial_printf("[VFS-MOUNT] (no mounts)\n");
    }

    serial_printf("[VFS-MOUNT] Total: %d mount(s)\n", g_mount_table.mount_count);
}

/* Get mount table statistics */
void VFSMount_GetStats(int* total, int* local, int* network, int* virtual) {
    int t = 0, l = 0, n = 0, v = 0;

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        VFSMountEntry* entry = &g_mount_table.entries[i];
        if (entry->active) {
            t++;
            if (entry->flags & VFS_MOUNT_NETWORK) {
                n++;
            } else if (entry->flags & VFS_MOUNT_VIRTUAL) {
                v++;
            } else {
                l++;
            }
        }
    }

    if (total) *total = t;
    if (local) *local = l;
    if (network) *network = n;
    if (virtual) *virtual = v;
}
