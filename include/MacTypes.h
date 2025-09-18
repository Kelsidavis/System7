/*
 * System 7.1 Portable - Common Mac OS Types and Error Codes
 *
 * This header defines common types and error codes used throughout
 * the Mac OS 7.1 portable implementation to avoid duplication and
 * ensure consistency.
 *
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#ifndef MACTYPES_H
#define MACTYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Basic Mac OS Types
 * ============================================================================ */

/* Classic Mac OS types */
typedef int16_t         OSErr;
typedef uint8_t         Boolean;
typedef uint8_t         Byte;
typedef uint8_t*        Ptr;
typedef Ptr*           Handle;
typedef char*          StringPtr;
typedef const char*    ConstStr255Param;
typedef uint32_t       OSType;
typedef uint32_t       ResType;
typedef int16_t        ResID;
typedef int16_t        ResAttributes;

/* Pascal string type (length byte followed by characters) */
typedef unsigned char   Str255[256];
typedef unsigned char   Str63[64];
typedef unsigned char   Str31[32];
typedef unsigned char   Str27[28];
typedef unsigned char   Str15[16];

/* Fixed-point types */
typedef int32_t         Fixed;         /* 16.16 fixed point */
typedef int16_t         Fract;         /* 2.14 fixed point */

/* Rectangle and Point */
typedef struct Point {
    int16_t v;  /* vertical */
    int16_t h;  /* horizontal */
} Point;

typedef struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

/* Memory sizes */
typedef uint32_t        Size;
typedef int32_t         SignedSize;

/* Reference numbers */
typedef int16_t         RefNum;
typedef int16_t         VRefNum;       /* Volume reference number */
typedef int16_t         DrvRefNum;     /* Driver reference number */

/* ============================================================================
 * Common Mac OS Error Codes
 * ============================================================================ */

typedef enum {
    /* No error */
    noErr               = 0,

    /* General System Errors */
    qErr                = -1,      /* Queue element not found */
    vTypErr             = -2,      /* Invalid queue element */
    corErr              = -3,      /* Core routine number out of range */
    unimpErr            = -4,      /* Unimplemented core routine */
    SlpTypeErr          = -5,      /* Invalid queue element */
    seNoDB              = -8,      /* No debugger installed */
    controlErr          = -17,     /* I/O System Errors */
    statusErr           = -18,     /* I/O System Errors */
    readErr             = -19,     /* I/O System Errors */
    writErr             = -20,     /* I/O System Errors */
    badUnitErr          = -21,     /* I/O System Errors */
    unitEmptyErr        = -22,     /* I/O System Errors */
    openErr             = -23,     /* I/O System Errors */
    closErr             = -24,     /* I/O System Errors */
    dRemovErr           = -25,     /* Tried to remove an open driver */

    /* File Manager Errors */
    dirFulErr           = -33,     /* Directory full */
    dskFulErr           = -34,     /* Disk full */
    nsvErr              = -35,     /* No such volume */
    ioErr               = -36,     /* I/O error */
    bdNamErr            = -37,     /* Bad name */
    fnOpnErr            = -38,     /* File not open */
    eofErr              = -39,     /* End of file */
    posErr              = -40,     /* Tried to position before start of file */
    mFulErr             = -41,     /* Memory full (open) or file won't fit (load) */
    tmfoErr             = -42,     /* Too many files open */
    fnfErr              = -43,     /* File not found */
    wPrErr              = -44,     /* Disk is write-protected */
    fLckdErr            = -45,     /* File is locked */
    vLckdErr            = -46,     /* Volume is locked */
    fBsyErr             = -47,     /* File is busy */
    dupFNErr            = -48,     /* Duplicate filename */
    opWrErr             = -49,     /* File already open for writing */
    paramErr            = -50,     /* Error in parameter list */
    rfNumErr            = -51,     /* Reference number error */
    gfpErr              = -52,     /* Get file position error */
    volOffLinErr        = -53,     /* Volume is offline */
    permErr             = -54,     /* Permission error */
    volOnLinErr         = -55,     /* Volume already online */
    nsDrvErr            = -56,     /* No such drive */
    noMacDskErr         = -57,     /* Not a Mac disk */
    extFSErr            = -58,     /* External file system error */
    fsRnErr             = -59,     /* File system internal error */
    badMDBErr           = -60,     /* Bad master directory block */
    wrPermErr           = -61,     /* Write permission error */

    /* Resource Manager Errors */
    mapReadErr          = -199,    /* Map inconsistent with operation */
    resNotFound         = -192,    /* Resource not found */
    resFNotFound        = -193,    /* Resource file not found */
    addResFailed        = -194,    /* AddResource failed */
    rmvResFailed        = -196,    /* RemoveResource failed */
    resAttrErr          = -198,    /* Attribute inconsistent with operation */
    CantDecompress      = -186,    /* Resource couldn't be decompressed */

    /* Memory Manager Errors */
    memROZErr           = -99,     /* Operation on read-only zone */
    memFullErr          = -108,    /* Not enough memory */
    nilHandleErr        = -109,    /* NIL handle */
    memWZErr            = -111,    /* Attempt to operate on free block */
    memPurErr           = -112,    /* Attempt to purge locked block */
    memAdrErr           = -110,    /* Address was odd or out of range */
    memAZErr            = -113,    /* Address in zone check failed */
    memPCErr            = -114,    /* Pointer check failed */
    memBCErr            = -115,    /* Block check failed */
    memSCErr            = -116,    /* Size check failed */
    memLockedErr        = -117,    /* Trying to move a locked block */

    /* B-Tree Manager Errors */
    btNoSpace           = -413,    /* Insufficient disk space */
    btDupRecErr         = -414,    /* Record already exists */
    btRecNotFnd         = -415,    /* Record not found */
    btKeyLenErr         = -416,    /* Key length too great or equal to zero */
    btKeyAttrErr        = -417,    /* Invalid key attributes */
    btbadNode           = -418     /* Bad node detected */
} MacOSErr;

/* ============================================================================
 * Common Constants
 * ============================================================================ */

/* Boolean values */
#ifndef true
#define true    1
#endif

#ifndef false
#define false   0
#endif

/* NULL definitions */
#ifndef NULL
#define NULL    ((void*)0)
#endif

#ifndef nil
#define nil     NULL
#endif

/* File permissions */
#define fsRdPerm    1       /* Read permission */
#define fsWrPerm    2       /* Write permission */
#define fsRdWrPerm  3       /* Read/write permission */

/* File positioning modes */
#define fsAtMark    0       /* At current mark */
#define fsFromStart 1       /* From beginning of file */
#define fsFromLEOF  2       /* From logical end of file */
#define fsFromMark  3       /* From current mark */

/* Resource attributes */
#define resSysHeap      64  /* System heap */
#define resPurgeable    32  /* Purgeable */
#define resLocked       16  /* Locked */
#define resProtected    8   /* Protected */
#define resPreload      4   /* Preload */
#define resChanged      2   /* Changed */

/* B-Tree Control Block Flags */
#define BTCDirty        0x0001  /* B-Tree needs to be written */
#define BTCWriteReq     0x0002  /* Write request pending */

#ifdef __cplusplus
}
#endif

#endif /* MACTYPES_H */