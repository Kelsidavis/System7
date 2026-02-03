#ifndef HAL_STORAGE_H
#define HAL_STORAGE_H

#include <stdint.h>
#include "MacTypes.h"

typedef struct {
    uint32_t block_size;
    uint64_t block_count;
} hal_storage_info_t;

OSErr hal_storage_init(void);
OSErr hal_storage_shutdown(void);

int hal_storage_get_drive_count(void);
int hal_storage_get_ata_count(void);
OSErr hal_storage_get_drive_info(int drive_index, hal_storage_info_t* info);

OSErr hal_storage_read_blocks(int drive_index, uint64_t start_block, uint32_t block_count, void* buffer);
OSErr hal_storage_write_blocks(int drive_index, uint64_t start_block, uint32_t block_count, const void* buffer);

#endif /* HAL_STORAGE_H */
