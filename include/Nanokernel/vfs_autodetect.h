/* System 7X Nanokernel - VFS Autodetect and Auto-Mount
 *
 * This module provides automatic filesystem detection and mounting
 * at boot time. It enumerates registered block devices, probes for
 * filesystems, and automatically mounts detected volumes.
 *
 * Features:
 * - MBR partition table parsing
 * - Automatic filesystem probing (HFS, FAT32, ext2, ISO9660, etc.)
 * - Raw device mounting (no partition table)
 * - Volume naming and labeling
 */

#ifndef NK_VFS_AUTODETECT_H
#define NK_VFS_AUTODETECT_H

#include <stdint.h>
#include <stdbool.h>

/* MBR Partition Table Entry */
typedef struct {
    uint8_t  boot_flag;        /* 0x80 = bootable, 0x00 = not bootable */
    uint8_t  start_chs[3];     /* Starting CHS address (legacy) */
    uint8_t  type;             /* Partition type code */
    uint8_t  end_chs[3];       /* Ending CHS address (legacy) */
    uint32_t start_lba;        /* Starting LBA */
    uint32_t num_sectors;      /* Number of sectors */
} __attribute__((packed)) MBRPartition;

/* MBR (Master Boot Record) Structure */
typedef struct {
    uint8_t        boot_code[446];   /* Boot loader code */
    MBRPartition   partitions[4];    /* Partition table entries */
    uint16_t       signature;        /* 0xAA55 boot signature */
} __attribute__((packed)) MBR;

/* MBR signature */
#define MBR_SIGNATURE 0xAA55

/* Common partition type codes */
#define PART_TYPE_EMPTY     0x00
#define PART_TYPE_FAT16     0x06
#define PART_TYPE_NTFS      0x07
#define PART_TYPE_FAT32     0x0B
#define PART_TYPE_FAT32_LBA 0x0C
#define PART_TYPE_FAT16_LBA 0x0E
#define PART_TYPE_EXT       0x05
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_HFS       0xAF

/* Initialize autodetect subsystem */
void vfs_autodetect_init(void);

/* Main auto-mount function
 * Enumerates all registered block devices and attempts to:
 * 1. Read MBR and parse partition table
 * 2. Probe each partition with all registered filesystems
 * 3. Mount detected filesystems automatically
 */
void vfs_autodetect_mount(void);

/* Probe a specific block device for filesystems
 * Returns: true if a filesystem was detected and mounted
 */
bool vfs_probe_device(const char* device_name);

/* Read and parse MBR from a block device
 * Returns: true if valid MBR found
 */
bool vfs_read_mbr(void* block_device, MBR* mbr);

/* Check if MBR partition entry is valid
 * Returns: true if partition is non-empty and valid
 */
bool vfs_is_partition_valid(const MBRPartition* part);

#endif /* NK_VFS_AUTODETECT_H */
