/* System 7X Nanokernel - Block Device Registry
 *
 * This module provides a centralized registry for block devices discovered
 * by platform drivers (ATA, RAM disk, ISO, SCSI, etc.). The VFS autodetect
 * system enumerates these devices to automatically detect and mount filesystems.
 */

#ifndef NK_BLOCK_H
#define NK_BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "vfs.h"  /* For BlockDevice definition */

/* Maximum number of block devices that can be registered */
#define BLOCK_MAX_DEVICES 16

/* Block device type identifiers */
typedef enum {
    BLOCK_TYPE_ATA,
    BLOCK_TYPE_MEMORY,
    BLOCK_TYPE_SCSI,
    BLOCK_TYPE_USB,
    BLOCK_TYPE_ISO,
    BLOCK_TYPE_VIRTUAL
} BlockDeviceType;

/* Block device metadata */
typedef struct {
    BlockDevice*    device;        /* Pointer to BlockDevice structure */
    BlockDeviceType type;          /* Device type */
    char            name[32];      /* Device name (e.g., "ata0", "ram0") */
    uint64_t        total_size;    /* Total size in bytes */
    bool            removable;     /* Removable media flag */
} BlockDeviceEntry;

/* Block device registry */
typedef struct {
    BlockDeviceEntry devices[BLOCK_MAX_DEVICES];
    size_t           count;
    bool             initialized;
} BlockRegistry;

/* Global block device registry */
extern BlockRegistry g_block_registry;

/* Initialize the block device registry */
void block_registry_init(void);

/* Register a block device
 * Returns: true on success, false if registry is full
 */
bool block_register(BlockDevice* dev, BlockDeviceType type, const char* name);

/* Enumerate registered block devices
 * Parameters:
 *   out     - Output array for BlockDeviceEntry pointers
 *   max     - Maximum entries to return
 * Returns: Number of devices enumerated
 */
size_t block_enumerate(const BlockDeviceEntry** out, size_t max);

/* Get block device by name
 * Returns: BlockDeviceEntry pointer or NULL if not found
 */
const BlockDeviceEntry* block_get_by_name(const char* name);

/* Get block device by index
 * Returns: BlockDeviceEntry pointer or NULL if invalid index
 */
const BlockDeviceEntry* block_get_by_index(size_t index);

/* Get total number of registered devices */
size_t block_get_count(void);

#endif /* NK_BLOCK_H */
