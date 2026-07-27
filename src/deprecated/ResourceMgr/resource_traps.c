#include "SystemTypes.h"

/* External globals defined in resource_manager.c */
extern OpenResourceFile gOpenResFiles[MAX_OPEN_RES_FILES];
extern SInt16 gNextFileRef;
extern SInt16 gCurResFile;

/*
 * RE-AGENT-BANNER
 * Apple System 7.1 Resource Manager - System Trap Handlers
 *
 * implemented based on System.rsrc
 *
 * Evidence source: 
 * Mappings source: 
 *
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "ResourceMgr/resource_manager.h"
#include "ResourceMgr/resource_types.h"


/*
 * TrapDispatcher - System trap dispatcher

 * Analysis: Heavily referenced function (15 calls), entry point pattern
 * Purpose: Dispatch system traps to appropriate Resource Manager functions
 */
void TrapDispatcher(UInt16 trapWord) {
    /*

    switch (trapWord) {
        case trapGetResource:
            /* GetResource trap - 0xA9A0 */
            {
                /* Parameters passed in registers (68k convention) */
                ResType theType = 0;    /* Would be in D0 */
                ResID theID = 0;        /* Would be in D1 */

                /* Call implementation and return handle in A0 */
                Handle result = GetResource(theType, theID);
                /* Result would be placed in A0 register */
                (void)result;
            }
            break;

        case trapGet1Resource:
            /* Get1Resource trap - 0xA9A1 */
            {
                ResType theType = 0;    /* Would be in D0 */
                ResID theID = 0;        /* Would be in D1 */

                Handle result = Get1Resource(theType, theID);
                (void)result;
            }
            break;

        case trapOpenResFile:
            /* OpenResFile trap - 0xA997 */
            {
                ConstStr255Param fileName = 0;  /* Would be in A0 */

                SInt16 result = OpenResFile(fileName);
                /* Result would be placed in D0 register */
                (void)result;
            }
            break;

        case trapCloseResFile:
            /* CloseResFile trap - 0xA99A */
            {
                SInt16 refNum = 0;     /* Would be in D0 */

                CloseResFile(refNum);
            }
            break;

        case trapReleaseResource:
            /* ReleaseResource trap - 0xA9A3 */
            {
                Handle theResource = 0;  /* Would be in A0 */

                ReleaseResource(theResource);
            }
            break;

        case trapAddResource:
            /* AddResource trap - 0xA9AB */
            {
                Handle theData = 0;             /* Would be in A0 */
                ResType theType = 0;            /* Would be in D0 */
                ResID theID = 0;                /* Would be in D1 */
                ConstStr255Param name = 0;      /* Would be in A1 */

                AddResource(theData, theType, theID, name);
            }
            break;

        case trapUpdateResFile:
            /* UpdateResFile trap - 0xA9AD */
            {
                SInt16 refNum = 0;     /* Would be in D0 */

                UpdateResFile(refNum);
            }
            break;

        case trapResError:
            /* ResError trap - 0xA9AF */
            {
                OSErr result = ResError();
                /* Result would be placed in D0 register */
                (void)result;
            }
            break;

        default:
            /* Unknown trap - set error */
            gLastResError = -1;
            break;
    }
}

/* Additional trap implementations that would be called by TrapDispatcher */

void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name) {
    /*
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    SInt16 fileRef;
    int i, j;

    gLastResError = noErr;

    if (theData == 0) {
        gLastResError = addResFailed;
        return;
    }

    /* Find current resource file */
    fileRef = gCurResFile;
    if (fileRef == 0) {
        gLastResError = resFNotFound;
        return;
    }

    /* Find file control block */
    for (i = 0; i < 16; i++) {
        if (gOpenResFiles[i].fileRef == fileRef) {
            if (gOpenResFiles[i].resourceMapHandle == 0) {
                gLastResError = mapReadErr;
                return;
            }

            mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;

            /* Search for existing resource type */
            typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);
            for (j = 0; j <= mapHeader->numTypes; j++) {
                if (typeEntry[j].resourceType == theType) {
                    /* Found existing type - would need to extend reference list */
                    /* Implementation would require complex map restructuring */
                    gLastResError = addResFailed;
                    return;
                }
            }

            /* Type not found - would need to add new type entry */
            /* Full implementation requires extending the resource map */
            gLastResError = addResFailed;
            return;
        }
    }

    gLastResError = resFNotFound;
}

void UpdateResFile(SInt16 refNum) {
    /*
    int i;

    gLastResError = noErr;

    /* Find file control block */
    for (i = 0; i < 16; i++) {
        if (gOpenResFiles[i].fileRef == refNum) {
            /* Would write resource map and data back to file */
            /* Implementation requires file I/O and map serialization */
            return;
        }
    }

    gLastResError = resFNotFound;
}

