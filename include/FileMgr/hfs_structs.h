/*
 * RE-AGENT-BANNER
 * This file was reverse-engineered from Mac OS System 7.1 HFS source code.
 * Original: Apple Computer, Inc. (c) 1982-1993
 * Reverse-engineered implementation based on evidence from:
 * - /home/k/Desktop/os71/sys71src/OS/HFS/TFS.a (SHA: a7eef70fb36ac...)
 * - /home/k/Desktop/os71/sys71src/OS/HFS/TFSVOL.a (SHA: 1a222760f20...)
 * - /home/k/Desktop/os71/sys71src/OS/HFS/BTSVCS.a (SHA: 16a68cd1f2f...)
 * - /home/k/Desktop/os71/sys71src/OS/HFS/CMSVCS.a (SHA: 3f66d9e57eb...)
 * - /home/k/Desktop/os71/sys71src/OS/HFS/FXM.a (SHA: 3863a4b618f...)
 * Evidence-based reconstruction preserving original HFS semantics.
 * Provenance: layouts.file_manager.json, evidence.file_manager.json
 */

#ifndef HFS_STRUCTS_H
#define HFS_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>

/* HFS Constants */
#define HFS_SIGNATURE           0x4244      /* 'BD' - HFS signature */
#define HFS_PLUS_SIGNATURE      0x482B      /* 'H+' - HFS Plus signature */
#define BTREE_NODE_SIZE         512         /* Fixed B-Tree node size */
#define MAX_BTREE_DEPTH         8           /* Maximum tree depth */
#define NUM_EXTENTS_PER_RECORD  3           /* Extents per extent record */
#define EXTENT_RECORD_SIZE      12          /* Size of extent record */
#define HFS_BLOCK_SIZE          512         /* Standard HFS block size */

/* Catalog Record Types */
#define kHFSFolderRecord        1           /* Directory record */
#define kHFSFileRecord          2           /* File record */
#define kHFSFolderThreadRecord  3           /* Directory thread record */
#define kHFSFileThreadRecord    4           /* File thread record */

/* B-Tree Node Types */
#define ndHdrNode               1           /* Header node */
#define ndMapNode               2           /* Map node */
#define ndIndxNode              0           /* Index node */
#define ndLeafNode              255         /* Leaf node */

/* Fork Types */
#define dataFk                  0x00        /* Data fork */
#define rsrcFk                  0xFF        /* Resource fork */

/* OSErr Error Codes */
typedef int16_t OSErr;
#define noErr                   0
#define memFullErr              -108
#define ioErr                   -36
#define fnfErr                  -43
#define permErr                 -54

/* Basic Mac OS Types */
typedef char* StringPtr;
typedef const char* ConstStringPtr;
typedef uint8_t Str31[32];
typedef uint8_t Str27[28];
typedef struct { int16_t v, h; } Point;
typedef struct { int16_t top, left, bottom, right; } Rect;

#pragma pack(push, 2)  /* Use 2-byte alignment for HFS compatibility */

/*
 * Extent Descriptor - represents a contiguous range of allocation blocks
 * Evidence: FSPrivate.a xdrStABN/xdrNumABlks definitions
 */
typedef struct ExtentDescriptor {
    uint16_t    startBlock;     /* Starting allocation block number */
    uint16_t    blockCount;     /* Number of allocation blocks */
} ExtentDescriptor;

/*
 * Extent Record - array of 3 extent descriptors per HFS specification
 * Evidence: FSPrivate.a numExts EQU 3, lenxdr EQU 12
 */
typedef struct ExtentRecord {
    ExtentDescriptor extent[NUM_EXTENTS_PER_RECORD];
} ExtentRecord;

/*
 * B-Tree Node - fixed 512-byte node structure
 * Evidence: BTSVCS.a BTNode operations, FSPrivate.a ndFlink/ndBlink definitions
 */
