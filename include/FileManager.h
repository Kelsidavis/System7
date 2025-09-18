/*
 * FileManager.h - System 7.1 File Manager Portable C Implementation
 *
 * This header defines the public API for the Macintosh System 7.1 File Manager,
 * providing HFS (Hierarchical File System) support for modern platforms.
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Based on Apple System Software 7.1 architecture
 */

#ifndef __FILEMANAGER_H__
#define __FILEMANAGER_H__

#include "MacTypes.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform configuration */
#if defined(__arm64__) || defined(__aarch64__)
    #define FM_PLATFORM_ARM64 1
#elif defined(__x86_64__) || defined(_M_X64)
    #define FM_PLATFORM_X86_64 1
#else
    #error "Unsupported platform - requires ARM64 or x86_64"
#endif

/* Thread safety configuration */
#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION FM_Mutex;
#else
    #include <pthread.h>
    typedef pthread_mutex_t FM_Mutex;
#endif

/* Forward declarations */
typedef struct VCB VCB;
typedef struct FCB FCB;
typedef struct WDCB WDCB;

/* File Manager specific types */
typedef int16_t VolumeRefNum;
typedef int16_t FileRefNum;
typedef int16_t WDRefNum;
typedef uint32_t DirID;
typedef uint32_t FileID;
typedef uint32_t CNodeID;

/* Mac OS date/time (seconds since Jan 1, 1904) */
typedef uint32_t MacDates;

/* Additional File System Error Codes beyond MacTypes.h */
enum {
    dirNFErr        = -120,     /* Directory not found */
    tmwdoErr        = -121,     /* Too many working directories open */
    badMovErr       = -122,     /* Can't move to this location */
    fidNotFound     = -1300,    /* File ID not found */
    fidExists       = -1301,    /* File ID already exists */
    notAFileErr     = -1302,    /* Not a file */
    diffVolErr      = -1303,    /* Different volume */
    fsDataTooBigErr = -1310,    /* File/fork too big */
    fxRangeErr      = -1320     /* File position out of range */
};

/* File attributes */
enum {
    kioFlAttribLocked     = 0x01,
    kioFlAttribResOpen    = 0x04,
    kioFlAttribDataOpen   = 0x08,
    kioFlAttribDir        = 0x10,
    kioFlAttribCopyProt   = 0x40,
    kioFlAttribFileOpen   = 0x80
};

/* Volume attributes */
enum {
    kioVAtrbHardwareLock  = 0x0080,
    kioVAtrbSoftwareLock  = 0x8000,
    kioVAtrbEjectable     = 0x0008,
    kioVAtrbOffline       = 0x0020
};

/* Fork types */
typedef enum {
    DataFork = 0,
    ResourceFork = 0xFF
} ForkType;

/* Finder Info structure (16 bytes) */
typedef struct FInfo {
    uint32_t    fdType;         /* File type */
    uint32_t    fdCreator;      /* File creator */
    uint16_t    fdFlags;        /* Finder flags */
    struct {
        int16_t v;              /* Vertical position */
        int16_t h;              /* Horizontal position */
    } fdLocation;
    uint16_t    fdFldr;         /* Folder or window */
} FInfo;

/* Extended Finder Info (16 bytes) */
typedef struct FXInfo {
    uint16_t    fdIconID;       /* Icon ID */
    uint16_t    fdReserved[3];  /* Reserved */
    uint8_t     fdScript;       /* Script system */
    uint8_t     fdXFlags;       /* Extended flags */
    uint16_t    fdComment;      /* Comment ID */
    uint32_t    fdPutAway;      /* Put away directory ID */
} FXInfo;

/* FSSpec - File System Specification */
typedef struct FSSpec {
    VolumeRefNum    vRefNum;    /* Volume reference number */
    DirID           parID;      /* Parent directory ID */
    Str255          name;       /* File/directory name */
} FSSpec;

