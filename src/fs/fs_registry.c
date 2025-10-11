/* Filesystem Driver Registry for System 7X VFS
 *
 * This module registers all available filesystem drivers with the VFS core.
 * It provides a single entry point for initializing all filesystem support.
 */

#include "../../include/Nanokernel/vfs.h"
#include "../../include/Nanokernel/filesystem.h"
#include "../../include/System71StdLib.h"

/* External filesystem driver initialization functions */
extern FileSystemOps* HFS_GetOps(void);
extern FileSystemOps* FAT32_GetOps(void);

/* Register all filesystem drivers */
void FS_RegisterFilesystems(void) {
    serial_printf("[FS Registry] Registering filesystem drivers...\n");

    /* Register HFS driver */
    FileSystemOps* hfs_ops = HFS_GetOps();
    if (hfs_ops) {
        if (MVFS_RegisterFilesystem(hfs_ops)) {
            serial_printf("[FS Registry] HFS driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register HFS driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: HFS driver not available\n");
    }

    /* Register FAT32 driver */
    FileSystemOps* fat32_ops = FAT32_GetOps();
    if (fat32_ops) {
        if (MVFS_RegisterFilesystem(fat32_ops)) {
            serial_printf("[FS Registry] FAT32 driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register FAT32 driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: FAT32 driver not available\n");
    }

    /* Future filesystem drivers can be registered here:
     * - ext2_GetOps()
     * - iso9660_GetOps()
     * - etc.
     */

    serial_printf("[FS Registry] Filesystem driver registration complete\n");
}