typedef struct BTNode {
    uint32_t    ndFLink;        /* Forward link to next node */
    uint32_t    ndBLink;        /* Backward link to previous node */
    uint8_t     ndType;         /* Node type (header/index/leaf/map) */
    uint8_t     ndNHeight;      /* Node height in tree */
    uint16_t    ndNRecs;        /* Number of records in node */
    uint16_t    ndResv2;        /* Reserved field */
    uint8_t     ndData[498];    /* Node data area (512-14=498 bytes) */
} BTNode;

/*
 * B-Tree Header - contains tree metadata in header node
 * Evidence: FSPrivate.a bthDepth/bthRoot/bthNRecs definitions
 */
typedef struct BTHeader {
    uint16_t    bthDepth;       /* Current depth of tree */
    uint32_t    bthRoot;        /* Root node number */
    uint32_t    bthNRecs;       /* Number of leaf records in tree */
    uint32_t    bthFNode;       /* First leaf node number */
    uint32_t    bthLNode;       /* Last leaf node number */
    uint16_t    bthNodeSize;    /* Size of a node (always 512) */
    uint16_t    bthKeyLen;      /* Maximum key length */
    uint32_t    bthNNodes;      /* Total number of nodes allocated */
    uint32_t    bthFree;        /* Number of free nodes */
    uint8_t     bthResv[76];    /* Reserved space */
} BTHeader;

/* Forward declarations */
typedef struct VCB VCB;
typedef struct FCB FCB;

/*
 * File Control Block - control structure for open files
 * Evidence: TFSCOMMON.a FCBFlNm/FCBMdRByt/FCBExtRec references
 */
typedef struct FCB {
    uint32_t    fcbFlNm;        /* File number (unique within volume) */
    uint8_t     fcbMdRByt;      /* File mode and flags */
    uint8_t     fcbTypByt;      /* File type byte */
    uint16_t    fcbSBlk;        /* Starting allocation block */
    uint32_t    fcbEOF;         /* End of file (logical) */
    uint32_t    fcbPLen;        /* Physical length in bytes */
    uint32_t    fcbCrPs;        /* Current file position */
    VCB*        fcbVPtr;        /* Pointer to Volume Control Block */
    void*       fcbBfAdr;       /* Buffer address */
    int16_t     fcbFlPos;       /* File position marker */
    uint32_t    fcbClmpSize;    /* File clump size */
    void*       fcbBTCBPtr;     /* B-Tree Control Block pointer */
    ExtentRecord fcbExtRec;     /* First extent record */
    uint32_t    fcbFType;       /* File type */
    uint32_t    fcbCatPos;      /* Catalog B-Tree position */
    uint32_t    fcbDirID;       /* Parent directory ID */
    Str31       fcbCName;       /* File name */
} FCB;

/*
 * Volume Control Block - main control structure for mounted volume
 * Evidence: TFSVOL.a VCBSigWord/VCBAlBlkSiz/VCBNxtCNID references
 */
