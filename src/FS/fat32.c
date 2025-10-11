/* FAT32 Filesystem Implementation */
#include "FS/fat32.h"
#include "System71StdLib.h"
#include <string.h>

#define FAT32_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleFileSystem, kLogLevelDebug, "[FAT32] " fmt, ##__VA_ARGS__)
#define FAT32_LOG_INFO(fmt, ...)  serial_logf(kLogModuleFileSystem, kLogLevelInfo,  "[FAT32] " fmt, ##__VA_ARGS__)
#define FAT32_LOG_WARN(fmt, ...)  serial_logf(kLogModuleFileSystem, kLogLevelWarn,  "[FAT32] " fmt, ##__VA_ARGS__)

/* Helper: Simple tolower implementation */
static inline int fat32_tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/* Helper: Case-insensitive string comparison */
static int fat32_stricmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = fat32_tolower((unsigned char)*s1);
        int c2 = fat32_tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return fat32_tolower((unsigned char)*s1) - fat32_tolower((unsigned char)*s2);
}

/* Helper: Convert little-endian values */
static inline uint16_t le16_to_cpu(uint16_t val) {
    return val;  /* x86 is little-endian */
}

static inline uint32_t le32_to_cpu(uint32_t val) {
    return val;  /* x86 is little-endian */
}

/* Helper: Check if cluster value indicates end of chain */
bool FAT32_IsEOC(uint32_t cluster) {
    return (cluster & FAT32_CLUSTER_MASK) >= FAT32_CLUSTER_EOC;
}

/* Helper: Check if cluster is valid */
bool FAT32_IsValidCluster(FAT32_Volume* vol, uint32_t cluster) {
    cluster &= FAT32_CLUSTER_MASK;
    return (cluster >= 2 && cluster < vol->totalClusters);
}

/* Convert FAT32 short name (8.3 format) to regular string */
void FAT32_ConvertShortName(const uint8_t* shortName, char* longName) {
    int i, j;

    /* Copy name part (first 8 chars) */
    for (i = 0; i < 8 && shortName[i] != ' '; i++) {
        longName[i] = shortName[i];
    }

    /* Check if there's an extension */
    if (shortName[8] != ' ') {
        longName[i++] = '.';
        for (j = 8; j < 11 && shortName[j] != ' '; j++) {
            longName[i++] = shortName[j];
        }
    }

    longName[i] = '\0';
}

/* Get sector number for a given cluster */
static uint32_t FAT32_ClusterToSector(FAT32_Volume* vol, uint32_t cluster) {
    return vol->dataStartSector + ((cluster - 2) * vol->boot.BPB_SecPerClus);
}

/* Read the next cluster number from FAT */
uint32_t FAT32_GetNextCluster(FAT32_Volume* vol, uint32_t cluster) {
    /* Calculate which FAT sector contains this cluster entry */
    uint32_t fatOffset = cluster * 4;  /* 4 bytes per entry in FAT32 */
    uint32_t fatSector = vol->fatStartSector + (fatOffset / FAT32_SECTOR_SIZE);
    uint32_t fatIndex = (fatOffset % FAT32_SECTOR_SIZE) / 4;

    /* Check if we need to load a different FAT sector */
    if (!vol->fatCacheValid || vol->fatCacheSector != fatSector) {
        if (!HFS_BD_ReadSector(&vol->bd, fatSector, vol->fatCache)) {
            FAT32_LOG_WARN("Failed to read FAT sector %u\n", fatSector);
            return FAT32_CLUSTER_EOC;
        }
        vol->fatCacheSector = fatSector;
        vol->fatCacheValid = true;
    }

    /* Return the cluster value, masked to 28 bits */
    return le32_to_cpu(vol->fatCache[fatIndex]) & FAT32_CLUSTER_MASK;
}

/* Read an entire cluster into buffer */
bool FAT32_ReadCluster(FAT32_Volume* vol, uint32_t cluster, void* buffer) {
    if (!FAT32_IsValidCluster(vol, cluster)) {
        FAT32_LOG_WARN("Invalid cluster %u\n", cluster);
        return false;
    }

    uint32_t sector = FAT32_ClusterToSector(vol, cluster);
    uint8_t* buf = (uint8_t*)buffer;

    /* Read all sectors in the cluster */
    for (uint32_t i = 0; i < vol->boot.BPB_SecPerClus; i++) {
        if (!HFS_BD_ReadSector(&vol->bd, sector + i, buf + (i * FAT32_SECTOR_SIZE))) {
            FAT32_LOG_WARN("Failed to read sector %u\n", sector + i);
            return false;
        }
    }

    return true;
}