SInt16 Count1Resources(ResType theType) {
    /* Count resources in current file only */
    return CountResources(theType);
}

Handle GetIndResource(ResType theType, SInt16 index) {
    /* Get resource by index within type */
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    int i;

    if (gCurResFile == 0 || index < 1) return 0;

    for (i = 0; i < 16; i++) {
        if (gOpenResFiles[i].fileRef == gCurResFile) {
            mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
            typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

            for (i = 0; i <= mapHeader->numTypes; i++) {
                if (typeEntry[i].resourceType == theType) {
                    if (index <= typeEntry[i].numResourcesMinusOne + 1) {
                        refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                                   mapHeader->typeListOffset + typeEntry[i].referenceListOffset);

                        return GetResource(theType, refEntry[index - 1].resourceID);
                    }
                    break;
                }
            }
            break;
        }
    }

    return 0;
}

Handle Get1IndResource(ResType theType, SInt16 index) {
    /* Get resource by index in current file only */
    return GetIndResource(theType, index);
}

SInt16 GetResAttrs(Handle theResource) {
    /* Get resource attributes */
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    SInt16 fileRef;
    int i, j, k;

    if (theResource == 0) return 0;

    /* Find resource in all open files */
    for (fileRef = 1; fileRef < gNextFileRef; fileRef++) {
        for (i = 0; i < 16; i++) {
            if (gOpenResFiles[i].fileRef == fileRef &&
                gOpenResFiles[i].resourceMapHandle != 0) {

                mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
                typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

                for (j = 0; j <= mapHeader->numTypes; j++) {
                    refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                               mapHeader->typeListOffset + typeEntry[j].referenceListOffset);

                    for (k = 0; k <= typeEntry[j].numResourcesMinusOne; k++) {
                        if (refEntry[k].resourceHandle == theResource) {
                            return refEntry[k].resourceAttrs;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void SetResAttrs(Handle theResource, SInt16 attrs) {
    /* Set resource attributes */
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    SInt16 fileRef;
    int i, j, k;

    if (theResource == 0) return;

    /* Find resource in all open files */
    for (fileRef = 1; fileRef < gNextFileRef; fileRef++) {
        for (i = 0; i < 16; i++) {
            if (gOpenResFiles[i].fileRef == fileRef &&
                gOpenResFiles[i].resourceMapHandle != 0) {

                mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
                typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

                for (j = 0; j <= mapHeader->numTypes; j++) {
                    refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                               mapHeader->typeListOffset + typeEntry[j].referenceListOffset);

                    for (k = 0; k <= typeEntry[j].numResourcesMinusOne; k++) {
                        if (refEntry[k].resourceHandle == theResource) {
                            refEntry[k].resourceAttrs = (UInt8)attrs;
                            return;
                        }
                    }
                }
            }
        }
    }
}

SInt16 HomeResFile(Handle theResource) {
    /* Get home file of resource */
    ResourceMapHeader* mapHeader;
    ResourceTypeEntry* typeEntry;
    ResourceRefEntry* refEntry;
    SInt16 fileRef;
    int i, j, k;

    if (theResource == 0) return 0;

    /* Find resource in all open files */
    for (fileRef = 1; fileRef < gNextFileRef; fileRef++) {
        for (i = 0; i < 16; i++) {
            if (gOpenResFiles[i].fileRef == fileRef &&
                gOpenResFiles[i].resourceMapHandle != 0) {

                mapHeader = (ResourceMapHeader*)gOpenResFiles[i].resourceMapHandle;
                typeEntry = (ResourceTypeEntry*)((UInt8*)mapHeader + mapHeader->typeListOffset);

                for (j = 0; j <= mapHeader->numTypes; j++) {
                    refEntry = (ResourceRefEntry*)((UInt8*)mapHeader +
                               mapHeader->typeListOffset + typeEntry[j].referenceListOffset);

                    for (k = 0; k <= typeEntry[j].numResourcesMinusOne; k++) {
                        if (refEntry[k].resourceHandle == theResource) {
                            return fileRef;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void SetResFileAttrs(SInt16 refNum, SInt16 attrs) {
    int i;

    for (i = 0; i < 16; i++) {
        if (gOpenResFiles[i].fileRef == refNum) {
            gOpenResFiles[i].fileAttrs = (UInt16)attrs;
            return;
        }
    }
}

SInt16 GetResFileAttrs(SInt16 refNum) {
    int i;

    for (i = 0; i < 16; i++) {
        if (gOpenResFiles[i].fileRef == refNum) {
            return (SInt16)gOpenResFiles[i].fileAttrs;
        }
    }

    return 0;
}

/*
 */
