/* Filesystem Driver Registry for System 7X VFS
 *
 * This module registers all available filesystem drivers with the VFS core.
 * It provides a single entry point for initializing all filesystem support.
 */

#include "../../include/Nanokernel/vfs.h"
#include "../../include/Nanokernel/filesystem.h"
#include "../../include/System71StdLib.h"

/* External filesystem driver initialization functions */
#ifdef ENABLE_FS_HFS
extern FileSystemOps* HFS_GetOps(void);
#endif

#ifdef ENABLE_FS_FAT32
extern FileSystemOps* FAT32_GetOps(void);
#endif

#ifdef ENABLE_FS_EXT4
extern FileSystemOps* EXT4_GetOps(void);
#endif

#ifdef ENABLE_FS_EXFAT
extern FileSystemOps* EXFAT_GetOps(void);
#endif

#ifdef ENABLE_FS_ISO9660
extern FileSystemOps* ISO9660_GetOps(void);
#endif

#ifdef ENABLE_FS_UDF
extern FileSystemOps* UDF_GetOps(void);
#endif

/* Register all filesystem drivers */
void FS_RegisterFilesystems(void) {
    serial_printf("[FS Registry] Registering filesystem drivers...\n");

#ifdef ENABLE_FS_HFS
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
#endif

#ifdef ENABLE_FS_FAT32
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
#endif

#ifdef ENABLE_FS_EXT4
    /* Register ext4 driver */
    FileSystemOps* ext4_ops = EXT4_GetOps();
    if (ext4_ops) {
        if (MVFS_RegisterFilesystem(ext4_ops)) {
            serial_printf("[FS Registry] ext4 driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register ext4 driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: ext4 driver not available\n");
    }
#endif

#ifdef ENABLE_FS_EXFAT
    /* Register exFAT driver */
    FileSystemOps* exfat_ops = EXFAT_GetOps();
    if (exfat_ops) {
        if (MVFS_RegisterFilesystem(exfat_ops)) {
            serial_printf("[FS Registry] exFAT driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register exFAT driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: exFAT driver not available\n");
    }
#endif

#ifdef ENABLE_FS_ISO9660
    /* Register ISO 9660 driver */
    FileSystemOps* iso9660_ops = ISO9660_GetOps();
    if (iso9660_ops) {
        if (MVFS_RegisterFilesystem(iso9660_ops)) {
            serial_printf("[FS Registry] ISO 9660 driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register ISO 9660 driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: ISO 9660 driver not available\n");
    }
#endif

#ifdef ENABLE_FS_UDF
    /* Register UDF driver */
    FileSystemOps* udf_ops = UDF_GetOps();
    if (udf_ops) {
        if (MVFS_RegisterFilesystem(udf_ops)) {
            serial_printf("[FS Registry] UDF driver registered successfully\n");
        } else {
            serial_printf("[FS Registry] WARNING: Failed to register UDF driver\n");
        }
    } else {
        serial_printf("[FS Registry] ERROR: UDF driver not available\n");
    }
#endif

    /* Future filesystem drivers can be registered here:
     * - ntfs_GetOps()
     * - ufs_GetOps()
     * - etc.
     */

    serial_printf("[FS Registry] Filesystem driver registration complete\n");
}
