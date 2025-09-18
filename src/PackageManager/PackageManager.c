/*
 * PackageManager.c - System 7.1 Package Manager Implementation
 *
 * Implements the Package Manager which loads and dispatches PACK resources.
 * This provides a plugin-like architecture for system services.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "PackageManager/PackageManager.h"
#include "PackageManager/PackageManager_HAL.h"
#include <string.h>
#include <stdio.h>

/* Package Manager globals */
static struct {
    Boolean         initialized;
    PackageEntry    packages[16];      /* 16 possible packages (0-15) */
    short           initFlags;
    OSErr          lastError;
    long           totalMemory;        /* Total memory used by packages */
} gPMState = {0};

/* Package names for debugging */
static const char* kPackageNames[16] = {
    "List Manager",         /* PACK 0 */
    "Reserved",            /* PACK 1 */
    "Disk Init",           /* PACK 2 */
    "Standard File",       /* PACK 3 */
    "SANE (Float)",        /* PACK 4 */
    "ELEMS (Math)",        /* PACK 5 */
    "Intl Utils",          /* PACK 6 */
    "Binary-Decimal",      /* PACK 7 */
    "Apple Events",        /* PACK 8 */
    "PPC Toolbox",         /* PACK 9 */
    "Edition Manager",     /* PACK 10 */
    "Color Picker",        /* PACK 11 */
    "Sound Manager",       /* PACK 12 */
    "Reserved",            /* PACK 13 */
    "Help Manager",        /* PACK 14 */
    "Picture Utils"        /* PACK 15 */
};

/* Built-in package dispatchers */
static OSErr BuiltInListManager(short selector, void* params);
static OSErr BuiltInStandardFile(short selector, void* params);
static OSErr BuiltInSANE(short selector, void* params);
static OSErr BuiltInIntlUtils(short selector, void* params);
static OSErr BuiltInColorPicker(short selector, void* params);

/* Initialize Package Manager */
OSErr InitPacks(void) {
    int i;

    if (gPMState.initialized) {
        return noErr;
    }

    /* Initialize HAL layer */
    OSErr err = PackageManager_HAL_Init();
    if (err != noErr) {
        gPMState.lastError = err;
        return err;
    }

    /* Clear package table */
    memset(&gPMState.packages, 0, sizeof(gPMState.packages));

    /* Initialize each package entry */
    for (i = 0; i < 16; i++) {
        gPMState.packages[i].packageID = i;
        gPMState.packages[i].packageHandle = NULL;
        gPMState.packages[i].entryPoint = NULL;
        gPMState.packages[i].refCount = 0;
        gPMState.packages[i].flags = 0;
        gPMState.packages[i].dispatchProc = NULL;
    }

    /* Register built-in package implementations */
    RegisterPackage(kListManagerPack, (ProcPtr)BuiltInListManager);
    RegisterPackage(kStandardFilePack, (ProcPtr)BuiltInStandardFile);
    RegisterPackage(kSANEPack, (ProcPtr)BuiltInSANE);
    RegisterPackage(kIntlUtilsPack, (ProcPtr)BuiltInIntlUtils);
    RegisterPackage(kColorPickerPack, (ProcPtr)BuiltInColorPicker);

    gPMState.initFlags = 0x0001;
    gPMState.initialized = true;
    gPMState.lastError = noErr;
    gPMState.totalMemory = 0;

    return noErr;
}

/* Initialize all standard packages */
OSErr InitAllPacks(void) {
    OSErr err = noErr;
    int i;

    /* Initialize Package Manager if needed */
    if (!gPMState.initialized) {
        err = InitPacks();
        if (err != noErr) return err;
    }

    /* Load standard packages */
    short standardPacks[] = {
        kListManagerPack,
        kStandardFilePack,
        kSANEPack,
        kElemsPack,
        kIntlUtilsPack,
        kBinaryDecimalPack,
        kColorPickerPack
    };

    for (i = 0; i < sizeof(standardPacks)/sizeof(short); i++) {
        LoadPackage(standardPacks[i]);
        /* Ignore errors - package may not be available */
    }

    return noErr;
}

