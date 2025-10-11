/* FAT32 Filesystem Implementation */
#ifndef FAT32_H
#define FAT32_H

#include "fat32_types.h"
#include "hfs_types.h"  /* For CatEntry compatibility */
#include "hfs_diskio.h"  /* For BlockDevice */
#include <stdint.h>
#include <stdbool.h>

/* FAT32 Volume structure */
typedef struct {
    HFS_BlockDev     bd;              /* Block device */
    FAT32_BootSector boot;            /* Boot sector */

    /* Computed values from boot sector */
    uint32_t         fatStartSector;  /* First FAT sector */
    uint32_t         dataStartSector; /* First data sector */
    uint32_t         rootDirCluster;  /* Root directory cluster */
    uint32_t         clusterSize;     /* Cluster size in bytes */
    uint32_t         totalClusters;   /* Total clusters */

    /* FAT cache - we'll cache one FAT sector at a time */
    uint32_t         fatCacheSector;  /* Current cached FAT sector */
    uint32_t         fatCache[128];   /* 512 bytes = 128 uint32_t entries */
    bool             fatCacheValid;

    /* Volume info */
    char             volumeLabel[12];
    bool             mounted;
} FAT32_Volume;

/* FAT32 File handle */
typedef struct {
    FAT32_Volume*    volume;
    uint32_t         firstCluster;    /* First cluster of file */
    uint32_t         currentCluster;  /* Current cluster */
    uint32_t         fileSize;        /* Total file size */
    uint32_t         position;        /* Current position in file */
    uint32_t         clusterOffset;   /* Offset within current cluster */
} FAT32_File;

/* Initialize FAT32 volume from block device */
bool FAT32_VolumeInit(FAT32_Volume* vol, int ata_device);

/* Unmount FAT32 volume */
void FAT32_VolumeUnmount(FAT32_Volume* vol);

/* Get volume information */
bool FAT32_GetVolumeInfo(FAT32_Volume* vol, VolumeControlBlock* vcb);

/* Directory operations */
bool FAT32_ReadDirEntries(FAT32_Volume* vol, uint32_t dirCluster,
                         CatEntry* entries, int maxEntries, int* count);

/* Lookup a file/directory by name in a directory */
bool FAT32_Lookup(FAT32_Volume* vol, uint32_t dirCluster, const char* name,
                 FAT32_DirEntry* entry, uint32_t* cluster);

/* File operations */
FAT32_File* FAT32_FileOpen(FAT32_Volume* vol, uint32_t firstCluster, uint32_t fileSize);
void FAT32_FileClose(FAT32_File* file);
bool FAT32_FileRead(FAT32_File* file, void* buffer, uint32_t length, uint32_t* bytesRead);
bool FAT32_FileSeek(FAT32_File* file, uint32_t position);
uint32_t FAT32_FileGetSize(FAT32_File* file);
uint32_t FAT32_FileTell(FAT32_File* file);

/* Cluster chain operations */
uint32_t FAT32_GetNextCluster(FAT32_Volume* vol, uint32_t cluster);
bool FAT32_ReadCluster(FAT32_Volume* vol, uint32_t cluster, void* buffer);

/* Utility functions */
bool FAT32_IsValidCluster(FAT32_Volume* vol, uint32_t cluster);
bool FAT32_IsEOC(uint32_t cluster);
void FAT32_ConvertShortName(const uint8_t* shortName, char* longName);

#endif /* FAT32_H */
