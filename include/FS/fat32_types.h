/* FAT32 Filesystem Types and Structures */
#ifndef FAT32_TYPES_H
#define FAT32_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* FAT32 Boot Sector / BIOS Parameter Block */
#pragma pack(push, 1)
typedef struct {
    uint8_t  BS_jmpBoot[3];       /* Jump instruction */
    uint8_t  BS_OEMName[8];       /* OEM name */
    uint16_t BPB_BytsPerSec;      /* Bytes per sector (usually 512) */
    uint8_t  BPB_SecPerClus;      /* Sectors per cluster */
    uint16_t BPB_RsvdSecCnt;      /* Reserved sector count */
    uint8_t  BPB_NumFATs;         /* Number of FATs (usually 2) */
    uint16_t BPB_RootEntCnt;      /* Root entry count (0 for FAT32) */
    uint16_t BPB_TotSec16;        /* Total sectors (0 for FAT32) */
    uint8_t  BPB_Media;           /* Media descriptor */
    uint16_t BPB_FATSz16;         /* FAT size in sectors (0 for FAT32) */
    uint16_t BPB_SecPerTrk;       /* Sectors per track */
    uint16_t BPB_NumHeads;        /* Number of heads */
    uint32_t BPB_HiddSec;         /* Hidden sectors */
    uint32_t BPB_TotSec32;        /* Total sectors (FAT32) */

    /* FAT32 specific fields */
    uint32_t BPB_FATSz32;         /* FAT size in sectors */
    uint16_t BPB_ExtFlags;        /* Extended flags */
    uint16_t BPB_FSVer;           /* Filesystem version */
    uint32_t BPB_RootClus;        /* Root directory cluster */
    uint16_t BPB_FSInfo;          /* FSInfo sector */
    uint16_t BPB_BkBootSec;       /* Backup boot sector */
    uint8_t  BPB_Reserved[12];    /* Reserved */
    uint8_t  BS_DrvNum;           /* Drive number */
    uint8_t  BS_Reserved1;        /* Reserved */
    uint8_t  BS_BootSig;          /* Boot signature (0x29) */
    uint32_t BS_VolID;            /* Volume ID */
    uint8_t  BS_VolLab[11];       /* Volume label */
    uint8_t  BS_FilSysType[8];    /* Filesystem type ("FAT32   ") */
} FAT32_BootSector;
#pragma pack(pop)

/* FAT32 FSInfo structure */
#pragma pack(push, 1)
typedef struct {
    uint32_t FSI_LeadSig;         /* Lead signature (0x41615252) */
    uint8_t  FSI_Reserved1[480];  /* Reserved */
    uint32_t FSI_StrucSig;        /* Structure signature (0x61417272) */
    uint32_t FSI_Free_Count;      /* Free cluster count */
    uint32_t FSI_Nxt_Free;        /* Next free cluster */
    uint8_t  FSI_Reserved2[12];   /* Reserved */
    uint32_t FSI_TrailSig;        /* Trail signature (0xAA550000) */
} FAT32_FSInfo;
#pragma pack(pop)

/* FAT32 Directory Entry (32 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  DIR_Name[11];        /* Short name (8.3 format) */
    uint8_t  DIR_Attr;            /* File attributes */
    uint8_t  DIR_NTRes;           /* Reserved for Windows NT */
    uint8_t  DIR_CrtTimeTenth;    /* Creation time (tenths of second) */
    uint16_t DIR_CrtTime;         /* Creation time */
    uint16_t DIR_CrtDate;         /* Creation date */
    uint16_t DIR_LstAccDate;      /* Last access date */
    uint16_t DIR_FstClusHI;       /* High word of first cluster */
    uint16_t DIR_WrtTime;         /* Write time */
    uint16_t DIR_WrtDate;         /* Write date */
    uint16_t DIR_FstClusLO;       /* Low word of first cluster */
    uint32_t DIR_FileSize;        /* File size in bytes */
} FAT32_DirEntry;
#pragma pack(pop)

/* Long File Name (LFN) Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  LDIR_Ord;            /* Order/sequence number */
    uint16_t LDIR_Name1[5];       /* First 5 characters (Unicode) */
    uint8_t  LDIR_Attr;           /* Attributes (always 0x0F) */
    uint8_t  LDIR_Type;           /* Type (always 0) */
    uint8_t  LDIR_Chksum;         /* Checksum */
    uint16_t LDIR_Name2[6];       /* Next 6 characters (Unicode) */
    uint16_t LDIR_FstClusLO;      /* Always 0 */
    uint16_t LDIR_Name3[2];       /* Last 2 characters (Unicode) */
} FAT32_LFNEntry;
#pragma pack(pop)

/* File Attributes */
#define FAT32_ATTR_READ_ONLY    0x01
#define FAT32_ATTR_HIDDEN       0x02
#define FAT32_ATTR_SYSTEM       0x04
#define FAT32_ATTR_VOLUME_ID    0x08
#define FAT32_ATTR_DIRECTORY    0x10
#define FAT32_ATTR_ARCHIVE      0x20
#define FAT32_ATTR_LONG_NAME    (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | \
                                 FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)

/* Special cluster values */
#define FAT32_CLUSTER_FREE      0x00000000
#define FAT32_CLUSTER_RESERVED  0x0FFFFFF0
#define FAT32_CLUSTER_BAD       0x0FFFFFF7
#define FAT32_CLUSTER_EOC       0x0FFFFFF8  /* End of chain */
#define FAT32_CLUSTER_MASK      0x0FFFFFFF  /* Mask for cluster value */

/* FAT32 Constants */
#define FAT32_SECTOR_SIZE       512
#define FAT32_BOOT_SIG          0x29
#define FAT32_FSINFO_LEAD_SIG   0x41615252
#define FAT32_FSINFO_STRUC_SIG  0x61417272
#define FAT32_FSINFO_TRAIL_SIG  0xAA550000

#endif /* FAT32_TYPES_H */