/* HFS Extent descriptor */
typedef struct ExtDescriptor {
    uint16_t    startBlock;     /* Starting allocation block */
    uint16_t    blockCount;     /* Number of allocation blocks */
} ExtDescriptor;

/* HFS Extent record (3 extents) */
typedef ExtDescriptor ExtDataRec[3];

/* File parameter block - Core structure for file operations */
typedef struct ParamBlockRec {
    /* Standard header (24 bytes) */
    struct ParamBlockRec* qLink;       /* Queue link pointer */
    uint16_t        qType;              /* Queue type */
    uint16_t        ioTrap;             /* Trap word */
    void*           ioCmdAddr;          /* Command address */
    void*           ioCompletion;       /* Completion routine */
    OSErr           ioResult;           /* Result code */
    StringPtr       ioNamePtr;          /* Name pointer */
    VolumeRefNum    ioVRefNum;          /* Volume reference */

    /* File/Volume specific fields */
    union {
        /* File operations */
        struct {
            FileRefNum  ioRefNum;       /* File reference number */
            uint8_t     ioVersNum;      /* Version number */
            uint8_t     ioPermssn;      /* Permission */
            void*       ioMisc;         /* Miscellaneous */
            void*       ioBuffer;       /* Data buffer */
            uint32_t    ioReqCount;     /* Requested count */
            uint32_t    ioActCount;     /* Actual count */
            uint16_t    ioPosMode;      /* Position mode */
            int32_t     ioPosOffset;    /* Position offset */
        } ioParam;

        /* File info operations */
        struct {
            FileRefNum  ioFRefNum;      /* File reference number */
            uint8_t     ioFVersNum;     /* File version */
            uint8_t     filler1;
            int16_t     ioFDirIndex;    /* Directory index */
            uint8_t     ioFlAttrib;     /* File attributes */
            uint8_t     ioFlVersNum;    /* File version number */
            FInfo       ioFlFndrInfo;   /* Finder info */
            DirID       ioDirID;        /* Directory ID */
            uint16_t    ioFlStBlk;      /* Start block */
            uint32_t    ioFlLgLen;      /* Logical length */
            uint32_t    ioFlPyLen;      /* Physical length */
            uint16_t    ioFlRStBlk;     /* Resource start block */
            uint32_t    ioFlRLgLen;     /* Resource logical length */
            uint32_t    ioFlRPyLen;     /* Resource physical length */
            uint32_t    ioFlCrDat;      /* Creation date */
            uint32_t    ioFlMdDat;      /* Modification date */
        } fileParam;

        /* Volume operations */
        struct {
            uint32_t    filler2;
            int16_t     ioVolIndex;     /* Volume index */
            uint32_t    ioVCrDate;      /* Creation date */
            uint32_t    ioVLsMod;       /* Last modification */
            uint16_t    ioVAtrb;        /* Volume attributes */
            uint16_t    ioVNmFls;       /* Number of files */
            uint16_t    ioVBitMap;      /* Bitmap start */
            uint16_t    ioAllocPtr;     /* Allocation pointer */
            uint16_t    ioVNmAlBlks;    /* Number of allocation blocks */
            uint32_t    ioVAlBlkSiz;    /* Allocation block size */
            uint32_t    ioVClpSiz;      /* Clump size */
            uint16_t    ioAlBlSt;       /* First allocation block */
            uint32_t    ioVNxtCNID;     /* Next catalog node ID */
            uint16_t    ioVFrBlk;       /* Number of free blocks */
            uint16_t    ioVSigWord;     /* Signature (0x4244 for HFS) */
            uint16_t    ioVDrvInfo;     /* Drive info */
            uint16_t    ioVDRefNum;     /* Driver ref number */
            uint16_t    ioVFSID;        /* File system ID */
            uint32_t    ioVBkUp;        /* Backup date */
            uint16_t    ioVSeqNum;      /* Sequence number */
            uint32_t    ioVWrCnt;       /* Write count */
            uint32_t    ioVFilCnt;      /* File count */
            uint32_t    ioVDirCnt;      /* Directory count */
            uint8_t     ioVFndrInfo[32];/* Finder info */
        } volumeParam;
    } u;
} ParamBlockRec, *ParmBlkPtr;

