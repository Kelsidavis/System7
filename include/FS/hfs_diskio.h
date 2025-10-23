/* HFS Disk I/O Layer */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Block device types */
typedef enum {
    HFS_BD_TYPE_MEMORY = 0,    /* Memory-based (RAM disk) */
    HFS_BD_TYPE_FILE,          /* File-based (hosted) */
    HFS_BD_TYPE_ATA,           /* ATA/IDE disk */
    HFS_BD_TYPE_SDHCI          /* SDHCI SD card (ARM/Raspberry Pi) */
} HFS_BD_Type;

/* Block device abstraction */
typedef struct {
    HFS_BD_Type type;       /* Device type */
    void*    data;          /* Memory-mapped image or file handle (for MEMORY/FILE) */
    int      device_index;  /* Device index (ATA device for ATA, drive index for SDHCI) */
    uint64_t size;          /* Total size in bytes */
    uint32_t sectorSize;    /* Sector size (typically 512) */
    bool     readonly;      /* Read-only flag */
} HFS_BlockDev;

/* Initialize block device from memory buffer (for RAM disk) */
bool HFS_BD_InitMemory(HFS_BlockDev* bd, void* buffer, uint64_t size);

/* Initialize block device from file path */
bool HFS_BD_InitFile(HFS_BlockDev* bd, const char* path, bool readonly);

/* Initialize block device from ATA drive */
bool HFS_BD_InitATA(HFS_BlockDev* bd, int device_index, bool readonly);

/* Initialize block device from SDHCI SD card (ARM/Raspberry Pi) */
bool HFS_BD_InitSDHCI(HFS_BlockDev* bd, int drive_index, bool readonly);

/* Read from block device */
bool HFS_BD_Read(const HFS_BlockDev* bd, uint64_t offset, void* buffer, uint32_t length);

/* Write to block device (if not readonly) */
bool HFS_BD_Write(HFS_BlockDev* bd, uint64_t offset, const void* buffer, uint32_t length);

/* Close block device */
void HFS_BD_Close(HFS_BlockDev* bd);

/* Read a sector */
bool HFS_BD_ReadSector(HFS_BlockDev* bd, uint32_t sector, void* buffer);

/* Write a sector (if not readonly) */
bool HFS_BD_WriteSector(HFS_BlockDev* bd, uint32_t sector, const void* buffer);

/* Flush any cached writes */
bool HFS_BD_Flush(HFS_BlockDev* bd);