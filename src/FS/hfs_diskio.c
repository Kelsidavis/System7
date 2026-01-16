/* HFS Disk I/O Implementation */
#include "../../include/FS/hfs_diskio.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#if !defined(__arm__) && !defined(__aarch64__) && !defined(HFS_DISABLE_ATA) && !defined(HFS_DISABLE_ATA)
#include "../../include/ATA_Driver.h"
#endif
#include "Platform/include/storage.h"
#include <string.h>
#include <stdio.h>
#include "FS/FSLogging.h"

#if defined(__powerpc__) || defined(__powerpc64__)
#define HFS_DISABLE_ATA 1
#endif

/* For now, we'll use memory-based implementation */
/* Later this can be extended to use real file I/O */

bool HFS_BD_InitMemory(HFS_BlockDev* bd, void* buffer, uint64_t size) {
    FS_LOG_DEBUG("HFS: BD_InitMemory: buffer=%p size=%d\n",
                 buffer, (int)size);

    if (!bd || !buffer || size == 0) return false;

    bd->type = HFS_BD_TYPE_MEMORY;
    bd->data = buffer;
    bd->device_index = -1;
    FS_LOG_DEBUG("HFS: BD_InitMemory: stored bd->data=%p\n", bd->data);
    bd->size = size;
    bd->sectorSize = 512;
    bd->readonly = false;
    return true;
}

bool HFS_BD_InitFile(HFS_BlockDev* bd, const char* path, bool readonly) {
    extern FILE* fopen(const char* filename, const char* mode);
    extern int fclose(FILE* stream);
    extern int fseek(FILE* stream, long offset, int whence);
    extern long ftell(FILE* stream);
    extern size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
    FILE* file = NULL;
    long fileSize;
    size_t bytesRead;

    if (!bd) return false;

    bd->type = HFS_BD_TYPE_FILE;
    bd->device_index = -1;

    /* Try to open the disk image file */
    if (path && path[0] != '\0') {
        file = fopen(path, readonly ? "rb" : "rb+");
    }

    if (file) {
        /* Get file size */
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return false;
        }

        fileSize = ftell(file);
        if (fileSize <= 0 || fileSize > 100 * 1024 * 1024) {  /* Cap at 100MB */
            fclose(file);
            return false;
        }

        /* Allocate memory for disk image */
        bd->size = (uint32_t)fileSize;
        bd->data = NewPtr(bd->size);
        if (!bd->data) {
            fclose(file);
            return false;
        }

        /* Read disk image into memory */
        if (fseek(file, 0, SEEK_SET) != 0) {
            DisposePtr((Ptr)bd->data);
            fclose(file);
            return false;
        }

        bytesRead = fread(bd->data, 1, bd->size, file);
        fclose(file);

        if (bytesRead != bd->size) {
            DisposePtr((Ptr)bd->data);
            return false;
        }

        bd->sectorSize = 512;
        bd->readonly = readonly;
        return true;
    }

    /* File doesn't exist or no path provided - create empty disk image */
    bd->size = 4 * 1024 * 1024;  /* Default 4MB */
    bd->data = NewPtr(bd->size);
    if (!bd->data) return false;

    memset(bd->data, 0, bd->size);
    bd->sectorSize = 512;
    bd->readonly = readonly;
    return true;
}

bool HFS_BD_InitATA(HFS_BlockDev* bd, int device_index, bool readonly) {
#if defined(__arm__) || defined(__aarch64__) || defined(HFS_DISABLE_ATA)
    (void)bd;
    (void)device_index;
    (void)readonly;
    FS_LOG_DEBUG("HFS: ATA not available on this platform\n");
    return false;
#else

    if (!bd) return false;

    /* Get ATA device */
    ATADevice* ata_dev = ATA_GetDevice(device_index);
    if (!ata_dev || !ata_dev->present) {
        FS_LOG_DEBUG("HFS: ATA device %d not found\n", device_index);
        return false;
    }

    /* Only support PATA hard disks for now */
    if (ata_dev->type != ATA_DEVICE_PATA) {
        FS_LOG_DEBUG("HFS: Device %d is not a PATA hard disk\n", device_index);
        return false;
    }

    FS_LOG_DEBUG("HFS: Initializing block device for ATA device %d\n", device_index);

    bd->type = HFS_BD_TYPE_ATA;
    bd->data = NULL;
    bd->device_index = device_index;
    bd->size = (uint64_t)ata_dev->sectors * 512;
    bd->sectorSize = 512;
    bd->readonly = readonly;

    FS_LOG_DEBUG("HFS: ATA block device initialized (size=%u MB)\n",
                 (uint32_t)(bd->size / (1024 * 1024)));

    return true;
#endif
}

