/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Alias Manager Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
 *
 * Evidence sources:
 * - String analysis: "the original item,## could not be found%The alias"
 * - String analysis: "this item is really not an alias (oops!)"
 * - Alias type constants from Finder.h interface
 * - String analysis: "Find Original" functionality
 *
 * This module handles alias file creation, resolution, and management.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "Finder/finder.h"
#include "Finder/finder_types.h"
#include "FileMgr/file_manager.h"
/* Use local headers instead of system headers */
#include "MemoryMgr/memory_manager_types.h"
#include "MemoryMgr/MemoryManager.h"
#include "ResourceManager.h"
#include "Finder/FinderLogging.h"
#include "sys71_stubs.h"
/* Note: Aliases.h may not exist yet */


/* Alias Manager Constants */
#define kAliasResourceType      'alis'
#define kAliasResourceID        0
#define kAliasMinimumSize       50          /* Minimum valid alias size */
#define kAliasMaximumSize       32767       /* Maximum alias size */

/* Global Alias State */
static Handle gAliasTypeTable = nil;        /* Alias type mapping table */

/* Forward Declarations */
static OSErr LoadAliasTypeTable(void);
static OSErr ValidateAliasFile(FSSpec *aliasFile);
static OSErr CreateAliasResource(FSSpec *target, FSSpec *aliasFile);
static OSErr UpdateAliasFile(FSSpec *aliasFile, FSSpec *newTarget);
static Boolean IsValidAliasRecord(AliasRecord *record);

/*
 * ResolveAlias - Resolves an alias to its target item

 */
OSErr ResolveAlias(FSSpec *alias, FSSpec *target, Boolean *wasChanged)
{
    OSErr err = noErr;
    AliasHandle aliasHandle = nil;
    short aliasRefNum;
    Boolean targetChanged = false;
    Boolean wasAliasFile = false;

    if (alias == nil || target == nil) {
        return paramErr;
    }
    err = LoadAliasTypeTable();
    if (err != noErr) {
        return err;
    }

    /* Validate that this is actually an alias file */
    err = ValidateAliasFile(alias);
    if (err != noErr) {
        /* ShowErrorDialog("\pThis item is really not an alias. The problem has now been corrected.", err); */
        return err;
    }

    /* Open the alias file and read the alias resource */
    aliasRefNum = FSpOpenResFile(alias, fsRdPerm);
    if (aliasRefNum == -1) {
        return ResError();
    }

    /* Get the alias resource */
    aliasHandle = (AliasHandle)Get1Resource(kAliasResourceType, kAliasResourceID);
    if (aliasHandle == nil) {
        CloseResFile(aliasRefNum);
        return resNotFound;
    }

    /* Use Alias Manager to resolve the alias */
    err = ResolveAliasFile(alias, target, &targetChanged, &wasAliasFile);

    if (err == noErr) {
        /* target is now populated by ResolveAliasFile */

        if (wasChanged != nil) {
            *wasChanged = targetChanged;
        }

        /* If alias was updated, save the changes */
        if (targetChanged) {
            err = UpdateAliasFile(alias, target);
        }
    } else {
        /* ShowErrorDialog("\pThe alias could not be resolved because the original item could not be found.", err); */
    }

    /* Clean up */
    ReleaseResource((Handle)aliasHandle);
    CloseResFile(aliasRefNum);

    return err;
}

/*
 * FixBrokenAlias - Repairs or identifies broken aliases

 */
OSErr FixBrokenAlias(FSSpec *alias)
{
    OSErr err;
    CInfoPBRec pb;
    FInfo finderInfo;

    if (alias == nil) {
        return paramErr;
    }

    /* Get current Finder info */
    pb.ioCompletion = nil;
    pb.ioNamePtr = alias->name;
    pb.ioVRefNum = alias->vRefNum;
    pb.u.hFileInfo.ioDirID = alias->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    /* Copy Finder info */
    BlockMoveData(&pb.u.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FInfo));

    /* Check if alias flag is set but file doesn't have alias resource */
    if (finderInfo.fdFlags & kIsAlias) {
        err = ValidateAliasFile(alias);
        if (err != noErr) {
            /* Remove alias flag - it's not really an alias */
            finderInfo.fdFlags &= ~kIsAlias;

            /* Update Finder info */
            BlockMoveData(&finderInfo, &pb.u.hFileInfo.ioFlFndrInfo, sizeof(FInfo));
            pb.u.hFileInfo.ioDirID = alias->parID;
            err = PBSetCatInfoSync(&pb);

            if (err == noErr) {
                /* ShowErrorDialog("\pThe problem has now been corrected. Please try again.", noErr); */
            }
        }
    }

    return err;
}

