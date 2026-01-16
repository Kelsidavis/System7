/*
 * ARM64 Storage HAL Implementation
 * Storage access for ARM64 (VirtIO for QEMU, SDHCI for Raspberry Pi)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef QEMU_BUILD
#include "virtio_blk.h"
#else
#include "sdhci.h"
#endif

/* Drive info structure (matches Platform/include/storage.h) */
typedef struct {
    uint32_t sector_size;
    uint64_t total_sectors;
    bool     present;
    bool     writable;
    char     name[32];
} hal_drive_info_t;

/* Storage state */
static bool storage_initialized = false;

/* Helper to copy string */
static void copy_string(char *dest, const char *src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/*
 * Initialize storage subsystem
 */
void hal_storage_init(void) {
    if (storage_initialized) return;

#ifdef QEMU_BUILD
    /* Initialize VirtIO block device */
    virtio_blk_init();
#else
    /* Initialize SDHCI controller for Raspberry Pi */
    sdhci_init();
#endif

    storage_initialized = true;
}

/*
 * Get number of available drives
 */
int hal_storage_get_drive_count(void) {
#ifdef QEMU_BUILD
    if (virtio_blk_is_initialized()) {
        return 1;
    }
#else
    if (sdhci_is_initialized() && sdhci_card_present()) {
        return 1;
    }
#endif
    return 0;
}

/*
 * Get drive information
 * Returns 0 on success, -1 on failure
 */
int hal_storage_get_drive_info(int drive_num, hal_drive_info_t *info) {
    if (!info || drive_num != 0) {
        return -1;
    }

#ifdef QEMU_BUILD
    if (virtio_blk_is_initialized()) {
        info->present = true;
        info->sector_size = virtio_blk_get_sector_size();
        info->total_sectors = virtio_blk_get_capacity();
        info->writable = !virtio_blk_is_readonly();
        copy_string(info->name, "VirtIO Disk", sizeof(info->name));
        return 0;
    }
#else
    if (sdhci_is_initialized() && sdhci_card_present()) {
        info->present = true;
        info->sector_size = sdhci_get_sector_size();
        info->total_sectors = sdhci_get_capacity();
        info->writable = true;  /* SD cards are writable */
        copy_string(info->name, "SD Card", sizeof(info->name));
        return 0;
    }
#endif

    /* No storage device present */
    info->present = false;
    info->sector_size = 512;
    info->total_sectors = 0;
    info->writable = false;
    info->name[0] = '\0';

    return -1;
}

/*
 * Check if storage is ready
 */
bool hal_storage_is_ready(int drive_num) {
    if (drive_num != 0) return false;

#ifdef QEMU_BUILD
    return virtio_blk_is_initialized();
#else
    return sdhci_is_initialized() && sdhci_card_present();
#endif
}

/*
 * Read blocks from storage
 * Returns number of blocks read, or -1 on error
 */
int hal_storage_read_blocks(int drive_num, uint64_t start_block, uint32_t block_count, void *buffer) {
    if (drive_num != 0 || !buffer) {
        return -1;
    }

#ifdef QEMU_BUILD
    if (virtio_blk_is_initialized()) {
        return virtio_blk_read(start_block, block_count, buffer);
    }
#else
    if (sdhci_is_initialized() && sdhci_card_present()) {
        return sdhci_read_blocks(start_block, block_count, buffer);
    }
#endif

    return -1;
}

/*
 * Write blocks to storage
 * Returns number of blocks written, or -1 on error
 */
int hal_storage_write_blocks(int drive_num, uint64_t start_block, uint32_t block_count, const void *buffer) {
    if (drive_num != 0 || !buffer) {
        return -1;
    }

#ifdef QEMU_BUILD
    if (virtio_blk_is_initialized()) {
        return virtio_blk_write(start_block, block_count, buffer);
    }
#else
    if (sdhci_is_initialized() && sdhci_card_present()) {
        return sdhci_write_blocks(start_block, block_count, buffer);
    }
#endif

    return -1;
}
