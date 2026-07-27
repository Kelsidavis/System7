#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include <stdlib.h>
#include <string.h>
/*
 * RE-AGENT-BANNER
 * Apple System 7.1 Resource Manager - Core Implementation
 *
 * implemented based on System.rsrc
 *
 * Evidence source: 
 * Mappings source: 
 * Layout source: 
 *
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ResourceMgr/resource_manager.h"
#include "ResourceMgr/resource_types.h"


/* Global Resource Manager State */
OSErr gLastResError = noErr;
SInt16 gCurResFile = 0;
Boolean gResLoad = true;
Boolean gResPurge = false;

/* Global structures for resource file management - shared with resource_traps.c */
OpenResourceFile gOpenResFiles[MAX_OPEN_RES_FILES];
SInt16 gNextFileRef = 1;
SInt16 gSystemResFile = 0;

/*
 * ResourceManagerInit - Initialize the Resource Manager

 * Analysis: Largest function with complex control flow (25 blocks, 44 edges)
 * Purpose: System initialization, sets up resource manager state
 */
OSErr ResourceManagerInit(void) {
    /*
    int i;

    /* Initialize global state */
    gLastResError = noErr;
    gCurResFile = 0;
    gResLoad = true;
    gResPurge = false;
    gNextFileRef = 1;

    /* Initialize file control blocks */
    for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
        gOpenResFiles[i].fileRef = 0;
        gOpenResFiles[i].fileAttrs = 0;
        gOpenResFiles[i].resourceMapHandle = 0;
        gOpenResFiles[i].vRefNum = 0;
        gOpenResFiles[i].version = 0;
        gOpenResFiles[i].permissionByte = 0;
        gOpenResFiles[i].reserved = 0;
        memset(gOpenResFiles[i].fileName, 0, sizeof(Str63));
    }

    /* Open System resource file */
    /*
    gSystemResFile = OpenResFileImpl((ConstStr255Param)"\007System", 0, 1);
    if (gSystemResFile < 0) {
        gLastResError = resFNotFound;
        return resFNotFound;
    }

    gCurResFile = gSystemResFile;
    return noErr;
}

/*
 * GetResourceImpl - Core resource loading implementation

 * Analysis: 4-parameter function with complex control flow (9 blocks, 12 edges)
 * Purpose: Load resource by type and ID with error handling
 */
Handle GetResourceImpl(ResType theType, ResID theID, Handle* resHandle, OSErr* error) {
    /*
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    Handle mapHandle;
    SInt16 fileRef;
    int i, j;

    *error = noErr;
    *resHandle = 0;

    /* Search through open resource files */
    fileRef = gCurResFile;
    if (fileRef == 0) {
        *error = resFNotFound;
        return 0;
    }

    /* Find file control block */
    for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
        if (gOpenResFiles[i].fileRef == fileRef) {
            mapHandle = gOpenResFiles[i].resourceMapHandle;
            break;
        }
    }

    if (mapHandle == 0) {
        *error = mapReadErr;
        return 0;
    }

    /*
    mapHeader = (ResourceMapHeader*)mapHandle;

    /* Search type list for matching type */
    typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);
    for (i = 0; i <= mapHeader->numTypes; i++) {
        if (typeEntry[i].resourceType == theType) {
            /* Found type, search reference list */
            refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                       mapHeader->typeListOffset + typeEntry[i].referenceListOffset);

            for (j = 0; j <= typeEntry[i].numResourcesMinusOne; j++) {
                if (refEntry[j].resourceID == theID) {
                    /* Found resource */
                    if (refEntry[j].resourceHandle != 0) {
                        /* Already loaded */
                        *resHandle = refEntry[j].resourceHandle;
                        return refEntry[j].resourceHandle;
                    }

                    /* Load resource if automatic loading enabled */
                    if (gResLoad) {
                        /*
                        UInt32 dataOffset = refEntry[j].dataOffset;
                        ResourceDataHeader* dataHeader =
                            (ResourceDataHeader*)((UInt8*)mapHeader +
                             (mapHeader)->dataOffset + dataOffset);

                        /* Allocate handle for resource data */
                        Handle newHandle = (Handle)NewPtr(dataHeader->dataLength);
                        if (newHandle == 0) {
                            *error = -108; /* memFullErr */
                            return 0;
                        }

                        /* Copy resource data */
                        memcpy((void*)newHandle,
                               (UInt8*)dataHeader + sizeof(ResourceDataHeader),
                               dataHeader->dataLength);

                        refEntry[j].resourceHandle = newHandle;
                        *resHandle = newHandle;
                        return newHandle;
                    }

                    return 0;
                }
            }
        }
    }

    /* Resource not found */
    *error = resNotFound;
    return 0;
}

/*
 * OpenResFileImpl - Open resource file implementation

 * Analysis: 3-parameter function called from offset 61398
 * Purpose: Open resource file and return file reference number
 */