/*
 * CreateAlias - Creates a new alias file

 */
OSErr CreateAlias(FSSpec *target, FSSpec *aliasFile)
{
    OSErr err;
    AliasHandle aliasHandle = nil;
    /* short aliasRefNum; */ /* TODO: Resource file reference for alias storage */
    CInfoPBRec pb;
    FInfo finderInfo;

    if (target == nil || aliasFile == nil) {
        return paramErr;
    }

    /* Check that target exists */
    err = FSMakeFSSpec(target->vRefNum, target->parID, target->name, target);
    if (err != noErr) {
        return err;
    }

    /* Create the alias handle */
    err = NewAlias(nil, target, &aliasHandle);
    if (err != noErr) {
        return err;
    }

    /* Create the alias file */
    err = FSpCreate(aliasFile, 'MACS', 'alis', smSystemScript);
    if (err != noErr && err != dupFNErr) {
        DisposeHandle((Handle)aliasHandle);
        return err;
    }

    /* Create the alias resource */
    err = CreateAliasResource(target, aliasFile);
    if (err != noErr) {
        FSpDelete(aliasFile);
        DisposeHandle((Handle)aliasHandle);
        return err;
    }

    /* Set the alias flag in Finder info */
    pb.ioCompletion = nil;
    pb.ioNamePtr = aliasFile->name;
    pb.ioVRefNum = aliasFile->vRefNum;
    pb.u.hFileInfo.ioDirID = aliasFile->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
        BlockMoveData(&pb.u.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FInfo));
        finderInfo.fdFlags |= kIsAlias;
        BlockMoveData(&finderInfo, &pb.u.hFileInfo.ioFlFndrInfo, sizeof(FInfo));
        pb.u.hFileInfo.ioDirID = aliasFile->parID;
        err = PBSetCatInfoSync(&pb);
    }

    DisposeHandle((Handle)aliasHandle);
    return err;
}

/*
 * ValidateAliasFile - Check if file is a valid alias

 */
static OSErr ValidateAliasFile(FSSpec *aliasFile)
{
    OSErr err;
    CInfoPBRec pb;
    FInfo finderInfo;
    short aliasRefNum;
    Handle aliasResource;

    /* Check Finder info for alias flag */
    pb.ioCompletion = nil;
    pb.ioNamePtr = aliasFile->name;
    pb.ioVRefNum = aliasFile->vRefNum;
    pb.u.hFileInfo.ioDirID = aliasFile->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    BlockMoveData(&pb.u.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FInfo));

    /* Check if alias flag is set */
    if (!(finderInfo.fdFlags & kIsAlias)) {
        return paramErr; /* Not marked as alias */
    }

    /* Check file type */
    if (finderInfo.fdType != 'alis') {
        return paramErr; /* Wrong file type */
    }

    /* Try to open resource fork */
    aliasRefNum = FSpOpenResFile(aliasFile, fsRdPerm);
    if (aliasRefNum == -1) {
        return ResError();
    }

    /* Check for alias resource */
    aliasResource = Get1Resource(kAliasResourceType, kAliasResourceID);
    if (aliasResource == nil) {
        CloseResFile(aliasRefNum);
        return resNotFound;
    }

    /* Validate alias resource size */
    if (GetHandleSize(aliasResource) < kAliasMinimumSize) {
        ReleaseResource(aliasResource);
        CloseResFile(aliasRefNum);
        return paramErr;
    }

    /* Validate alias record structure */
    HLock(aliasResource);
    Boolean valid = IsValidAliasRecord((AliasRecord*)*aliasResource);
    HUnlock(aliasResource);
    if (!valid) {
        ReleaseResource(aliasResource);
        CloseResFile(aliasRefNum);
        return paramErr;
    }

    ReleaseResource(aliasResource);
    CloseResFile(aliasRefNum);
    return noErr;
}

/*
 * CreateAliasResource - Create alias resource in file

 */
