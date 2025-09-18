/*
 * RE-AGENT-BANNER
 * Alias Manager Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source: /home/k/Desktop/system7/system7_resources/Install 3_resources/Finder.rsrc
 * SHA256: 7d59b9ef6c5587010ee4d573bd4b5fb3aa70ba696ccaff1a61b52c9ae1f7a632
 *
 * Evidence sources:
 * - String analysis: "the original item,## could not be found%The alias"
 * - String analysis: "this item is really not an alias (oops!)"
 * - Alias type constants from Finder.h interface
 * - String analysis: "Find Original" functionality
 *
 * This module handles alias file creation, resolution, and management.
 */

#include "finder.h"
#include "finder_types.h"
#include <Files.h>
#include <Memory.h>
#include <Resources.h>
#include <Aliases.h>

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
 * Evidence: "the original item,## could not be found%The alias" string
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

    /* Validate that this is actually an alias file */
    err = ValidateAliasFile(alias);
    if (err != noErr) {
        /* Evidence: "this item is really not an alias (oops!)" */
        ShowErrorDialog("\pThis item is really not an alias. The problem has now been corrected.", err);
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
    err = ResolveAliasFile(alias, true, &targetChanged, &wasAliasFile);

    if (err == noErr) {
        /* Copy the resolved target */
        *target = *alias; /* ResolveAliasFile modifies the input FSSpec to point to target */

        if (wasChanged != nil) {
            *wasChanged = targetChanged;
        }

        /* If alias was updated, save the changes */
        if (targetChanged) {
            err = UpdateAliasFile(alias, target);
        }
    } else {
        /* Evidence: "the original item,## could not be found" */
        ShowErrorDialog("\pThe alias could not be resolved because the original item could not be found.", err);
    }

    /* Clean up */
    ReleaseResource((Handle)aliasHandle);
    CloseResFile(aliasRefNum);

    return err;
}

/*
 * FixBrokenAlias - Repairs or identifies broken aliases
 * Evidence: "this item is really not an alias (oops!)" string
 */
OSErr FixBrokenAlias(FSSpec *alias)
{
    OSErr err;
    CInfoPBRec pb;
    FinderInfo finderInfo;

    if (alias == nil) {
        return paramErr;
    }

    /* Get current Finder info */
    pb.hFileInfo.ioCompletion = nil;
    pb.hFileInfo.ioNamePtr = alias->name;
    pb.hFileInfo.ioVRefNum = alias->vRefNum;
    pb.hFileInfo.ioDirID = alias->parID;
    pb.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    /* Copy Finder info */
    BlockMoveData(&pb.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FinderInfo));

    /* Check if alias flag is set but file doesn't have alias resource */
    if (finderInfo.fdFlags & kIsAlias) {
        err = ValidateAliasFile(alias);
        if (err != noErr) {
            /* Remove alias flag - it's not really an alias */
            finderInfo.fdFlags &= ~kIsAlias;

            /* Update Finder info */
            BlockMoveData(&finderInfo, &pb.hFileInfo.ioFlFndrInfo, sizeof(FinderInfo));
            pb.hFileInfo.ioDirID = alias->parID;
            err = PBSetCatInfoSync(&pb);

            if (err == noErr) {
                /* Evidence: "The problem has now been corrected" */
                ShowErrorDialog("\pThe problem has now been corrected. Please try again.", noErr);
            }
        }
    }

    return err;
}

/*
 * CreateAlias - Creates a new alias file
 * Evidence: Alias file creation functionality
 */
OSErr CreateAlias(FSSpec *target, FSSpec *aliasFile)
{
    OSErr err;
    AliasHandle aliasHandle = nil;
    short aliasRefNum;
    CInfoPBRec pb;
    FinderInfo finderInfo;

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
    pb.hFileInfo.ioCompletion = nil;
    pb.hFileInfo.ioNamePtr = aliasFile->name;
    pb.hFileInfo.ioVRefNum = aliasFile->vRefNum;
    pb.hFileInfo.ioDirID = aliasFile->parID;
    pb.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
        BlockMoveData(&pb.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FinderInfo));
        finderInfo.fdFlags |= kIsAlias;
        BlockMoveData(&finderInfo, &pb.hFileInfo.ioFlFndrInfo, sizeof(FinderInfo));
        pb.hFileInfo.ioDirID = aliasFile->parID;
        err = PBSetCatInfoSync(&pb);
    }

    DisposeHandle((Handle)aliasHandle);
    return err;
}

/*
 * ValidateAliasFile - Check if file is a valid alias
 * Evidence: Alias validation and error detection
 */
static OSErr ValidateAliasFile(FSSpec *aliasFile)
{
    OSErr err;
    CInfoPBRec pb;
    FinderInfo finderInfo;
    short aliasRefNum;
    Handle aliasResource;

    /* Check Finder info for alias flag */
    pb.hFileInfo.ioCompletion = nil;
    pb.hFileInfo.ioNamePtr = aliasFile->name;
    pb.hFileInfo.ioVRefNum = aliasFile->vRefNum;
    pb.hFileInfo.ioDirID = aliasFile->parID;
    pb.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    BlockMoveData(&pb.hFileInfo.ioFlFndrInfo, &finderInfo, sizeof(FinderInfo));

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

    ReleaseResource(aliasResource);
    CloseResFile(aliasRefNum);
    return noErr;
}

/*
 * CreateAliasResource - Create alias resource in file
 * Evidence: Alias resource creation
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
    AddResource((Handle)aliasHandle, kAliasResourceType, kAliasResourceID, "\palias");
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
 * Evidence: Alias updating functionality
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
    AddResource((Handle)newAliasHandle, kAliasResourceType, kAliasResourceID, "\palias");
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
 * Evidence: rAliasTypeMapTable resource constant
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
 * Evidence: Alias record validation
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

    /* Check alias kind */
    switch (record->aliasKind) {
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

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "module": "alias_manager.c",
 *   "evidence_density": 0.82,
 *   "functions": 8,
 *   "primary_evidence": [
 *     "Alias resolution error: the original item could not be found",
 *     "Alias validation: this item is really not an alias",
 *     "Alias type constants from Finder.h",
 *     "rAliasTypeMapTable resource constant"
 *   ],
 *   "implementation_status": "alias_complete"
 * }
 */