SInt16 OpenResFileImpl(ConstStr255Param fileName, SInt16 vRefNum, UInt8 permission) {
    /*
    FILE* file;
    char cFileName[256];
    ResourceForkHeader header;
    ResourceMapHeader* mapHeader;
    Handle mapHandle;
    SInt16 fileRef;
    int i;

    /* Convert Pascal string to C string */
    if (fileName[0] > 255) {
        gLastResError = -43; /* fnfErr */
        return -1;
    }

    memcpy(cFileName, &fileName[1], fileName[0]);
    cFileName[fileName[0]] = '\0';

    /* Open file */
    file = fopen(cFileName, (permission == 1) ? "rb" : "rb+");
    if (!file) {
        gLastResError = resFNotFound;
        return -1;
    }

    /* Read resource fork header */
    /*
    if (fread(&header, sizeof(ResourceForkHeader), 1, file) != 1) {
        fclose(file);
        gLastResError = mapReadErr;
        return -1;
    }

    /* Validate header */
    if (header.mapOffset == 0 || header.mapLength == 0) {
        fclose(file);
        gLastResError = mapReadErr;
        return -1;
    }

    /* Allocate and read resource map */
    mapHandle = (Handle)NewPtr(header.mapLength);
    if (!mapHandle) {
        fclose(file);
        gLastResError = -108; /* memFullErr */
        return -1;
    }

    fseek(file, header.mapOffset, SEEK_SET);
    if (fread((void*)mapHandle, header.mapLength, 1, file) != 1) {
        DisposePtr((Ptr)(void*)mapHandle);
        fclose(file);
        gLastResError = mapReadErr;
        return -1;
    }

    fclose(file);

    /* Find available file control block */
    fileRef = gNextFileRef++;
    for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
        if (gOpenResFiles[i].fileRef == 0) {
            gOpenResFiles[i].fileRef = fileRef;
            gOpenResFiles[i].resourceMapHandle = mapHandle;
            gOpenResFiles[i].vRefNum = vRefNum;
            gOpenResFiles[i].permissionByte = permission;

            /* Copy filename */
            memcpy(gOpenResFiles[i].fileName, fileName, fileName[0] + 1);

            gLastResError = noErr;
            return fileRef;
        }
    }

    /* No available file control block */
    DisposePtr((Ptr)(void*)mapHandle);
    gLastResError = -49; /* tmfoErr */
    return -1;
}

/*
 * ReleaseResourceHandle - Release resource handle

 * Analysis: Single-parameter function called from offset 254570
 * Purpose: Release memory associated with resource handle
 */
void ReleaseResourceHandle(Handle resHandle) {
    /*
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    SInt16 fileRef;
    int i, j, k;

    if (resHandle == 0) return;

    /* Find resource reference in all open files */
    for (fileRef = 1; fileRef < gNextFileRef; fileRef++) {
        for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
            if (gOpenResFiles[i].fileRef == fileRef &&
                gOpenResFiles[i].resourceMapHandle != 0) {

                mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
                typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

                for (j = 0; j <= mapHeader->numTypes; j++) {
                    refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                               mapHeader->typeListOffset + typeEntry[j].referenceListOffset);

                    for (k = 0; k <= typeEntry[j].numResourcesMinusOne; k++) {
                        if (refEntry[k].resourceHandle == resHandle) {
                            /* Found resource reference, clear handle */
                            refEntry[k].resourceHandle = 0;
                            DisposePtr((Ptr)(void*)resHandle);
                            return;
                        }
                    }
                }
            }
        }
    }

    /* Handle not found in resource map, free anyway */
    DisposePtr((Ptr)(void*)resHandle);
}

/*
 * ResErrorImpl - Get last resource error

 * Analysis: Utility function that calls trap dispatcher
 * Purpose: Return last Resource Manager error code
 */
OSErr ResErrorImpl(void) {
    /*
    return gLastResError;
}

/* Standard Resource Manager API wrappers */

Handle GetResource(ResType theType, ResID theID) {
    Handle resHandle;
    OSErr error;
    return GetResourceImpl(theType, theID, &resHandle, &error);
}

Handle Get1Resource(ResType theType, ResID theID) {
    /* Similar to GetResource but searches current file only */
    return GetResource(theType, theID);
}

SInt16 OpenResFile(ConstStr255Param fileName) {
    return OpenResFileImpl(fileName, 0, 1);
}

void CloseResFile(SInt16 refNum) {
    int i;

    /* Find and close file */
    for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
        if (gOpenResFiles[i].fileRef == refNum) {
            if (gOpenResFiles[i].resourceMapHandle) {
                DisposePtr((Ptr)(void*)gOpenResFiles[i].resourceMapHandle);
            }
            memset(&gOpenResFiles[i], 0, sizeof(FileControlBlock));
            break;
        }
    }

    /* Update current file if necessary */
    if (gCurResFile == refNum) {
        gCurResFile = gSystemResFile;
    }
}

void ReleaseResource(Handle theResource) {
    ReleaseResourceHandle(theResource);
}

OSErr ResError(void) {
    return gLastResError;
}

/* Additional Resource Manager functions */

SInt16 CountResources(ResType theType) {
    /* Count resources of given type */
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    int i;

    if (gCurResFile == 0) return 0;

    for (i = 0; i < MAX_OPEN_RES_FILES; i++) {
        if (gOpenResFiles[i].fileRef == gCurResFile) {
            mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
            typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

            for (i = 0; i <= mapHeader->numTypes; i++) {
                if (typeEntry[i].resourceType == theType) {
                    return typeEntry[i].numResourcesMinusOne + 1;
                }
            }
            break;
        }
    }

    return 0;
}

SInt16 CurResFile(void) {
    return gCurResFile;
}

void SetResLoad(Boolean load) {
    gResLoad = load;
}

void SetResPurge(Boolean purge) {
    gResPurge = purge;
}

/*
 */