bool HFS_BD_InitSDHCI(HFS_BlockDev* bd, int drive_index, bool readonly) {
    /* Initialize block device for SDHCI SD card (ARM/Raspberry Pi)
     * Uses HAL storage interface for cross-platform compatibility
     */
    #if defined(__arm__) || defined(__aarch64__) || defined(HFS_DISABLE_ATA)

    if (!bd) return false;

    /* Query drive info via HAL */
    hal_storage_info_t info = {0};
    OSErr err = hal_storage_get_drive_info(drive_index, &info);
    if (err != 0) {  /* noErr should be 0 */
        FS_LOG_DEBUG("HFS: SDHCI drive %d not found (err=%d)\n", drive_index, err);
        return false;
    }

    FS_LOG_DEBUG("HFS: Initializing SDHCI block device for drive %d\n", drive_index);

    bd->type = HFS_BD_TYPE_SDHCI;
    bd->data = NULL;
    bd->device_index = drive_index;
    bd->size = info.block_count * info.block_size;
    bd->sectorSize = info.block_size;
    bd->readonly = readonly;

    FS_LOG_DEBUG("HFS: SDHCI block device initialized (size=%u MB)\n",
                 (uint32_t)(bd->size / (1024 * 1024)));

    return true;
    #else
    /* SDHCI not supported on non-ARM platforms */
    FS_LOG_DEBUG("HFS: SDHCI not supported on this platform\n");
    return false;
    #endif
}

bool HFS_BD_Read(const HFS_BlockDev* bd, uint64_t offset, void* buffer, uint32_t length) {
    if (!bd || !buffer) return false;
    if (offset + length > bd->size) return false;

#if !defined(__arm__) && !defined(__aarch64__) && !defined(HFS_DISABLE_ATA)
    if (bd->type == HFS_BD_TYPE_ATA) {
        /* ATA device - read sectors */
        ATADevice* ata_dev = ATA_GetDevice(bd->device_index);
        if (!ata_dev) return false;

        /* Prevent division by zero */
        if (bd->sectorSize == 0) {
            return false;
        }

        /* Reject offsets that would overflow 32-bit sector math */
        if (offset + length > (uint64_t)UINT32_MAX * bd->sectorSize) {
            return false;  /* Offset too large */
        }

        /* Calculate sector alignment using 64-bit math, then truncate */
        uint32_t start_sector = (uint32_t)(offset / bd->sectorSize);
        uint32_t end_sector = (uint32_t)((offset + length + bd->sectorSize - 1) / bd->sectorSize);
        uint32_t sector_count = end_sector - start_sector;

        /* Allocate temporary buffer for sector-aligned read */
        /* Check for integer overflow in multiplication */
        if (sector_count > UINT32_MAX / bd->sectorSize) {
            return false;  /* Overflow would occur */
        }

        uint8_t* temp_buffer = NewPtr(sector_count * bd->sectorSize);
        if (!temp_buffer) return false;

        /* Read sectors */
        OSErr err = ATA_ReadSectors(ata_dev, start_sector, sector_count, temp_buffer);
        if (err != noErr) {
            DisposePtr((Ptr)temp_buffer);
            return false;
        }

        /* Copy requested data from temp buffer */
        uint32_t offset_in_sector = (uint32_t)offset % bd->sectorSize;
        memcpy(buffer, temp_buffer + offset_in_sector, length);

        DisposePtr((Ptr)temp_buffer);
        return true;
    } else
#endif
    if (bd->type == HFS_BD_TYPE_SDHCI) {
        /* SDHCI SD card - read blocks via HAL */
        #if defined(__arm__) || defined(__aarch64__) || defined(HFS_DISABLE_ATA)

        /* Prevent division by zero */
        if (bd->sectorSize == 0) {
            return false;
        }

        /* Reject offsets that would overflow 32-bit block math */
        if (offset + length > (uint64_t)UINT32_MAX * bd->sectorSize) {
            return false;  /* Offset too large */
        }

        /* Calculate block alignment using 64-bit math, then truncate */
        uint32_t start_block = (uint32_t)(offset / bd->sectorSize);
        uint32_t end_block = (uint32_t)((offset + length + bd->sectorSize - 1) / bd->sectorSize);
        uint32_t block_count = end_block - start_block;

        /* Allocate temporary buffer for block-aligned read */
        /* Check for integer overflow in multiplication */
        if (block_count > UINT32_MAX / bd->sectorSize) {
            return false;  /* Overflow would occur */
        }

        uint8_t* temp_buffer = NewPtr(block_count * bd->sectorSize);
        if (!temp_buffer) return false;

        /* Read blocks via HAL storage interface */
        OSErr err = hal_storage_read_blocks(bd->device_index, start_block, block_count, temp_buffer);
        if (err != 0) {  /* noErr should be 0 */
            DisposePtr((Ptr)temp_buffer);
            return false;
        }

        /* Copy requested data from temp buffer */
        uint32_t offset_in_block = (uint32_t)offset % bd->sectorSize;
        memcpy(buffer, temp_buffer + offset_in_block, length);

        DisposePtr((Ptr)temp_buffer);
        return true;
        #else
        return false;
        #endif
    } else {
        /* Memory or file-based device */
        if (!bd->data) return false;
        memcpy(buffer, (uint8_t*)bd->data + offset, length);
        return true;
    }
}