/* Initialize FAT32 volume from ATA device */
bool FAT32_VolumeInit(FAT32_Volume* vol, int ata_device) {
    if (!vol) {
        FAT32_LOG_WARN("Null volume pointer\n");
        return false;
    }

    memset(vol, 0, sizeof(FAT32_Volume));

    /* Open block device */
    if (!HFS_BD_InitATA(&vol->bd, ata_device, false)) {
        FAT32_LOG_WARN("Failed to open ATA device %d\n", ata_device);
        return false;
    }

    /* Read boot sector */
    if (!HFS_BD_ReadSector(&vol->bd, 0, &vol->boot)) {
        FAT32_LOG_WARN("Failed to read boot sector\n");
        HFS_BD_Close(&vol->bd);
        return false;
    }

    /* Verify FAT32 signature */
    if (vol->boot.BS_BootSig != FAT32_BOOT_SIG) {
        FAT32_LOG_DEBUG("Invalid boot signature: 0x%02X (expected 0x%02X)\n",
                       vol->boot.BS_BootSig, FAT32_BOOT_SIG);
        HFS_BD_Close(&vol->bd);
        return false;
    }

    /* Verify it's FAT32 (not FAT16 or FAT12) */
    if (le16_to_cpu(vol->boot.BPB_FATSz16) != 0 ||
        le16_to_cpu(vol->boot.BPB_RootEntCnt) != 0) {
        FAT32_LOG_DEBUG("Not a FAT32 volume (appears to be FAT12/FAT16)\n");
        HFS_BD_Close(&vol->bd);
        return false;
    }

    /* Calculate important values */
    vol->fatStartSector = le16_to_cpu(vol->boot.BPB_RsvdSecCnt);
    uint32_t fatSize = le32_to_cpu(vol->boot.BPB_FATSz32);
    uint32_t rootDirSectors = 0;  /* FAT32 has no fixed root dir */
    uint32_t dataSectors = le32_to_cpu(vol->boot.BPB_TotSec32) -
                           (vol->fatStartSector +
                            (vol->boot.BPB_NumFATs * fatSize) +
                            rootDirSectors);

    vol->dataStartSector = vol->fatStartSector +
                           (vol->boot.BPB_NumFATs * fatSize);
    vol->rootDirCluster = le32_to_cpu(vol->boot.BPB_RootClus);
    vol->clusterSize = vol->boot.BPB_SecPerClus * FAT32_SECTOR_SIZE;
    vol->totalClusters = dataSectors / vol->boot.BPB_SecPerClus;

    /* Extract volume label */
    memcpy(vol->volumeLabel, vol->boot.BS_VolLab, 11);
    vol->volumeLabel[11] = '\0';
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && vol->volumeLabel[i] == ' '; i--) {
        vol->volumeLabel[i] = '\0';
    }

    vol->fatCacheValid = false;
    vol->mounted = true;

    FAT32_LOG_INFO("Mounted FAT32 volume: '%s'\n", vol->volumeLabel);
    FAT32_LOG_DEBUG("  Cluster size: %u bytes\n", vol->clusterSize);
    FAT32_LOG_DEBUG("  Total clusters: %u\n", vol->totalClusters);
    FAT32_LOG_DEBUG("  Root dir cluster: %u\n", vol->rootDirCluster);

    return true;
}

/* Unmount FAT32 volume */
void FAT32_VolumeUnmount(FAT32_Volume* vol) {
    if (!vol || !vol->mounted) return;

    HFS_BD_Close(&vol->bd);
    vol->mounted = false;
    FAT32_LOG_DEBUG("Volume unmounted\n");
}

/* Get volume information */
bool FAT32_GetVolumeInfo(FAT32_Volume* vol, VolumeControlBlock* vcb) {
    if (!vol || !vol->mounted || !vcb) return false;

    /* Fill in volume control block */
    strncpy(vcb->name, vol->volumeLabel, sizeof(vcb->name) - 1);
    vcb->name[sizeof(vcb->name) - 1] = '\0';

    vcb->totalBytes = (uint64_t)vol->totalClusters * vol->clusterSize;
    vcb->freeBytes = 0;  /* TODO: Calculate from FSInfo */
    vcb->rootID = vol->rootDirCluster;
    vcb->mounted = true;

    return true;
}