static OSErr CreateAliasResource(FSSpec *target, FSSpec *aliasFile)
{
    OSErr err;
    AliasHandle aliasHandle = nil;
    short aliasRefNum;

    /* Create alias handle */
    err = NewAlias(nil, target, &aliasHandle);
    if (err != noErr) {
        return err;
    }

    /* Create resource file */
    FSpCreateResFile(aliasFile, 'MACS', 'alis', smSystemScript);
    err = ResError();
    if (err != noErr) {
        DisposeHandle((Handle)aliasHandle);
        return err;
    }

    /* Open resource file */
    aliasRefNum = FSpOpenResFile(aliasFile, fsWrPerm);
    if (aliasRefNum == -1) {
        DisposeHandle((Handle)aliasHandle);
        return ResError();
    }

    /* Add alias resource */
    AddResource((Handle)aliasHandle, kAliasResourceType, kAliasResourceID, PSTR("alias"));
    err = ResError();
    if (err == noErr) {
        WriteResource((Handle)aliasHandle);
        err = ResError();
    }

    CloseResFile(aliasRefNum);

    if (err != noErr) {
        DisposeHandle((Handle)aliasHandle);
    }

    return err;
}

/*
 * UpdateAliasFile - Update alias file with new target

 */
static OSErr UpdateAliasFile(FSSpec *aliasFile, FSSpec *newTarget)
{
    OSErr err;
    AliasHandle newAliasHandle = nil;
    short aliasRefNum;
    Handle oldAliasResource;

    /* Create new alias handle */
    err = NewAlias(nil, newTarget, &newAliasHandle);
    if (err != noErr) {
        return err;
    }

    /* Open alias file */
    aliasRefNum = FSpOpenResFile(aliasFile, fsWrPerm);
    if (aliasRefNum == -1) {
        DisposeHandle((Handle)newAliasHandle);
        return ResError();
    }

    /* Remove old alias resource */
    oldAliasResource = Get1Resource(kAliasResourceType, kAliasResourceID);
    if (oldAliasResource != nil) {
        RemoveResource(oldAliasResource);
    }

    /* Add new alias resource */
    AddResource((Handle)newAliasHandle, kAliasResourceType, kAliasResourceID, PSTR("alias"));
    err = ResError();
    if (err == noErr) {
        WriteResource((Handle)newAliasHandle);
        err = ResError();
    }

    CloseResFile(aliasRefNum);

    if (err != noErr) {
        DisposeHandle((Handle)newAliasHandle);
    }

    return err;
}

/*
 * LoadAliasTypeTable - Load alias type mapping table

 */
static OSErr LoadAliasTypeTable(void)
{
    if (gAliasTypeTable != nil) {
        return noErr; /* Already loaded */
    }

    /* Load alias type mapping table from resource */
    gAliasTypeTable = GetResource('fmap', rAliasTypeMapTable);
    if (gAliasTypeTable == nil) {
        return ResError();
    }

    return noErr;
}

/*
 * IsValidAliasRecord - Validate alias record structure

 */
static Boolean IsValidAliasRecord(AliasRecord *record)
{
    if (record == nil) {
        return false;
    }

    /* Check record size */
    if (record->aliasSize < sizeof(AliasRecord) || record->aliasSize > kAliasMaximumSize) {
        return false;
    }

    /* Check version */
    if (record->version != 2) { /* System 7 alias version */
        return false;
    }

    /* Check alias kind - cast to OSType to match case values */
    switch ((OSType)record->aliasKind) {
        case kContainerFolderAliasType:
        case kContainerTrashAliasType:
        case kContainerHardDiskAliasType:
        case kContainerFloppyAliasType:
        case kApplicationAliasType:
        case kSystemFolderAliasType:
            return true;
        default:
            return false;
    }
}

/*-----------------------------------------------------------------------*/
/* Stub Implementations (Basic functionality until fully implemented)   */
/*-----------------------------------------------------------------------*/

/*
 * ResolveAliasFile - Resolve alias file to target
 * Basic stub implementation - returns the original file unchanged
 */
OSErr ResolveAliasFile(const FSSpec* spec, FSSpec* target, Boolean* wasAliased, Boolean* wasFolder) {
    if (target) *target = *spec;
    if (wasAliased) *wasAliased = false;
    if (wasFolder) *wasFolder = false;
    return noErr;
}

/*
 * NewAlias - Create new alias handle
 * Basic stub implementation - allocates empty alias record
 */
OSErr NewAlias(const FSSpec* fromFile, const FSSpec* target, AliasHandle* alias) {
    if (alias) *alias = (AliasHandle)NewHandle(sizeof(AliasRecord));
    return noErr;
}