/* Shutdown Package Manager */
void ShutdownPacks(void) {
    int i;

    if (!gPMState.initialized) {
        return;
    }

    /* Unload all packages */
    for (i = 0; i < 16; i++) {
        if (gPMState.packages[i].flags & kPackLoaded) {
            UnloadPackage(i);
        }
    }

    /* Cleanup HAL layer */
    PackageManager_HAL_Cleanup();

    gPMState.initialized = false;
}

/* Load a package */
OSErr LoadPackage(short packID) {
    PackageEntry *entry;
    OSErr err;

    if (packID < 0 || packID > 15) {
        return packNotFound;
    }

    entry = &gPMState.packages[packID];

    /* Check if already loaded */
    if (entry->flags & kPackLoaded) {
        entry->refCount++;
        return noErr;
    }

    /* Try to load from HAL (resource file, etc.) */
    Handle packHandle = NULL;
    err = PackageManager_HAL_LoadPackage(packID, &packHandle);

    if (err == noErr && packHandle) {
        /* Lock package in memory */
        HLock(packHandle);

        entry->packageHandle = packHandle;
        entry->entryPoint = *packHandle;
        entry->refCount = 1;
        entry->flags |= kPackLoaded;

        /* Update memory usage */
        gPMState.totalMemory += GetHandleSize(packHandle);

        /* Initialize the package */
        InitPackage(packID);
    } else if (entry->dispatchProc) {
        /* Use built-in implementation */
        entry->flags |= (kPackLoaded | kPackInit);
        entry->refCount = 1;
        err = noErr;
    } else {
        err = packNotFound;
    }

    gPMState.lastError = err;
    return err;
}

/* Unload a package */
OSErr UnloadPackage(short packID) {
    PackageEntry *entry;

    if (packID < 0 || packID > 15) {
        return packNotFound;
    }

    entry = &gPMState.packages[packID];

    /* Check if loaded */
    if (!(entry->flags & kPackLoaded)) {
        return packNotFound;
    }

    /* Check if system package */
    if (entry->flags & kPackSystem) {
        return packNotInit;  /* Can't unload system packages */
    }

    /* Decrement reference count */
    if (entry->refCount > 0) {
        entry->refCount--;
    }

    /* Unload if no more references */
    if (entry->refCount <= 0) {
        ReleasePackage(packID);

        if (entry->packageHandle) {
            /* Update memory usage */
            gPMState.totalMemory -= GetHandleSize(entry->packageHandle);

            HUnlock(entry->packageHandle);
            DisposeHandle(entry->packageHandle);
            entry->packageHandle = NULL;
        }

        entry->entryPoint = NULL;
        entry->refCount = 0;
        entry->flags &= ~(kPackLoaded | kPackInit);
    }

    return noErr;
}

/* Reload a package */
OSErr ReloadPackage(short packID) {
    OSErr err;

    /* Unload if loaded */
    if (IsPackageLoaded(packID)) {
        err = UnloadPackage(packID);
        if (err != noErr) return err;
    }

    /* Load again */
    return LoadPackage(packID);
}

/* Check if package is loaded */
Boolean IsPackageLoaded(short packID) {
    if (packID < 0 || packID > 15) {
        return false;
    }
    return (gPMState.packages[packID].flags & kPackLoaded) != 0;
}

/* Get package handle */
Handle GetPackageHandle(short packID) {
    if (packID < 0 || packID > 15) {
        return NULL;
    }
    return gPMState.packages[packID].packageHandle;
}