/* Read directory entries from a directory cluster */
bool FAT32_ReadDirEntries(FAT32_Volume* vol, uint32_t dirCluster,
                         CatEntry* entries, int maxEntries, int* count) {
    if (!vol || !vol->mounted || !entries || !count) {
        FAT32_LOG_WARN("Invalid parameters\n");
        return false;
    }

    *count = 0;
    uint32_t cluster = dirCluster;

    /* Allocate cluster buffer */
    uint8_t* clusterBuf = (uint8_t*)malloc(vol->clusterSize);
    if (!clusterBuf) {
        FAT32_LOG_WARN("Failed to allocate cluster buffer\n");
        return false;
    }

    /* Traverse the cluster chain for this directory */
    while (!FAT32_IsEOC(cluster) && *count < maxEntries) {
        if (!FAT32_ReadCluster(vol, cluster, clusterBuf)) {
            FAT32_LOG_WARN("Failed to read cluster %u\n", cluster);
            free(clusterBuf);
            return false;
        }

        /* Process all directory entries in this cluster */
        uint32_t entriesPerCluster = vol->clusterSize / sizeof(FAT32_DirEntry);
        FAT32_DirEntry* dirEntry = (FAT32_DirEntry*)clusterBuf;

        for (uint32_t i = 0; i < entriesPerCluster && *count < maxEntries; i++) {
            /* Check for end of directory */
            if (dirEntry[i].DIR_Name[0] == 0x00) {
                return true;  /* End of directory */
            }

            /* Skip deleted entries */
            if (dirEntry[i].DIR_Name[0] == 0xE5) {
                continue;
            }

            /* Skip LFN entries and volume labels */
            if ((dirEntry[i].DIR_Attr & FAT32_ATTR_LONG_NAME) == FAT32_ATTR_LONG_NAME ||
                (dirEntry[i].DIR_Attr & FAT32_ATTR_VOLUME_ID)) {
                continue;
            }

            /* Convert to CatEntry format */
            CatEntry* entry = &entries[*count];
            FAT32_ConvertShortName(dirEntry[i].DIR_Name, entry->name);

            /* Set kind */
            entry->kind = (dirEntry[i].DIR_Attr & FAT32_ATTR_DIRECTORY) ?
                         kNodeDir : kNodeFile;

            /* Get cluster number */
            uint32_t fileCluster = ((uint32_t)le16_to_cpu(dirEntry[i].DIR_FstClusHI) << 16) |
                                  le16_to_cpu(dirEntry[i].DIR_FstClusLO);
            entry->id = fileCluster;  /* Use cluster as ID */

            entry->size = le32_to_cpu(dirEntry[i].DIR_FileSize);
            entry->creator = 0;  /* FAT32 doesn't have creator/type */
            entry->type = 0;
            entry->flags = 0;
            entry->modTime = 0;  /* TODO: Convert FAT time to Mac time */
            entry->parent = dirCluster;

            (*count)++;
        }

        /* Get next cluster in chain */
        cluster = FAT32_GetNextCluster(vol, cluster);
    }

    free(clusterBuf);
    return true;
}

/* Lookup a file/directory by name in a directory */
bool FAT32_Lookup(FAT32_Volume* vol, uint32_t dirCluster, const char* name,
                 FAT32_DirEntry* entry, uint32_t* cluster) {
    if (!vol || !vol->mounted || !name || !entry || !cluster) {
        return false;
    }

    uint32_t currentCluster = dirCluster;

    /* Allocate cluster buffer */
    uint8_t* clusterBuf = (uint8_t*)malloc(vol->clusterSize);
    if (!clusterBuf) {
        FAT32_LOG_WARN("Failed to allocate cluster buffer\n");
        return false;
    }

    /* Traverse the cluster chain */
    while (!FAT32_IsEOC(currentCluster)) {
        if (!FAT32_ReadCluster(vol, currentCluster, clusterBuf)) {
            free(clusterBuf);
            return false;
        }

        uint32_t entriesPerCluster = vol->clusterSize / sizeof(FAT32_DirEntry);
        FAT32_DirEntry* dirEntry = (FAT32_DirEntry*)clusterBuf;

        for (uint32_t i = 0; i < entriesPerCluster; i++) {
            if (dirEntry[i].DIR_Name[0] == 0x00) {
                return false;  /* End of directory, not found */
            }

            if (dirEntry[i].DIR_Name[0] == 0xE5) {
                continue;  /* Deleted */
            }

            if ((dirEntry[i].DIR_Attr & FAT32_ATTR_LONG_NAME) == FAT32_ATTR_LONG_NAME ||
                (dirEntry[i].DIR_Attr & FAT32_ATTR_VOLUME_ID)) {
                continue;  /* Skip LFN and volume ID */
            }

            /* Convert name and compare */
            char entryName[13];
            FAT32_ConvertShortName(dirEntry[i].DIR_Name, entryName);

            if (fat32_stricmp(entryName, name) == 0) {
                /* Found it! */
                *entry = dirEntry[i];
                *cluster = ((uint32_t)le16_to_cpu(entry->DIR_FstClusHI) << 16) |
                          le16_to_cpu(entry->DIR_FstClusLO);
                free(clusterBuf);
                return true;
            }
        }

        currentCluster = FAT32_GetNextCluster(vol, currentCluster);
    }

    free(clusterBuf);
    return false;  /* Not found */
}