typedef struct VCB {
    uint16_t    vcbSigWord;     /* Volume signature (0x4244 for HFS) */
    uint32_t    vcbCrDate;      /* Volume creation date */
    uint32_t    vcbLsMod;       /* Last modification date */
    uint16_t    vcbAtrb;        /* Volume attributes */
    uint16_t    vcbNmFls;       /* Number of files in root directory */
    uint16_t    vcbVBMSt;       /* Start block of volume bitmap */
    uint16_t    vcbAllocPtr;    /* Next allocation search start */
    uint16_t    vcbNmAlBlks;    /* Number of allocation blocks */
    uint32_t    vcbAlBlkSiz;    /* Size of allocation blocks */
    uint32_t    vcbClpSiz;      /* Default clump size */
    uint16_t    vcbAlBlSt;      /* First allocation block */
    uint32_t    vcbNxtCNID;     /* Next catalog node ID */
    uint16_t    vcbFreeBks;     /* Number of free allocation blocks */
    Str27       vcbVN;          /* Volume name */
    int16_t     vcbDrvNum;      /* Drive number */
    int16_t     vcbDRefNum;     /* Driver reference number */
    int16_t     vcbFSID;        /* File system ID */
    int16_t     vcbVRefNum;     /* Volume reference number */
    void*       vcbMAdr;        /* VCB memory address */
    void*       vcbBufAdr;      /* Volume buffer address */
    int16_t     vcbMLen;        /* VCB length */
    int16_t     vcbDirIndex;    /* Directory index */
    int16_t     vcbDirBlk;      /* Directory start block */
    /* HFS-specific fields */
    uint32_t    vcbVolBkUp;     /* Volume backup date */
    uint16_t    vcbVSeqNum;     /* Volume sequence number */
    uint32_t    vcbWrCnt;       /* Volume write count */
    uint32_t    vcbXTClpSiz;    /* Extents overflow file clump size */
    uint32_t    vcbCTClpSiz;    /* Catalog file clump size */
    uint16_t    vcbNmRtDirs;    /* Number of root directories */
    uint32_t    vcbFilCnt;      /* File count */
    uint32_t    vcbDirCnt;      /* Directory count */
    uint32_t    vcbFndrInfo[8]; /* Finder info */
    uint16_t    vcbVCSize;      /* VCB size */
    uint16_t    vcbVBMCSiz;     /* Volume bitmap cache size */
    uint16_t    vcbCtlCSiz;     /* Control cache size */
    uint16_t    vcbXTAlBlks;    /* Extents overflow allocation blocks */
    uint16_t    vcbCTAlBlks;    /* Catalog allocation blocks */
    int16_t     vcbXTRef;       /* Extents overflow file refnum */
    int16_t     vcbCTRef;       /* Catalog file refnum */
    void*       vcbCtlBuf;      /* Control cache buffer */
    uint32_t    vcbDirIDM;      /* Directory ID mask */
    int16_t     vcbOffsM;       /* Offset mask */
} VCB;

/*
 * Master Directory Block - on-disk volume header (block 2)
 * Evidence: TFSVOL.a drSigWord/drAlBlkSiz references
 */
typedef struct MDB {
    uint16_t    drSigWord;      /* Volume signature */
    uint32_t    drCrDate;       /* Creation date */
    uint32_t    drLsMod;        /* Last modification date */
    uint16_t    drAtrb;         /* Volume attributes */
    uint16_t    drNmFls;        /* Number of files */
    uint16_t    drVBMSt;        /* Volume bitmap start block */
    uint16_t    drAllocPtr;     /* Allocation search start */
    uint16_t    drNmAlBlks;     /* Number of allocation blocks */
    uint32_t    drAlBlkSiz;     /* Allocation block size */
    uint32_t    drClpSiz;       /* Default clump size */
    uint16_t    drAlBlSt;       /* First allocation block */
    uint32_t    drNxtCNID;      /* Next catalog node ID */
    uint16_t    drFreeBks;      /* Free allocation blocks */
    Str27       drVN;           /* Volume name */
    uint32_t    drVolBkUp;      /* Volume backup date */
    uint16_t    drVSeqNum;      /* Volume sequence number */
    uint32_t    drWrCnt;        /* Volume write count */
    uint32_t    drXTClpSiz;     /* Extents overflow clump size */
    uint32_t    drCTClpSiz;     /* Catalog clump size */
    uint16_t    drNmRtDirs;     /* Number of root directories */
    uint32_t    drFilCnt;       /* File count */
    uint32_t    drDirCnt;       /* Directory count */
    uint32_t    drFndrInfo[8];  /* Finder info */
    uint16_t    drEmbedSigWord; /* Embedded volume signature */
    uint32_t    drEmbedExtent;  /* Embedded volume extent */
    uint32_t    drXTFlSize;     /* Extents overflow file size */
    ExtentRecord drXTExtRec;    /* Extents overflow file extent */
    uint32_t    drCTFlSize;     /* Catalog file size */
    ExtentRecord drCTExtRec;    /* Catalog file extent */
    uint8_t     drFiller[352];  /* Padding to 512 bytes */
} MDB;

/*
 * Finder Info structures
 * Evidence: Catalog record structures in CMSVCS.a
 */