/* Get package information */
OSErr GetPackageInfo(short packID, PackageHeader* info) {
    PackageEntry *entry;
    PackageHeader *header;

    if (packID < 0 || packID > 15 || !info) {
        return packNotFound;
    }

    entry = &gPMState.packages[packID];
    if (!(entry->flags & kPackLoaded)) {
        return packNotFound;
    }

    if (entry->packageHandle) {
        /* Get header from package resource */
        header = (PackageHeader*)*(entry->packageHandle);
        *info = *header;
    } else {
        /* Fill in info for built-in package */
        info->version = 1;
        info->packageID = packID;
        info->entryPoint = 0;
        info->initProc = 0;
        info->reserved1 = 0;
        info->reserved2 = 0;
    }

    return noErr;
}

/* Get package version */
OSErr GetPackageVersion(short packID, short* version) {
    PackageHeader info;
    OSErr err;

    if (!version) return paramErr;

    err = GetPackageInfo(packID, &info);
    if (err == noErr) {
        *version = info.version;
    }

    return err;
}

/* Get package reference count */
short GetPackageRefCount(short packID) {
    if (packID < 0 || packID > 15) {
        return 0;
    }
    return gPMState.packages[packID].refCount;
}

/* Call package function */
OSErr CallPackage(short packID, short selector, void* params) {
    PackageEntry *entry;
    OSErr err;

    if (packID < 0 || packID > 15) {
        return packNotFound;
    }

    entry = &gPMState.packages[packID];

    /* Load package if needed */
    if (!(entry->flags & kPackLoaded)) {
        err = LoadPackage(packID);
        if (err != noErr) {
            return err;
        }
    }

    /* Dispatch to package */
    if (entry->dispatchProc) {
        /* Use registered dispatcher */
        typedef OSErr (*DispatchFunc)(short, void*);
        DispatchFunc dispatcher = (DispatchFunc)entry->dispatchProc;
        return dispatcher(selector, params);
    } else if (entry->entryPoint) {
        /* Call package entry point */
        return PackageManager_HAL_CallPackage(packID, selector,
                                             entry->entryPoint, params);
    }

    return packNotFound;
}

/* Call package with result */
OSErr CallPackageWithResult(short packID, short selector,
                           void* params, void* result) {
    PackageParams pParams;

    pParams.selector = selector;
    pParams.paramPtr = params;
    pParams.paramSize = 0;  /* Size would be determined by selector */
    pParams.resultPtr = result;
    pParams.resultSize = 0;

    return CallPackage(packID, selector, &pParams);
}

/* Package trap dispatchers */
OSErr Pack0(short selector, void* params) {
    return CallPackage(0, selector, params);
}

OSErr Pack1(short selector, void* params) {
    return CallPackage(1, selector, params);
}

OSErr Pack2(short selector, void* params) {
    return CallPackage(2, selector, params);
}

OSErr Pack3(short selector, void* params) {
    return CallPackage(3, selector, params);
}

OSErr Pack4(short selector, void* params) {
    return CallPackage(4, selector, params);
}

OSErr Pack5(short selector, void* params) {
    return CallPackage(5, selector, params);
}

OSErr Pack6(short selector, void* params) {
    return CallPackage(6, selector, params);
}

OSErr Pack7(short selector, void* params) {
    return CallPackage(7, selector, params);
}

OSErr Pack8(short selector, void* params) {
    return CallPackage(8, selector, params);
}

OSErr Pack9(short selector, void* params) {
    return CallPackage(9, selector, params);
}

OSErr Pack10(short selector, void* params) {
    return CallPackage(10, selector, params);
}

OSErr Pack11(short selector, void* params) {
    return CallPackage(11, selector, params);
}

OSErr Pack12(short selector, void* params) {
    return CallPackage(12, selector, params);
}

OSErr Pack13(short selector, void* params) {
    return CallPackage(13, selector, params);
}

OSErr Pack14(short selector, void* params) {
    return CallPackage(14, selector, params);
}

OSErr Pack15(short selector, void* params) {
    return CallPackage(15, selector, params);
}