/* File operations */
FAT32_File* FAT32_FileOpen(FAT32_Volume* vol, uint32_t firstCluster, uint32_t fileSize) {
    if (!vol || !vol->mounted) {
        return NULL;
    }

    FAT32_File* file = (FAT32_File*)malloc(sizeof(FAT32_File));
    if (!file) {
        FAT32_LOG_WARN("Failed to allocate file structure\n");
        return NULL;
    }

    file->volume = vol;
    file->firstCluster = firstCluster;
    file->currentCluster = firstCluster;
    file->fileSize = fileSize;
    file->position = 0;
    file->clusterOffset = 0;

    return file;
}

void FAT32_FileClose(FAT32_File* file) {
    if (file) {
        free(file);
    }
}

bool FAT32_FileRead(FAT32_File* file, void* buffer, uint32_t length, uint32_t* bytesRead) {
    if (!file || !buffer || !bytesRead) {
        return false;
    }

    *bytesRead = 0;

    /* Don't read past end of file */
    if (file->position >= file->fileSize) {
        return true;  /* EOF */
    }

    if (file->position + length > file->fileSize) {
        length = file->fileSize - file->position;
    }

    uint8_t* dest = (uint8_t*)buffer;

    /* Allocate cluster buffer */
    uint8_t* clusterBuf = (uint8_t*)malloc(file->volume->clusterSize);
    if (!clusterBuf) {
        FAT32_LOG_WARN("Failed to allocate cluster buffer\n");
        return false;
    }

    while (length > 0 && !FAT32_IsEOC(file->currentCluster)) {
        /* Read current cluster */
        if (!FAT32_ReadCluster(file->volume, file->currentCluster, clusterBuf)) {
            free(clusterBuf);
            return false;
        }

        /* Copy data from cluster */
        uint32_t bytesInCluster = file->volume->clusterSize - file->clusterOffset;
        uint32_t bytesToCopy = (length < bytesInCluster) ? length : bytesInCluster;

        memcpy(dest, clusterBuf + file->clusterOffset, bytesToCopy);

        dest += bytesToCopy;
        length -= bytesToCopy;
        *bytesRead += bytesToCopy;
        file->position += bytesToCopy;
        file->clusterOffset += bytesToCopy;

        /* Move to next cluster if we've exhausted this one */
        if (file->clusterOffset >= file->volume->clusterSize) {
            file->currentCluster = FAT32_GetNextCluster(file->volume, file->currentCluster);
            file->clusterOffset = 0;
        }
    }

    free(clusterBuf);
    return true;
}

bool FAT32_FileSeek(FAT32_File* file, uint32_t position) {
    if (!file) return false;

    if (position > file->fileSize) {
        position = file->fileSize;
    }

    /* Reset to beginning */
    file->currentCluster = file->firstCluster;
    file->clusterOffset = 0;
    file->position = 0;

    /* Skip forward to the target position */
    uint32_t clusterSize = file->volume->clusterSize;
    while (file->position + clusterSize <= position) {
        file->currentCluster = FAT32_GetNextCluster(file->volume, file->currentCluster);
        if (FAT32_IsEOC(file->currentCluster)) {
            return false;
        }
        file->position += clusterSize;
    }

    /* Set offset within final cluster */
    file->clusterOffset = position - file->position;
    file->position = position;

    return true;
}

uint32_t FAT32_FileGetSize(FAT32_File* file) {
    return file ? file->fileSize : 0;
}

uint32_t FAT32_FileTell(FAT32_File* file) {
    return file ? file->position : 0;
}
