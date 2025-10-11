/* HFS Volume Management */
#pragma once
#include "hfs_types.h"
#include "hfs_diskio.h"
#include <stdbool.h>

/* HFS Volume structure */
typedef struct {
    HFS_BlockDev bd;          /* Block device */
    HFS_MDB     mdb;          /* Master Directory Block */

    /* Cached volume parameters */
    uint32_t    alBlkSize;    /* Allocation block size in bytes */
    uint16_t    alBlSt;       /* First allocation block number */
    uint16_t    vbmStart;     /* Volume bitmap start block */
    uint16_t    numAlBlks;    /* Total allocation blocks */

    /* Catalog B-tree info */
    uint32_t    catFileSize;  /* Catalog file size */
    HFS_Extent  catExtents[3];/* First 3 catalog extents */
    uint16_t    catNodeSize;  /* Catalog node size */
    uint32_t    catRootNode;  /* Catalog root node */

    /* Extents B-tree info */
    uint32_t    extFileSize;  /* Extents file size */
    HFS_Extent  extExtents[3];/* First 3 extents extents */
    uint16_t    extNodeSize;  /* Extents node size */
    uint32_t    extRootNode;  /* Extents root node */

    /* Volume info */
    char        volName[28];  /* Volume name */
    uint32_t    rootDirID;    /* Root directory CNID */
    uint32_t    nextCNID;     /* Next available CNID */
    bool        mounted;      /* Is volume mounted */
    VRefNum     vRefNum;      /* Volume reference number */
} HFS_Volume;

/* Mount an HFS volume from a disk image */
bool HFS_VolumeMount(HFS_Volume* vol, const char* imagePath, VRefNum vRefNum);

/* Mount an HFS volume from memory */
bool HFS_VolumeMountMemory(HFS_Volume* vol, void* buffer, uint64_t size, VRefNum vRefNum);

/* Unmount an HFS volume */
void HFS_VolumeUnmount(HFS_Volume* vol);

/* Convert allocation block number to byte offset in image */
uint64_t HFS_AllocBlockToByteOffset(const HFS_Volume* vol, uint32_t allocBlock);

/* Read allocation blocks */
bool HFS_ReadAllocBlocks(const HFS_Volume* vol, uint32_t startBlock, uint32_t blockCount,
                         void* buffer);

/* Write allocation blocks */
bool HFS_WriteAllocBlocks(HFS_Volume* vol, uint32_t startBlock, uint32_t blockCount,
                          const void* buffer);

/* Get volume info */
bool HFS_GetVolumeInfo(const HFS_Volume* vol, VolumeControlBlock* vcb);

/* Create a blank HFS volume in memory (for initial testing) */
bool HFS_CreateBlankVolume(void* buffer, uint64_t size, const char* volName);

/* Format a block device with HFS filesystem */
bool HFS_FormatVolume(HFS_BlockDev* bd, const char* volName);