/* Initialize a loaded package */
OSErr InitPackage(short packID) {
    PackageEntry *entry;

    if (packID < 0 || packID > 15) {
        return packNotFound;
    }

    entry = &gPMState.packages[packID];

    if (!(entry->flags & kPackLoaded)) {
        return packNotInit;
    }

    /* Package-specific initialization */
    /* This would call the package's init proc if it has one */

    entry->flags |= kPackInit;

    return noErr;
}

/* Release package resources */
void ReleasePackage(short packID) {
    if (packID < 0 || packID > 15) {
        return;
    }

    /* Package-specific cleanup */
    /* This would call any cleanup routines */
}

/* Register a package implementation */
OSErr RegisterPackage(short packID, ProcPtr dispatchProc) {
    if (packID < 0 || packID > 15 || !dispatchProc) {
        return paramErr;
    }

    gPMState.packages[packID].dispatchProc = dispatchProc;
    return noErr;
}

/* Unregister a package */
OSErr UnregisterPackage(short packID) {
    if (packID < 0 || packID > 15) {
        return paramErr;
    }

    /* Can't unregister if loaded */
    if (gPMState.packages[packID].flags & kPackLoaded) {
        return packNotInit;
    }

    gPMState.packages[packID].dispatchProc = NULL;
    return noErr;
}

/* Lock package in memory */
void LockPackage(short packID) {
    if (packID < 0 || packID > 15) {
        return;
    }

    gPMState.packages[packID].flags |= kPackLocked;

    if (gPMState.packages[packID].packageHandle) {
        HLock(gPMState.packages[packID].packageHandle);
    }
}

/* Unlock package */
void UnlockPackage(short packID) {
    if (packID < 0 || packID > 15) {
        return;
    }

    gPMState.packages[packID].flags &= ~kPackLocked;

    if (gPMState.packages[packID].packageHandle) {
        HUnlock(gPMState.packages[packID].packageHandle);
    }
}

/* Purge unused packages */
OSErr PurgePackages(void) {
    int i;
    OSErr err = noErr;

    for (i = 0; i < 16; i++) {
        if ((gPMState.packages[i].flags & kPackLoaded) &&
            !(gPMState.packages[i].flags & (kPackLocked | kPackSystem)) &&
            gPMState.packages[i].refCount == 0) {

            err = UnloadPackage(i);
            if (err != noErr) break;
        }
    }

    return err;
}

/* Get total memory used by packages */
long GetPackagesMemoryUsage(void) {
    return gPMState.totalMemory;
}

/* Built-in List Manager implementation */
static OSErr BuiltInListManager(short selector, void* params) {
    switch (selector) {
        case kLNew:
            /* Create new list */
            return noErr;

        case kLDispose:
            /* Dispose list */
            return noErr;

        case kLSetSelect:
            /* Set selection */
            return noErr;

        case kLGetSelect:
            /* Get selection */
            return noErr;

        default:
            return packSelectorErr;
    }
}

/* Built-in Standard File implementation */
static OSErr BuiltInStandardFile(short selector, void* params) {
    switch (selector) {
        case kSFPutFile:
        case kStandardPutFile:
            /* Show save dialog */
            return PackageManager_HAL_StandardPutFile(params);

        case kSFGetFile:
        case kStandardGetFile:
            /* Show open dialog */
            return PackageManager_HAL_StandardGetFile(params);

        default:
            return packSelectorErr;
    }
}

/* Built-in SANE implementation */
static OSErr BuiltInSANE(short selector, void* params) {
    /* SANE provides floating-point operations */
    /* This would interface with platform math library */
    return PackageManager_HAL_SANEOperation(selector, params);
}

/* Built-in International Utilities */
static OSErr BuiltInIntlUtils(short selector, void* params) {
    /* International utilities for localization */
    return PackageManager_HAL_IntlOperation(selector, params);
}

/* Built-in Color Picker */
static OSErr BuiltInColorPicker(short selector, void* params) {
    /* Show color picker dialog */
    return PackageManager_HAL_ColorPicker(selector, params);
}