/* Catalog info parameter block for extended operations */
typedef struct CInfoPBRec {
    /* Standard header */
    struct CInfoPBRec* qLink;
    uint16_t        qType;
    uint16_t        ioTrap;
    void*           ioCmdAddr;
    void*           ioCompletion;
    OSErr           ioResult;
    StringPtr       ioNamePtr;
    VolumeRefNum    ioVRefNum;

    /* Catalog specific */
    FileRefNum      ioRefNum;
    uint8_t         ioVersNum;
    uint8_t         ioPermssn;
    uint32_t        ioMisc;
    int32_t         ioDTBuffer;
    int16_t         ioDirIndex;

    union {
        struct {    /* For files */
            uint8_t     ioFlAttrib;
            uint8_t     ioFlVersNum;
            FInfo       ioFlFndrInfo;
            DirID       ioFlParID;
            uint16_t    ioFlStBlk;
            uint32_t    ioFlLgLen;
            uint32_t    ioFlPyLen;
            uint16_t    ioFlRStBlk;
            uint32_t    ioFlRLgLen;
            uint32_t    ioFlRPyLen;
            uint32_t    ioFlCrDat;
            uint32_t    ioFlMdDat;
            uint32_t    ioFlBkDat;
            FXInfo      ioFlXFndrInfo;
            CNodeID     ioFlParID2;
            uint32_t    ioFlClpSiz;
            ExtDataRec  ioFlExtRec;
            ExtDataRec  ioFlRExtRec;
            uint32_t    ioFlReserved;
        } hFileInfo;

        struct {    /* For directories */
            uint8_t     ioDrAttrib;
            uint8_t     ioDrVersNum;
            uint8_t     ioDrFndrInfo[16];
            DirID       ioDrDirID;
            uint16_t    ioDrNmFls;
            uint16_t    filler3[9];
            uint32_t    ioDrCrDat;
            uint32_t    ioDrMdDat;
            uint32_t    ioDrBkDat;
            uint8_t     ioDrXFndrInfo[16];
            DirID       ioDrParID;
        } dirInfo;
    } u;
} CInfoPBRec, *CInfoPBPtr;

/* Working Directory parameter block */
typedef struct WDPBRec {
    struct WDPBRec* qLink;
    uint16_t        qType;
    uint16_t        ioTrap;
    void*           ioCmdAddr;
    void*           ioCompletion;
    OSErr           ioResult;
    StringPtr       ioNamePtr;
    VolumeRefNum    ioVRefNum;
    uint16_t        filler;
    int16_t         ioWDIndex;
    DirID           ioWDProcID;
    WDRefNum        ioWDVRefNum;
    uint16_t        filler2[7];
    DirID           ioWDDirID;
} WDPBRec, *WDPBPtr;

/* File Control Block access */
typedef struct FCBPBRec {
    struct FCBPBRec* qLink;
    uint16_t        qType;
    uint16_t        ioTrap;
    void*           ioCmdAddr;
    void*           ioCompletion;
    OSErr           ioResult;
    StringPtr       ioNamePtr;
    VolumeRefNum    ioVRefNum;
    FileRefNum      ioRefNum;
    uint16_t        filler;
    int16_t         ioFCBIndx;
    uint32_t        ioFCBFlNm;
    uint16_t        ioFCBFlags;
    uint16_t        ioFCBStBlk;
    uint32_t        ioFCBEOF;
    uint32_t        ioFCBPLen;
    uint32_t        ioFCBCrPs;
    WDRefNum        ioFCBVRefNum;
    uint32_t        ioFCBClpSiz;
    DirID           ioFCBParID;
} FCBPBRec, *FCBPBPtr;

