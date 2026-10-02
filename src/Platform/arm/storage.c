/*
 * HAL Storage Implementation for ARM (Raspberry Pi)
 * Implements block device abstraction layer wrapping SDHCI SD card driver
 *
 * Supports:
 * - SD/eMMC cards via SDHCI controller
 * - Multiple drives (single drive by default)
 * - DMA-accelerated block transfers
 * - Standard POSIX-like block device interface
 */

#include <stdint.h>
#include <stddef.h>
#include "System71StdLib.h"
#include "Platform/include/storage.h"
#include "MacTypes.h"
#include "FileManagerTypes.h"

/* Forward declarations - SDHCI driver functions */
extern int sdhci_init(void);
extern int sdhci_read_blocks(uint32_t addr, uint32_t count, void *buffer);
extern int sdhci_write_blocks(uint32_t addr, uint32_t count, const void *buffer);
extern int sdhci_get_card_info(uint32_t *block_count);
extern int sdhci_card_present(void);
extern int sdhci_card_ready(void);
extern void sdhci_shutdown(void);

/* Drive info structure */
#define MAX_DRIVES 1

typedef struct {
    int initialized;
    uint32_t block_count;
    uint32_t block_size;
    int present;
} hal_drive_t;

static hal_drive_t drives[MAX_DRIVES] = {0};
static int storage_initialized = 0;

/*
 * Initialize storage subsystem
 * Detects and initializes all available block devices
 */
OSErr hal_storage_init(void) {
    Serial_WriteString("[Storage] Initializing storage subsystem\n");

    /* Initialize SDHCI driver */
    if (sdhci_init() != 0) {
        Serial_WriteString("[Storage] Warning: SDHCI initialization failed\n");
        /* Don't fail - storage still available if card inserted later */
    }

    /* Detect first drive (SD card) */
    drives[0].block_size = 512;  /* Standard SD block size */
    drives[0].block_count = 0;

    if (sdhci_card_present()) {
        if (sdhci_get_card_info(&drives[0].block_count) == 0) {
            drives[0].present = 1;
            Serial_Printf("[Storage] Drive 0: %lu blocks detected\n",
                          (unsigned long)drives[0].block_count);
        }
    }

    drives[0].initialized = 1;
    storage_initialized = 1;

    Serial_WriteString("[Storage] Storage initialization complete\n");
    return noErr;
}

/*
 * Shutdown storage subsystem
 */
OSErr hal_storage_shutdown(void) {
    Serial_WriteString("[Storage] Shutting down storage subsystem\n");

    sdhci_shutdown();

    storage_initialized = 0;
    return noErr;
}

/*
 * Get number of available drives
 */
int hal_storage_get_drive_count(void) {
    if (!storage_initialized) {
        return 0;
    }

    return MAX_DRIVES;
}

/*
 * Get information about a specific drive
 * drive_index: 0-based index of drive
 * info: Pointer to hal_storage_info_t structure to fill
 */
OSErr hal_storage_get_drive_info(int drive_index, hal_storage_info_t* info) {
    if (!storage_initialized || !info || drive_index < 0 || drive_index >= MAX_DRIVES) {
        return paramErr;
    }

    if (!drives[drive_index].initialized || !drives[drive_index].present) {
        return nsvErr;  /* nsvErr = -35: No such volume */
    }

    info->block_size = drives[drive_index].block_size;
    info->block_count = drives[drive_index].block_count;

    return noErr;
}

/*
 * Read blocks from a drive
 * drive_index: 0-based index of drive
 * start_block: Starting LBA (logical block address)
 * block_count: Number of 512-byte blocks to read
 * buffer: Destination buffer
 */
OSErr hal_storage_read_blocks(int drive_index, uint64_t start_block, uint32_t block_count, void* buffer) {
    if (!storage_initialized || !buffer) {
        return paramErr;
    }

    if (drive_index < 0 || drive_index >= MAX_DRIVES) {
        return paramErr;
    }

    if (!drives[drive_index].initialized || !drives[drive_index].present) {
        return nsvErr;  /* nsvErr = -35: No such volume */
    }

    /* Validate block range */
    if (start_block + block_count > drives[drive_index].block_count) {
        Serial_Printf("[Storage] Read error: Block range exceeds drive capacity\n");
        return paramErr;
    }

    Serial_Printf("[Storage] Reading blocks %llu-%llu from drive %d\n",
                  start_block, start_block + block_count - 1, drive_index);

    /* Call SDHCI read function */
    int result = sdhci_read_blocks((uint32_t)start_block, block_count, buffer);

    if (result < 0) {
        Serial_WriteString("[Storage] Read operation failed\n");
        return ioErr;
    }

    if (result != (int)block_count) {
        Serial_Printf("[Storage] Partial read: got %d blocks, expected %lu\n",
                      result, (unsigned long)block_count);
        return ioErr;
    }

    return noErr;
}

/*
 * Write blocks to a drive
 * drive_index: 0-based index of drive
 * start_block: Starting LBA (logical block address)
 * block_count: Number of 512-byte blocks to write
 * buffer: Source buffer
 */
OSErr hal_storage_write_blocks(int drive_index, uint64_t start_block, uint32_t block_count, const void* buffer) {
    if (!storage_initialized || !buffer) {
        return paramErr;
    }

    if (drive_index < 0 || drive_index >= MAX_DRIVES) {
        return paramErr;
    }

    if (!drives[drive_index].initialized || !drives[drive_index].present) {
        return nsvErr;  /* nsvErr = -35: No such volume */
    }

    /* Validate block range */
    if (start_block + block_count > drives[drive_index].block_count) {
        Serial_Printf("[Storage] Write error: Block range exceeds drive capacity\n");
        return paramErr;
    }

    Serial_Printf("[Storage] Writing blocks %llu-%llu to drive %d\n",
                  start_block, start_block + block_count - 1, drive_index);

    /* Call SDHCI write function */
    int result = sdhci_write_blocks((uint32_t)start_block, block_count, buffer);

    if (result < 0) {
        Serial_WriteString("[Storage] Write operation failed\n");
        return ioErr;
    }

    if (result != (int)block_count) {
        Serial_Printf("[Storage] Partial write: wrote %d blocks, expected %lu\n",
                      result, (unsigned long)block_count);
        return ioErr;
    }

    return noErr;
}
