/* System 7X Nanokernel - Block Device Registry Implementation
 *
 * Provides a centralized registry for block devices discovered by
 * platform drivers. This allows the VFS autodetect system to enumerate
 * all available storage devices for filesystem detection.
 */

#include "../../include/Nanokernel/block.h"
#include "../../include/System71StdLib.h"
#include <string.h>

/* Global block device registry */
BlockRegistry g_block_registry = { 0 };

/* Initialize the block device registry */
void block_registry_init(void) {
    if (g_block_registry.initialized) {
        serial_printf("[BLOCK] Registry already initialized\n");
        return;
    }

    memset(&g_block_registry, 0, sizeof(BlockRegistry));
    g_block_registry.initialized = true;

    serial_printf("[BLOCK] Block device registry initialized\n");
}

/* Register a block device */
bool block_register(BlockDevice* dev, BlockDeviceType type, const char* name) {
    if (!g_block_registry.initialized) {
        block_registry_init();
    }

    if (!dev || !name) {
        serial_printf("[BLOCK] ERROR: Invalid parameters for registration\n");
        return false;
    }

    if (g_block_registry.count >= BLOCK_MAX_DEVICES) {
        serial_printf("[BLOCK] ERROR: Registry full (max %d devices)\n", BLOCK_MAX_DEVICES);
        return false;
    }

    /* Check for duplicate registration */
    for (size_t i = 0; i < g_block_registry.count; i++) {
        if (g_block_registry.devices[i].device == dev) {
            serial_printf("[BLOCK] WARNING: Device '%s' already registered\n", name);
            return false;
        }
    }

    /* Add device to registry */
    BlockDeviceEntry* entry = &g_block_registry.devices[g_block_registry.count];
    entry->device = dev;
    entry->type = type;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->total_size = dev->total_blocks * dev->block_size;
    entry->removable = false;  /* Default to non-removable */

    g_block_registry.count++;

    /* Convert type to string for logging */
    const char* type_str = "UNKNOWN";
    switch (type) {
        case BLOCK_TYPE_ATA:     type_str = "ATA"; break;
        case BLOCK_TYPE_MEMORY:  type_str = "MEMORY"; break;
        case BLOCK_TYPE_SCSI:    type_str = "SCSI"; break;
        case BLOCK_TYPE_USB:     type_str = "USB"; break;
        case BLOCK_TYPE_ISO:     type_str = "ISO"; break;
        case BLOCK_TYPE_VIRTUAL: type_str = "VIRTUAL"; break;
    }

    serial_printf("[BLOCK] Registered device: %s (type=%s, size=%llu bytes, block_size=%u)\n",
                  name, type_str, entry->total_size, dev->block_size);

    return true;
}

/* Enumerate registered block devices */
size_t block_enumerate(const BlockDeviceEntry** out, size_t max) {
    if (!out || max == 0) {
        return 0;
    }

    if (!g_block_registry.initialized) {
        return 0;
    }

    size_t count = g_block_registry.count < max ? g_block_registry.count : max;

    for (size_t i = 0; i < count; i++) {
        out[i] = &g_block_registry.devices[i];
    }

    return count;
}

/* Get block device by name */
const BlockDeviceEntry* block_get_by_name(const char* name) {
    if (!name || !g_block_registry.initialized) {
        return NULL;
    }

    for (size_t i = 0; i < g_block_registry.count; i++) {
        if (strcmp(g_block_registry.devices[i].name, name) == 0) {
            return &g_block_registry.devices[i];
        }
    }

    return NULL;
}

/* Get block device by index */
const BlockDeviceEntry* block_get_by_index(size_t index) {
    if (!g_block_registry.initialized || index >= g_block_registry.count) {
        return NULL;
    }

    return &g_block_registry.devices[index];
}

/* Get total number of registered devices */
size_t block_get_count(void) {
    return g_block_registry.initialized ? g_block_registry.count : 0;
}