/* Volume mount info */
typedef struct VolMountInfoHeader {
    uint16_t        length;         /* Length of this structure */
    uint32_t        media;          /* Media type */
    uint16_t        flags;          /* Mount flags */
    uint8_t         reserved[8];    /* Reserved */
} VolMountInfoHeader;

/* ============================================================================
 * File Manager Public API
 * ============================================================================ */

/* File System Initialization */
OSErr FM_Initialize(void);
OSErr FM_Shutdown(void);

/* File Operations - Basic */
OSErr FSOpen(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSClose(FileRefNum refNum);
OSErr FSRead(FileRefNum refNum, uint32_t* count, void* buffer);
OSErr FSWrite(FileRefNum refNum, uint32_t* count, const void* buffer);

/* File Operations - Extended */
OSErr FSOpenDF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSOpenRF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSCreate(ConstStr255Param fileName, VolumeRefNum vRefNum, uint32_t creator, uint32_t fileType);
OSErr FSDelete(ConstStr255Param fileName, VolumeRefNum vRefNum);
OSErr FSRename(ConstStr255Param oldName, ConstStr255Param newName, VolumeRefNum vRefNum);

/* File Position and Size */
OSErr FSGetFPos(FileRefNum refNum, uint32_t* position);
OSErr FSSetFPos(FileRefNum refNum, uint16_t posMode, int32_t posOffset);
OSErr FSGetEOF(FileRefNum refNum, uint32_t* eof);
OSErr FSSetEOF(FileRefNum refNum, uint32_t eof);
OSErr FSAllocate(FileRefNum refNum, uint32_t* count);

/* File Information */
OSErr FSGetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, FInfo* fndrInfo);
OSErr FSSetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, const FInfo* fndrInfo);
OSErr FSGetCatInfo(CInfoPBPtr paramBlock);
OSErr FSSetCatInfo(CInfoPBPtr paramBlock);

/* Directory Operations */
OSErr FSMakeFSSpec(VolumeRefNum vRefNum, DirID dirID, ConstStr255Param fileName, FSSpec* spec);
OSErr FSCreateDir(ConstStr255Param dirName, VolumeRefNum vRefNum, DirID* createdDirID);
OSErr FSDeleteDir(ConstStr255Param dirName, VolumeRefNum vRefNum);
OSErr FSGetWDInfo(WDRefNum wdRefNum, VolumeRefNum* vRefNum, DirID* dirID, uint32_t* procID);
OSErr FSOpenWD(VolumeRefNum vRefNum, DirID dirID, uint32_t procID, WDRefNum* wdRefNum);
OSErr FSCloseWD(WDRefNum wdRefNum);

/* Volume Operations */
OSErr FSMount(uint16_t drvNum, void* buffer);
OSErr FSUnmount(VolumeRefNum vRefNum);
OSErr FSEject(VolumeRefNum vRefNum);
OSErr FSFlushVol(ConstStr255Param volName, VolumeRefNum vRefNum);
OSErr FSGetVInfo(VolumeRefNum vRefNum, StringPtr volName, uint16_t* actualVRefNum, uint32_t* freeBytes);
OSErr FSSetVol(ConstStr255Param volName, VolumeRefNum vRefNum);
OSErr FSGetVol(StringPtr volName, VolumeRefNum* vRefNum);

/* File ID Operations */
OSErr FSCreateFileIDRef(ConstStr255Param fileName, VolumeRefNum vRefNum, DirID dirID, FileID* fileID);
OSErr FSDeleteFileIDRef(ConstStr255Param fileName, VolumeRefNum vRefNum);
OSErr FSResolveFileIDRef(ConstStr255Param volName, VolumeRefNum vRefNum, FileID fileID, FSSpec* spec);
OSErr FSExchangeFiles(const FSSpec* file1, const FSSpec* file2);