bool HFS_BD_Write(HFS_BlockDev* bd, uint64_t offset, const void* buffer, uint32_t length) {
    if (!bd || !buffer) return false;
    if (bd->readonly) return false;
    if (offset + length > bd->size) return false;

#if !defined(__arm__) && !defined(__aarch64__) && !defined(HFS_DISABLE_ATA)
    if (bd->type == HFS_BD_TYPE_ATA) {
        /* ATA device - write sectors */
        ATADevice* ata_dev = ATA_GetDevice(bd->device_index);
        if (!ata_dev) return false;

        /* Prevent division by zero */
        if (bd->sectorSize == 0) {
            return false;
        }

        /* Reject offsets that would overflow 32-bit sector math */
        if (offset + length > (uint64_t)UINT32_MAX * bd->sectorSize) {
            return false;  /* Offset too large */
        }

        /* Calculate sector alignment using 64-bit math, then truncate */
        uint32_t start_sector = (uint32_t)(offset / bd->sectorSize);
        uint32_t end_sector = (uint32_t)((offset + length + bd->sectorSize - 1) / bd->sectorSize);
        uint32_t sector_count = end_sector - start_sector;
        uint32_t offset_in_sector = (uint32_t)(offset % bd->sectorSize);

        /* Allocate temporary buffer for sector-aligned write */
        /* Check for integer overflow in multiplication */
        if (sector_count > UINT32_MAX / bd->sectorSize) {
            return false;  /* Overflow would occur */
        }

        uint8_t* temp_buffer = NewPtr(sector_count * bd->sectorSize);
        if (!temp_buffer) return false;

        /* If write doesn't start/end on sector boundary, need to read-modify-write */
        if (offset_in_sector != 0 || ((uint32_t)offset + length) % bd->sectorSize != 0) {
            /* Read existing sectors first */
            OSErr err = ATA_ReadSectors(ata_dev, start_sector, sector_count, temp_buffer);
            if (err != noErr) {
                DisposePtr((Ptr)temp_buffer);
                return false;
            }
        }

        /* Copy new data into temp buffer */
        memcpy(temp_buffer + offset_in_sector, buffer, length);

        /* Write sectors */
        OSErr err = ATA_WriteSectors(ata_dev, start_sector, sector_count, temp_buffer);
        DisposePtr((Ptr)temp_buffer);

        return (err == noErr);
    } else
#endif
    if (bd->type == HFS_BD_TYPE_SDHCI) {
        /* SDHCI SD card - write blocks via HAL */
        #if defined(__arm__) || defined(__aarch64__) || defined(HFS_DISABLE_ATA)

        /* Prevent division by zero */
        if (bd->sectorSize == 0) {
            return false;
        }

        /* Reject offsets that would overflow 32-bit block math */
        if (offset + length > (uint64_t)UINT32_MAX * bd->sectorSize) {
            return false;  /* Offset too large */
        }

        /* Calculate block alignment using 64-bit math, then truncate */
        uint32_t start_block = (uint32_t)(offset / bd->sectorSize);
        uint32_t end_block = (uint32_t)((offset + length + bd->sectorSize - 1) / bd->sectorSize);
        uint32_t block_count = end_block - start_block;
        uint32_t offset_in_block = (uint32_t)(offset % bd->sectorSize);

        /* Allocate temporary buffer for block-aligned write */
        /* Check for integer overflow in multiplication */
        if (block_count > UINT32_MAX / bd->sectorSize) {
            return false;  /* Overflow would occur */
        }

        uint8_t* temp_buffer = NewPtr(block_count * bd->sectorSize);
        if (!temp_buffer) return false;

        /* If write doesn't start/end on block boundary, need to read-modify-write */
        if (offset_in_block != 0 || ((uint32_t)offset + length) % bd->sectorSize != 0) {
            /* Read existing blocks first */
            OSErr err = hal_storage_read_blocks(bd->device_index, start_block, block_count, temp_buffer);
            if (err != 0) {  /* noErr should be 0 */
                DisposePtr((Ptr)temp_buffer);
                return false;
            }
        }

        /* Copy new data into temp buffer */
        memcpy(temp_buffer + offset_in_block, buffer, length);

        /* Write blocks via HAL storage interface */
        OSErr err = hal_storage_write_blocks(bd->device_index, start_block, block_count, temp_buffer);
        DisposePtr((Ptr)temp_buffer);

        return (err == 0);  /* noErr should be 0 */
        #else
        return false;
        #endif
    } else {
        /* Memory or file-based device */
        if (!bd->data) return false;
        memcpy((uint8_t*)bd->data + offset, buffer, length);
        return true;
    }
}

