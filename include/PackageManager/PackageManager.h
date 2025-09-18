/*
 * PackageManager.h - System 7.1 Package Manager Interface
 *
 * The Package Manager provides a unified interface for loading and
 * dispatching PACK resources. These packages contain code for various
 * system services like Standard File, List Manager, and others.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include "../DeskManager/Types.h"
#include "../Memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Package IDs (PACK resources) */
enum {
    kListManagerPack      = 0,   /* PACK 0: List Manager */
    kUnusedPack1         = 1,   /* PACK 1: Reserved */
    kDiskInitPack        = 2,   /* PACK 2: Disk Initialization */
    kStandardFilePack    = 3,   /* PACK 3: Standard File Package */
    kSANEPack           = 4,   /* PACK 4: SANE (Floating Point) */
    kElemsPack          = 5,   /* PACK 5: Transcendental Functions */
    kIntlUtilsPack      = 6,   /* PACK 6: International Utilities */
    kBinaryDecimalPack  = 7,   /* PACK 7: Binary-Decimal Conversion */
    kAppleEventPack     = 8,   /* PACK 8: Apple Event Manager */
    kPPCToolboxPack     = 9,   /* PACK 9: PPC Toolbox */
    kEditionMgrPack     = 10,  /* PACK 10: Edition Manager */
    kColorPickerPack    = 11,  /* PACK 11: Color Picker */
    kSoundManagerPack   = 12,  /* PACK 12: Sound Manager/StartSound */
    kUnusedPack13       = 13,  /* PACK 13: Reserved */
    kHelpManagerPack    = 14,  /* PACK 14: Help Manager */
    kPictureUtilsPack   = 15   /* PACK 15: Picture Utilities */
};

/* Package Manager error codes */
enum {
    packNotFound        = -2800, /* Package not found */
    packNotInit         = -2801, /* Package not initialized */
    packSelectorErr     = -2802, /* Invalid selector */
    packMemErr          = -2803, /* Memory error loading package */
    packResErr          = -2804  /* Resource error loading package */
};

/* Package flags */
enum {
    kPackLoaded         = 0x0001, /* Package is loaded */
    kPackLocked         = 0x0002, /* Package is locked in memory */
    kPackInit           = 0x0004, /* Package has been initialized */
    kPackSystem         = 0x0008, /* System package (don't unload) */
    kPackPreload        = 0x0010  /* Preload at startup */
};

/* Package selectors for common packages */

/* Standard File Package (PACK 3) selectors */
enum {
    kSFPutFile          = 1,
    kSFGetFile          = 2,
    kSFPPutFile         = 3,
    kSFPGetFile         = 4,
    kStandardPutFile    = 5,
    kStandardGetFile    = 6,
    kCustomPutFile      = 7,
    kCustomGetFile      = 8
};

/* List Manager (PACK 0) selectors */
enum {
    kLNew               = 0,
    kLDispose           = 1,
    kLSetSelect         = 2,
    kLGetSelect         = 3,
    kLAddRow            = 4,
    kLDelRow            = 5,
    kLAddColumn         = 6,
    kLDelColumn         = 7,
    kLSetCell           = 8,
    kLGetCell           = 9,
    kLClick             = 10,
    kLUpdate            = 11,
    kLDraw              = 12,
    kLScroll            = 13
};

/* Package resource header structure */
typedef struct PackageHeader {
    short       version;        /* Package version */
    short       packageID;      /* Package ID (0-15) */
    long        entryPoint;     /* Offset to entry point */
    long        initProc;       /* Offset to init procedure */
    long        reserved1;      /* Reserved */
    long        reserved2;      /* Reserved */
} PackageHeader;

/* Package dispatch table entry */
typedef struct PackageEntry {
    short       packageID;      /* Package ID */
    Handle      packageHandle;  /* Handle to loaded package */
    Ptr         entryPoint;     /* Entry point address */
    short       refCount;       /* Reference count */
    short       flags;          /* Package flags */
    ProcPtr     dispatchProc;   /* Package dispatcher */
} PackageEntry;

/* Package parameter block */
typedef struct PackageParams {
    short       selector;       /* Function selector */
    Ptr         paramPtr;       /* Parameters */
    long        paramSize;      /* Parameter size */
    Ptr         resultPtr;      /* Result buffer */
    long        resultSize;     /* Result buffer size */
} PackageParams;

/* Package Manager Functions */

/* Initialization */
OSErr InitPacks(void);
OSErr InitAllPacks(void);
void ShutdownPacks(void);

/* Package loading and unloading */
OSErr LoadPackage(short packID);
OSErr UnloadPackage(short packID);
OSErr ReloadPackage(short packID);
Boolean IsPackageLoaded(short packID);

/* Package information */
Handle GetPackageHandle(short packID);
OSErr GetPackageInfo(short packID, PackageHeader* info);
OSErr GetPackageVersion(short packID, short* version);
short GetPackageRefCount(short packID);

/* Package execution */
OSErr CallPackage(short packID, short selector, void* params);
OSErr CallPackageWithResult(short packID, short selector,
                           void* params, void* result);

/* Package trap dispatchers (A9E7-A9F6) */
OSErr Pack0(short selector, void* params);  /* List Manager */
OSErr Pack1(short selector, void* params);  /* Reserved */
OSErr Pack2(short selector, void* params);  /* Disk Init */
OSErr Pack3(short selector, void* params);  /* Standard File */
OSErr Pack4(short selector, void* params);  /* SANE */
OSErr Pack5(short selector, void* params);  /* ELEMS */
OSErr Pack6(short selector, void* params);  /* Intl Utils */
OSErr Pack7(short selector, void* params);  /* Binary-Decimal */
OSErr Pack8(short selector, void* params);  /* Apple Events */
OSErr Pack9(short selector, void* params);  /* PPC Toolbox */
OSErr Pack10(short selector, void* params); /* Edition Manager */
OSErr Pack11(short selector, void* params); /* Color Picker */
OSErr Pack12(short selector, void* params); /* Sound Manager */
OSErr Pack13(short selector, void* params); /* Reserved */
OSErr Pack14(short selector, void* params); /* Help Manager */
OSErr Pack15(short selector, void* params); /* Picture Utils */

/* Internal package initialization */
OSErr InitPackage(short packID);
void ReleasePackage(short packID);

/* Package registration (for modern implementation) */
OSErr RegisterPackage(short packID, ProcPtr dispatchProc);
OSErr UnregisterPackage(short packID);

/* Utility functions */
void LockPackage(short packID);
void UnlockPackage(short packID);
OSErr PurgePackages(void);
long GetPackagesMemoryUsage(void);

#ifdef __cplusplus
}
#endif

#endif /* PACKAGEMANAGER_H */