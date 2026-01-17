/*
 * ARM64 Storage HAL Interface
 * Storage access for ARM64 platforms
 */

#ifndef ARM64_STORAGE_H
#define ARM64_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

/* Drive info structure */
typedef struct {
    uint32_t sector_size;
    uint64_t total_sectors;
    bool     present;
    bool     writable;
    char     name[32];
} hal_drive_info_t;

/* Initialize storage subsystem */
void hal_storage_init(void);

/* Get number of available drives */
int hal_storage_get_drive_count(void);

/* Get drive information */
int hal_storage_get_drive_info(int drive, hal_drive_info_t *info);

/* Check if storage is ready */
bool hal_storage_is_ready(int drive);

/* Read blocks from storage */
int hal_storage_read_blocks(int drive, uint64_t start, uint32_t count, void *buffer);

/* Write blocks to storage */
int hal_storage_write_blocks(int drive, uint64_t start, uint32_t count, const void *buffer);

#endif /* ARM64_STORAGE_H */