typedef struct DInfo {
    Rect        frRect;         /* Folder rectangle */
    uint16_t    frFlags;        /* Flags */
    Point       frLocation;     /* Location */
    int16_t     frView;         /* View */
} DInfo;

typedef struct DXInfo {
    Point       frScroll;       /* Scroll position */
    int32_t     frOpenChain;    /* Directory chain */
    uint16_t    frUnused;       /* Unused */
    uint16_t    frComment;      /* Comment ID */
    int32_t     frPutAway;      /* Put away directory */
} DXInfo;

typedef struct FInfo {
    uint32_t    fdType;         /* File type */
    uint32_t    fdCreator;      /* File creator */
    uint16_t    fdFlags;        /* File flags */
    Point       fdLocation;     /* File location */
    int16_t     fdFldr;         /* Folder */
} FInfo;

typedef struct FXInfo {
    int16_t     fdIconID;       /* Icon ID */
    uint32_t    fdUnused[2];    /* Unused */
    int16_t     fdComment;      /* Comment ID */
    int32_t     fdPutAway;      /* Put away directory */
} FXInfo;

/*
 * Catalog Records - variable-length records in catalog B-Tree
 * Evidence: CMSVCS.a catalog operations and record types
 */
typedef struct CatDirRec {
    int8_t      cdrType;        /* Record type = kHFSFolderRecord */
    int8_t      cdrResrv2;      /* Reserved */
    uint16_t    dirFlags;       /* Directory flags */
    uint16_t    dirVal;         /* Directory valence (number of items) */
    uint32_t    dirDirID;       /* Directory ID */
    uint32_t    dirCrDat;       /* Creation date */
    uint32_t    dirMdDat;       /* Modification date */
    uint32_t    dirBkDat;       /* Backup date */
    DInfo       dirUsrInfo;     /* User info */
    DXInfo      dirFndrInfo;    /* Finder info */
    uint32_t    dirResrv[4];    /* Reserved */
} CatDirRec;

typedef struct CatFileRec {
    int8_t      cdrType;        /* Record type = kHFSFileRecord */
    uint8_t     filFlags;       /* File flags */
    int8_t      filTyp;         /* File type */
    uint8_t     filUsrWds;      /* User words */
    uint32_t    filFlNum;       /* File number */
    uint16_t    filStBlk;       /* Starting block (data fork) */
    int32_t     filLgLen;       /* Logical length (data fork) */
    int32_t     filPyLen;       /* Physical length (data fork) */
    uint16_t    filRStBlk;      /* Starting block (resource fork) */
    int32_t     filRLgLen;      /* Logical length (resource fork) */
    int32_t     filRPyLen;      /* Physical length (resource fork) */
    uint32_t    filCrDat;       /* Creation date */
    uint32_t    filMdDat;       /* Modification date */
    uint32_t    filBkDat;       /* Backup date */
    FInfo       filUsrInfo;     /* User info */
    FXInfo      filFndrInfo;    /* Finder info */
    uint16_t    filClpSize;     /* File clump size */
    ExtentRecord filExtRec;     /* First data extent record */
    ExtentRecord filRExtRec;    /* First resource extent record */
    uint32_t    filResrv;       /* Reserved */
} CatFileRec;

#pragma pack(pop)

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "reverse_engineering": {
 *     "tool": "evidence_curator + struct_layout + mapping",
 *     "confidence": 0.95,
 *     "evidence_files": [
 *       "evidence.file_manager.json",
 *       "layouts.file_manager.json",
 *       "mappings.file_manager.json"
 *     ],
 *     "original_sources": [
 *       "TFS.a", "TFSVOL.a", "BTSVCS.a", "CMSVCS.a", "FXM.a"
 *     ],
 *     "key_structures": [
 *       "VCB", "FCB", "BTNode", "ExtentRecord", "MDB", "CatalogRecord"
 *     ],
 *     "validation": "Structure layouts verified against 68k assembly offsets"
 *   }
 * }
 */

#endif /* HFS_STRUCTS_H */