/* Alias and Path Resolution */
OSErr FSResolveAliasFile(FSSpec* theSpec, bool resolveAliasChains, bool* targetIsFolder, bool* wasAliased);
OSErr FSMakeAlias(const FSSpec* fromFile, const FSSpec* target, void** alias);

/* Parameter Block Operations (Low-level) */
OSErr PBOpenSync(ParmBlkPtr paramBlock);
OSErr PBOpenAsync(ParmBlkPtr paramBlock);
OSErr PBCloseSync(ParmBlkPtr paramBlock);
OSErr PBCloseAsync(ParmBlkPtr paramBlock);
OSErr PBReadSync(ParmBlkPtr paramBlock);
OSErr PBReadAsync(ParmBlkPtr paramBlock);
OSErr PBWriteSync(ParmBlkPtr paramBlock);
OSErr PBWriteAsync(ParmBlkPtr paramBlock);
OSErr PBGetCatInfoSync(CInfoPBPtr paramBlock);
OSErr PBGetCatInfoAsync(CInfoPBPtr paramBlock);
OSErr PBSetCatInfoSync(CInfoPBPtr paramBlock);
OSErr PBSetCatInfoAsync(CInfoPBPtr paramBlock);

/* HFS-specific Parameter Block Operations */
OSErr PBHOpenDFSync(ParmBlkPtr paramBlock);
OSErr PBHOpenDFAsync(ParmBlkPtr paramBlock);
OSErr PBHOpenRFSync(ParmBlkPtr paramBlock);
OSErr PBHOpenRFAsync(ParmBlkPtr paramBlock);
OSErr PBHCreateSync(ParmBlkPtr paramBlock);
OSErr PBHCreateAsync(ParmBlkPtr paramBlock);
OSErr PBHDeleteSync(ParmBlkPtr paramBlock);
OSErr PBHDeleteAsync(ParmBlkPtr paramBlock);
OSErr PBHRenameSync(ParmBlkPtr paramBlock);
OSErr PBHRenameAsync(ParmBlkPtr paramBlock);

/* Working Directory Parameter Block Operations */
OSErr PBOpenWDSync(WDPBPtr paramBlock);
OSErr PBOpenWDAsync(WDPBPtr paramBlock);
OSErr PBCloseWDSync(WDPBPtr paramBlock);
OSErr PBCloseWDAsync(WDPBPtr paramBlock);
OSErr PBGetWDInfoSync(WDPBPtr paramBlock);
OSErr PBGetWDInfoAsync(WDPBPtr paramBlock);

/* File Control Block Operations */
OSErr PBGetFCBInfoSync(FCBPBPtr paramBlock);
OSErr PBGetFCBInfoAsync(FCBPBPtr paramBlock);

/* Utility Functions */
OSErr FM_GetVolumeFromRefNum(VolumeRefNum vRefNum, VCB** vcb);
OSErr FM_GetFCBFromRefNum(FileRefNum refNum, FCB** fcb);
bool FM_IsDirectory(const FSSpec* spec);
OSErr FM_ConvertPath(const char* unixPath, FSSpec* spec);
OSErr FM_ConvertToUnixPath(const FSSpec* spec, char* unixPath, size_t maxLen);

/* Process Manager Integration */
OSErr FM_SetProcessOwner(FileRefNum refNum, uint32_t processID);
OSErr FM_ReleaseProcessFiles(uint32_t processID);
OSErr FM_YieldToProcess(void);

/* Cache Management */
OSErr FM_FlushCache(void);
OSErr FM_SetCacheSize(uint32_t cacheSize);

/* Compatibility Functions */
OSErr FM_RegisterExternalFS(uint16_t fsID, void* dispatcher);
OSErr FM_UnregisterExternalFS(uint16_t fsID);

/* Debug and Statistics */
void FM_GetStatistics(void* stats);
void FM_DumpVolumeInfo(VolumeRefNum vRefNum);
void FM_DumpOpenFiles(void);

#ifdef __cplusplus
}
#endif

#endif /* __FILEMANAGER_H__ */