void HFS_BD_Close(HFS_BlockDev* bd) {
    if (!bd) return;

#if !defined(__arm__) && !defined(__aarch64__) && !defined(HFS_DISABLE_ATA)
    if (bd->type == HFS_BD_TYPE_ATA) {
        /* Flush ATA device cache before closing */
        ATADevice* ata_dev = ATA_GetDevice(bd->device_index);
        if (ata_dev && ata_dev->present) {
            ATA_FlushCache(ata_dev);
        }
        bd->device_index = -1;
    } else
#endif
    if (bd->type == HFS_BD_TYPE_SDHCI) {
        /* SDHCI devices don't need explicit flushing (SD protocol handles this) */
        bd->device_index = -1;
    } else if (bd->type == HFS_BD_TYPE_MEMORY || bd->type == HFS_BD_TYPE_FILE) {
        /* If we allocated memory, free it */
        if (bd->data) {
            DisposePtr((Ptr)bd->data);
            bd->data = NULL;
        }
    }

    bd->size = 0;
}

bool HFS_BD_ReadSector(HFS_BlockDev* bd, uint32_t sector, void* buffer) {
    if (!bd || !buffer) return false;

    uint64_t offset = (uint64_t)sector * bd->sectorSize;
    return HFS_BD_Read(bd, offset, buffer, bd->sectorSize);
}

bool HFS_BD_WriteSector(HFS_BlockDev* bd, uint32_t sector, const void* buffer) {
    if (!bd || !buffer || bd->readonly) return false;

    uint64_t offset = (uint64_t)sector * bd->sectorSize;
    return HFS_BD_Write(bd, offset, buffer, bd->sectorSize);
}

bool HFS_BD_Flush(HFS_BlockDev* bd) {
    if (!bd) return false;

#if !defined(__arm__) && !defined(__aarch64__) && !defined(HFS_DISABLE_ATA)
    if (bd->type == HFS_BD_TYPE_ATA) {
        /* Flush ATA device cache */
        ATADevice* ata_dev = ATA_GetDevice(bd->device_index);
        if (ata_dev && ata_dev->present) {
            OSErr err = ATA_FlushCache(ata_dev);
            return (err == noErr);
        }
        return false;
    } else
#endif
    if (bd->type == HFS_BD_TYPE_SDHCI) {
        /* SDHCI devices handle flushing internally */
        return true;
    }

    /* Memory/file devices don't need explicit flushing */
    return true;
}
