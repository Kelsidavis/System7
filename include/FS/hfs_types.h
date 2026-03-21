/* HFS Classic On-Disk Types and Structures */
#ifndef HFS_TYPES_H
#define HFS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "../SystemTypes.h"  /* For DirID */

/* Volume and file references */
typedef uint32_t VRefNum;
/* Use system DirID definition from MacTypes.h */
/* typedef uint32_t DirID; */
typedef uint32_t FileID;

/* Node types in filesystem */
typedef enum {
    kNodeFile,
    kNodeDir
} NodeKind;

/* Catalog entry for VFS layer */
typedef struct {
    char     name[32];      /* ASCII/UTF-8 converted from MacRoman */
    NodeKind kind;
    uint32_t creator;       /* OSType - 4-char code */
    uint32_t type;          /* OSType - 4-char code */
    uint32_t size;          /* Data fork size */
    uint16_t flags;         /* Finder flags */
    uint32_t modTime;       /* Modification time - seconds since 1904 */
    uint32_t createTime;    /* Creation time - seconds since 1904 */
    DirID    parent;        /* Parent directory CNID */
    FileID   id;            /* This node's CNID */
} CatEntry;

/* Volume Control Block */
typedef struct {
    char     name[32];      /* Volume name */
    VRefNum  vRefNum;       /* Volume reference number */
    uint64_t totalBytes;
    uint64_t freeBytes;
    DirID    rootID;        /* Root directory CNID (usually 2) */
    bool     mounted;
} VolumeControlBlock;

/* HFS Extent - allocation block range */
typedef struct {
    uint16_t startBlock;
    uint16_t blockCount;
} HFS_Extent;

/* Master Directory Block (MDB) - at sector 2 */
#pragma pack(push,1)
typedef struct {
    uint16_t drSigWord;       /* 0x4244 'BD' */
    uint32_t drCrDate;        /* Creation date */
    uint32_t drLsMod;         /* Last modification date */
    uint16_t drAtrb;          /* Volume attributes */
    uint16_t drNmFls;         /* Number of files in root */
    uint16_t drVBMSt;         /* First block of volume bitmap */
    uint16_t drAllocPtr;      /* Start of next allocation search */
    uint16_t drNmAlBlks;      /* Number of allocation blocks */
    uint32_t drAlBlkSiz;      /* Bytes per allocation block */
    uint32_t drClpSiz;        /* Default clump size */
    uint16_t drAlBlSt;        /* First allocation block */
    uint32_t drNxtCNID;       /* Next available CNID */
    uint16_t drFreeBks;       /* Free allocation blocks */
    uint8_t  drVN[28];        /* Volume name (Pascal string) */
    uint32_t drVolBkUp;       /* Last backup date */
    uint16_t drVSeqNum;       /* Volume backup sequence number */
    uint32_t drWrCnt;         /* Volume write count */
    uint32_t drXTClpSiz;      /* Extents overflow clump size */
    uint32_t drCTClpSiz;      /* Catalog clump size */
    uint16_t drNmRtDirs;      /* Number of directories in root */
    uint32_t drFilCnt;        /* Number of files */
    uint32_t drDirCnt;        /* Number of directories */
    uint32_t drFndrInfo[8];   /* Finder info */
    uint16_t drEmbedSigWord;  /* Embedded volume signature */
    HFS_Extent drEmbedExtent; /* Embedded volume location */
    uint32_t drXTFlSize;      /* Extents overflow file size */
    HFS_Extent drXTExtRec[3]; /* First extents of extents overflow */
    uint32_t drCTFlSize;      /* Catalog file size */
    HFS_Extent drCTExtRec[3]; /* First extents of catalog */
} HFS_MDB;
#pragma pack(pop)

/* B-Tree structures */
#pragma pack(push,1)
typedef struct {
    uint16_t depth;
    uint32_t rootNode;
    uint32_t leafRecords;
    uint32_t firstLeafNode;
    uint32_t lastLeafNode;
    uint16_t nodeSize;
    uint16_t keyCompareType;
    uint32_t totalNodes;
    uint32_t freeNodes;
    uint16_t reserved1;
    uint32_t clumpSize;
    uint8_t  btreeType;
    uint8_t  reserved2;
    uint32_t attributes;
    uint32_t reserved3[16];
} HFS_BTHeaderRec;

typedef struct {
    uint32_t fLink;           /* Forward link */
    uint32_t bLink;           /* Backward link */
    uint8_t  kind;            /* Node type */
    uint8_t  height;          /* Node height */
    uint16_t numRecords;      /* Number of records */
    uint16_t reserved;
} HFS_BTNodeDesc;
#pragma pack(pop)

/* Node types */
enum {
    kBTHeaderNode = 1,
    kBTMapNode    = 2,
    kBTIndexNode  = 0,
    kBTLeafNode   = 0xFF
};

/* Catalog key */
#pragma pack(push,1)
typedef struct {
    uint8_t  keyLength;       /* Key length (excluding this byte) */
    uint8_t  reserved;
    uint32_t parentID;        /* Parent directory CNID */
    uint8_t  nameLength;      /* Name length (1-31) */
    uint8_t  name[31];        /* MacRoman name */
} HFS_CatKey;

/* Catalog data record types */
enum {
    kHFS_FolderRecord        = 0x0100,
    kHFS_FileRecord          = 0x0200,
    kHFS_FolderThreadRecord  = 0x0300,
    kHFS_FileThreadRecord    = 0x0400
};

/* Catalog file record */
typedef struct {
    int16_t  recordType;      /* kHFSFileRecord */
    uint8_t  flags;
    uint8_t  fileType;
    uint32_t fileID;          /* CNID */
    uint16_t dataStartBlock;
    uint32_t dataLogicalSize;
    uint32_t dataPhysicalSize;
    uint16_t rsrcStartBlock;
    uint32_t rsrcLogicalSize;
    uint32_t rsrcPhysicalSize;
    uint32_t createDate;
    uint32_t modifyDate;
    uint32_t backupDate;
    uint8_t  finderInfo[16];
    uint16_t clumpSize;
    HFS_Extent dataExtents[3];
    HFS_Extent rsrcExtents[3];
    uint32_t reserved;
} HFS_CatFileRec;

/* Catalog folder record */
typedef struct {
    int16_t  recordType;      /* kHFSFolderRecord */
    uint16_t flags;
    uint16_t valence;         /* Number of items in folder */
    uint32_t folderID;        /* CNID */
    uint32_t createDate;
    uint32_t modifyDate;
    uint32_t backupDate;
    uint8_t  finderInfo[16];
    uint32_t reserved[4];
} HFS_CatFolderRec;

/* Thread record */
typedef struct {
    int16_t  recordType;      /* Thread type */
    uint8_t  reserved[8];
    uint32_t parentID;
    uint8_t  nameLength;
    uint8_t  name[31];
} HFS_CatThreadRec;
#pragma pack(pop)

/* Constants */
#define HFS_SECTOR_SIZE      512
#define HFS_MDB_SECTOR       2
#define HFS_SIGNATURE        0x4244  /* 'BD' */
#define HFS_ROOT_CNID        1
#define HFS_ROOT_PARENT_CNID 1
#define HFS_FIRST_CNID       16
#define MAC_EPOCH_DELTA      2082844800u /* Seconds between 1904 and 1970 */

#endif /* HFS_TYPES_H */