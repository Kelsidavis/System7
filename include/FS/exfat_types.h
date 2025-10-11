/* exFAT Filesystem Types and Structures */
#ifndef EXFAT_TYPES_H
#define EXFAT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* exFAT Boot Sector - starts at byte 0 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  JumpBoot[3];              /* Jump instruction to boot code */
    char     FileSystemName[8];        /* "EXFAT   " */
    uint8_t  MustBeZero[53];           /* Must be zeros */
    uint64_t PartitionOffset;          /* Sector offset of partition on drive */
    uint64_t VolumeLength;             /* Size of exFAT volume in sectors */
    uint32_t FatOffset;                /* Sector offset of 1st FAT */
    uint32_t FatLength;                /* Size of FAT in sectors */
    uint32_t ClusterHeapOffset;        /* Sector offset of data region */
    uint32_t ClusterCount;             /* Number of clusters */
    uint32_t FirstClusterOfRootDirectory; /* First cluster of root dir */
    uint32_t VolumeSerialNumber;       /* Volume serial number */
    uint16_t FileSystemRevision;       /* File system revision (major.minor) */
    uint16_t VolumeFlags;              /* Volume flags */
    uint8_t  BytesPerSectorShift;      /* Power of 2: bytes per sector */
    uint8_t  SectorsPerClusterShift;   /* Power of 2: sectors per cluster */
    uint8_t  NumberOfFats;             /* Number of FATs (should be 1) */
    uint8_t  DriveSelect;              /* INT 13h drive number */
    uint8_t  PercentInUse;             /* Percentage of allocated clusters */
    uint8_t  Reserved[7];              /* Reserved */
    uint8_t  BootCode[390];            /* Boot code */
    uint16_t BootSignature;            /* Boot signature (0xAA55) */
} EXFAT_BootSector;
#pragma pack(pop)

/* exFAT Directory Entry Types */
#define EXFAT_ENTRY_TYPE_MASK           0x1F
#define EXFAT_ENTRY_VALID               0x80
#define EXFAT_ENTRY_CONTINUED           0x40
#define EXFAT_ENTRY_OPTIONAL            0x20

#define EXFAT_ENTRY_TYPE_ALLOCATION     0x01  /* Volume/allocation bitmap */
#define EXFAT_ENTRY_TYPE_UPCASE         0x02  /* Upcase table */
#define EXFAT_ENTRY_TYPE_VOLUME_LABEL   0x03  /* Volume label */
#define EXFAT_ENTRY_TYPE_FILE           0x05  /* File directory entry */
#define EXFAT_ENTRY_TYPE_STREAM         0x00  /* Stream extension (with CONTINUED) */
#define EXFAT_ENTRY_TYPE_NAME           0x01  /* File name extension (with CONTINUED) */

/* exFAT Generic Directory Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* Entry type */
    uint8_t  CustomDefined[19];        /* Custom defined */
    uint32_t FirstCluster;             /* First cluster */
    uint64_t DataLength;               /* Data length */
} EXFAT_GenericEntry;
#pragma pack(pop)

/* exFAT File Directory Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* 0x85 = file entry */
    uint8_t  SecondaryCount;           /* Number of secondary entries */
    uint16_t SetChecksum;              /* Checksum of all entries in set */
    uint16_t FileAttributes;           /* File attributes */
    uint16_t Reserved1;
    uint32_t CreateTimestamp;          /* Create timestamp */
    uint32_t ModifyTimestamp;          /* Last modified timestamp */
    uint32_t AccessTimestamp;          /* Last accessed timestamp */
    uint8_t  Create10msIncrement;      /* Create time 10ms increment */
    uint8_t  Modify10msIncrement;      /* Modify time 10ms increment */
    uint8_t  CreateUtcOffset;          /* Create UTC offset */
    uint8_t  ModifyUtcOffset;          /* Modify UTC offset */
    uint8_t  AccessUtcOffset;          /* Access UTC offset */
    uint8_t  Reserved2[7];
} EXFAT_FileEntry;
#pragma pack(pop)

/* exFAT Stream Extension Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* 0xC0 = stream extension */
    uint8_t  GeneralSecondaryFlags;    /* Flags */
    uint8_t  Reserved1;
    uint8_t  NameLength;               /* Length of file name */
    uint16_t NameHash;                 /* Hash of up-cased file name */
    uint16_t Reserved2;
    uint64_t ValidDataLength;          /* Valid data length */
    uint32_t Reserved3;
    uint32_t FirstCluster;             /* First cluster of data */
    uint64_t DataLength;               /* Data length */
} EXFAT_StreamEntry;
#pragma pack(pop)

/* exFAT File Name Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* 0xC1 = file name extension */
    uint8_t  GeneralSecondaryFlags;    /* Flags */
    uint16_t FileName[15];             /* UTF-16 file name characters */
} EXFAT_NameEntry;
#pragma pack(pop)

/* exFAT Volume Label Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* 0x83 = volume label */
    uint8_t  CharacterCount;           /* Length of volume label */
    uint16_t VolumeLabel[11];          /* UTF-16 volume label */
    uint8_t  Reserved[8];
} EXFAT_VolumeLabelEntry;
#pragma pack(pop)

/* exFAT Allocation Bitmap Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  EntryType;                /* 0x81 = allocation bitmap */
    uint8_t  GeneralSecondaryFlags;    /* Flags */
    uint8_t  Reserved[18];
    uint32_t FirstCluster;             /* First cluster of bitmap */
    uint64_t DataLength;               /* Size of bitmap in bytes */
} EXFAT_BitmapEntry;
#pragma pack(pop)

/* exFAT Constants */
#define EXFAT_SIGNATURE                 "EXFAT   "
#define EXFAT_BOOT_SIGNATURE            0xAA55
#define EXFAT_ENTRY_SIZE                32
#define EXFAT_EOC                       0xFFFFFFFF  /* End of cluster chain */
#define EXFAT_BAD_CLUSTER               0xFFFFFFF7  /* Bad cluster marker */
#define EXFAT_ROOT_CLUSTER_MIN          2           /* Root dir starts at cluster 2+ */

/* exFAT File Attributes */
#define EXFAT_ATTR_READ_ONLY            0x0001
#define EXFAT_ATTR_HIDDEN               0x0002
#define EXFAT_ATTR_SYSTEM               0x0004
#define EXFAT_ATTR_DIRECTORY            0x0010
#define EXFAT_ATTR_ARCHIVE              0x0020

/* exFAT Volume Flags */
#define EXFAT_VOL_FLAG_ACTIVE_FAT       0x0001  /* Active FAT: 0=1st, 1=2nd */
#define EXFAT_VOL_FLAG_VOLUME_DIRTY     0x0002  /* Volume dirty flag */
#define EXFAT_VOL_FLAG_MEDIA_FAILURE    0x0004  /* Media failure flag */
#define EXFAT_VOL_FLAG_CLEAR_TO_ZERO    0x0008  /* Clear to zero flag */

/* exFAT Stream Flags */
#define EXFAT_STREAM_FLAG_ALLOC_POSSIBLE 0x01   /* Allocation possible */
#define EXFAT_STREAM_FLAG_NO_FAT_CHAIN   0x02   /* No FAT chain (contiguous) */

#endif /* EXFAT_TYPES_H */
