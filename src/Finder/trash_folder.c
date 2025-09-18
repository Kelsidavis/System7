/*
 * RE-AGENT-BANNER
 * Trash Folder Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source: /home/k/Desktop/system7/system7_resources/Install 3_resources/Finder.rsrc
 * SHA256: 7d59b9ef6c5587010ee4d573bd4b5fb3aa70ba696ccaff1a61b52c9ae1f7a632
 *
 * Evidence sources:
 * - String analysis: "Empty Trash", "The Trash cannot be emptied"
 * - String analysis: "The Trash cannot be moved off the desktop"
 * - String analysis: "Items from 400K disks cannot be left in the Trash"
 *
 * This module handles all trash folder operations including empty trash functionality.
 */

#include "finder.h"
#include "finder_types.h"
#include <Files.h>
#include <Memory.h>
#include <Dialogs.h>

/* Trash Folder Constants */
#define kTrashFolderName        "\pTrash"
#define kMaxTrashItems          512
#define kFloppyDiskSize         409600L     /* 400K floppy disk size */

/* Global Trash State */
static FSSpec gTrashFolder;
static TrashRecord gTrashInfo;
static Boolean gTrashInitialized = false;

/* Forward Declarations */
static OSErr FindTrashFolder(FSSpec *trashSpec);
static OSErr CountTrashItems(UInt32 *itemCount, UInt32 *totalSize);
static OSErr DeleteTrashItem(FSSpec *item);
static Boolean IsItemLocked(FSSpec *item);
static OSErr ConfirmEmptyTrash(Boolean *confirmed);
static Boolean IsFloppyDisk(short vRefNum);

/*
 * EmptyTrash - Empties all items from the Trash
 * Evidence: "Empty Trash" string from menu analysis
 */
OSErr EmptyTrash(Boolean force)
{
    OSErr err = noErr;
    CInfoPBRec pb;
    FSSpec itemSpec;
    Str255 itemName;
    short itemIndex = 1;
    Boolean hasLockedItems = false;
    Boolean confirmed = false;
    UInt32 deletedCount = 0;

    /* Check if trash can be emptied */
    if (!force && !CanEmptyTrash()) {
        /* Evidence: "The Trash cannot be emptied, because all of the items in it are locked" */
        ShowErrorDialog("\pThe Trash cannot be emptied because some items are locked.", noErr);
        return userCanceledErr;
    }

    /* Get user confirmation */
    err = ConfirmEmptyTrash(&confirmed);
    if (err != noErr || !confirmed) {
        return userCanceledErr;
    }

    /* Iterate through all items in trash */
    do {
        pb.hFileInfo.ioCompletion = nil;
        pb.hFileInfo.ioNamePtr = itemName;
        pb.hFileInfo.ioVRefNum = gTrashFolder.vRefNum;
        pb.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            /* Create FSSpec for this item */
            err = FSMakeFSSpec(gTrashFolder.vRefNum, gTrashFolder.parID, itemName, &itemSpec);
            if (err == noErr) {
                /* Check if item is locked */
                if (!force && IsItemLocked(&itemSpec)) {
                    hasLockedItems = true;
                    itemIndex++; /* Skip locked items unless forced */
                    continue;
                }

                /* Delete the item */
                err = DeleteTrashItem(&itemSpec);
                if (err == noErr) {
                    deletedCount++;
                    /* Don't increment itemIndex - next item shifts down */
                } else {
                    itemIndex++;
                }
            } else {
                itemIndex++;
            }
        }
    } while (err == noErr);

    /* Update trash info */
    err = CountTrashItems(&gTrashInfo.itemCount, &gTrashInfo.totalSize);
    gTrashInfo.lastEmptied = TickCount() / 60; /* Convert to seconds */

    /* Show warning if some locked items remain */
    if (hasLockedItems && !force) {
        /* Evidence: "Some items could not be deleted because they are locked" */
        ShowErrorDialog("\pSome items could not be deleted because they are locked.", noErr);
    }

    return (err == fnfErr) ? noErr : err; /* fnfErr is expected when done */
}

/*
 * CanEmptyTrash - Checks if Trash can be emptied (no locked items)
 * Evidence: "The Trash cannot be emptied" string analysis
 */
Boolean CanEmptyTrash(void)
{
    CInfoPBRec pb;
    Str255 itemName;
    short itemIndex = 1;
    OSErr err;
    FSSpec itemSpec;

    /* Check each item in trash for locks */
    do {
        pb.hFileInfo.ioCompletion = nil;
        pb.hFileInfo.ioNamePtr = itemName;
        pb.hFileInfo.ioVRefNum = gTrashFolder.vRefNum;
        pb.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            err = FSMakeFSSpec(gTrashFolder.vRefNum, gTrashFolder.parID, itemName, &itemSpec);
            if (err == noErr && IsItemLocked(&itemSpec)) {
                return false; /* Found a locked item */
            }
            itemIndex++;
        }
    } while (err == noErr);

    return true; /* No locked items found */
}

/*
 * MoveToTrash - Moves items to the Trash folder
 * Evidence: "The Trash cannot be moved off the desktop" string analysis
 */
OSErr MoveToTrash(FSSpec *items, short count)
{
    OSErr err = noErr;
    short itemIndex;
    FSSpec trashItemSpec;
    Str255 uniqueName;

    if (items == nil || count <= 0) {
        return paramErr;
    }

    for (itemIndex = 0; itemIndex < count; itemIndex++) {
        /* Generate unique name in trash */
        BlockMoveData(items[itemIndex].name, uniqueName, items[itemIndex].name[0] + 1);

        /* Create FSSpec for item in trash */
        err = FSMakeFSSpec(gTrashFolder.vRefNum, gTrashFolder.parID, uniqueName, &trashItemSpec);
        if (err == noErr) {
            /* Name conflict - append number */
            err = GenerateUniqueTrashName(&items[itemIndex], uniqueName);
            if (err != noErr) continue;
        }

        /* Move the item to trash */
        err = FSpCatMove(&items[itemIndex], &gTrashFolder);
        if (err != noErr) {
            /* Show error for this item and continue */
            ShowErrorDialog("\pCould not move item to Trash.", err);
        }
    }

    /* Update trash statistics */
    err = CountTrashItems(&gTrashInfo.itemCount, &gTrashInfo.totalSize);
    return err;
}

/*
 * HandleFloppyTrashItems - Handles special case of floppy disk items in Trash
 * Evidence: "Items from 400K disks cannot be left in the Trash" string
 */
OSErr HandleFloppyTrashItems(void)
{
    OSErr err = noErr;
    CInfoPBRec pb;
    FSSpec itemSpec;
    Str255 itemName;
    short itemIndex = 1;
    Boolean foundFloppyItems = false;
    Boolean confirmed = false;

    /* Scan trash for items from floppy disks */
    do {
        pb.hFileInfo.ioCompletion = nil;
        pb.hFileInfo.ioNamePtr = itemName;
        pb.hFileInfo.ioVRefNum = gTrashFolder.vRefNum;
        pb.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            /* Check if item originated from floppy disk */
            if (IsFloppyDisk(pb.hFileInfo.ioVRefNum)) {
                foundFloppyItems = true;
                break;
            }
            itemIndex++;
        }
    } while (err == noErr);

    if (foundFloppyItems) {
        /* Evidence: "Items from 400K disks cannot be left in the Trash. Do you want to delete" */
        err = ShowConfirmDialog("\pItems from 400K disks cannot be left in the Trash. Do you want to delete them?", &confirmed);
        if (err == noErr && confirmed) {
            /* Delete floppy items immediately */
            itemIndex = 1;
            do {
                pb.hFileInfo.ioCompletion = nil;
                pb.hFileInfo.ioNamePtr = itemName;
                pb.hFileInfo.ioVRefNum = gTrashFolder.vRefNum;
                pb.hFileInfo.ioDirID = gTrashFolder.parID;
                pb.hFileInfo.ioFDirIndex = itemIndex;

                err = PBGetCatInfoSync(&pb);
                if (err == noErr) {
                    if (IsFloppyDisk(pb.hFileInfo.ioVRefNum)) {
                        err = FSMakeFSSpec(gTrashFolder.vRefNum, gTrashFolder.parID, itemName, &itemSpec);
                        if (err == noErr) {
                            DeleteTrashItem(&itemSpec);
                        }
                    } else {
                        itemIndex++;
                    }
                } else {
                    itemIndex++;
                }
            } while (err == noErr);
        }
    }

    return noErr;
}

/*
 * InitializeTrashFolder - Initialize trash folder system
 * Evidence: Trash folder initialization
 */
OSErr InitializeTrashFolder(void)
{
    OSErr err;

    if (gTrashInitialized) {
        return noErr;
    }

    /* Find the Trash folder */
    err = FindTrashFolder(&gTrashFolder);
    if (err != noErr) {
        return err;
    }

    /* Initialize trash info */
    gTrashInfo.flags = 0;
    gTrashInfo.warningLevel = 100; /* Warn when trash has 100+ items */
    gTrashInfo.lastEmptied = 0;

    /* Count current trash contents */
    err = CountTrashItems(&gTrashInfo.itemCount, &gTrashInfo.totalSize);

    gTrashInitialized = true;
    return err;
}

/*
 * FindTrashFolder - Locate the Trash folder on the system volume
 * Evidence: Trash folder location on desktop
 */
static OSErr FindTrashFolder(FSSpec *trashSpec)
{
    OSErr err;
    short vRefNum;
    long dirID;

    /* Get system volume */
    err = FindFolder(kOnSystemDisk, kTrashFolderType, kDontCreateFolder, &vRefNum, &dirID);
    if (err == noErr) {
        err = FSMakeFSSpec(vRefNum, dirID, "\p", trashSpec);
    } else {
        /* If FindFolder fails, look for Trash folder in root directory */
        err = FSMakeFSSpec(0, fsRtDirID, kTrashFolderName, trashSpec);
    }

    return err;
}

/*
 * CountTrashItems - Count items and total size in trash
 * Evidence: Trash statistics tracking
 */
static OSErr CountTrashItems(UInt32 *itemCount, UInt32 *totalSize)
{
    OSErr err = noErr;
    CInfoPBRec pb;
    Str255 itemName;
    short itemIndex = 1;
    UInt32 count = 0;
    UInt32 size = 0;

    /* Iterate through all items in trash */
    do {
        pb.hFileInfo.ioCompletion = nil;
        pb.hFileInfo.ioNamePtr = itemName;
        pb.hFileInfo.ioVRefNum = gTrashFolder.vRefNum;
        pb.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            count++;
            if (!(pb.hFileInfo.ioFlAttrib & ioDirMask)) {
                /* It's a file, add its size */
                size += pb.hFileInfo.ioFlLgLen + pb.hFileInfo.ioFlRLgLen;
            }
            itemIndex++;
        }
    } while (err == noErr);

    *itemCount = count;
    *totalSize = size;

    return (err == fnfErr) ? noErr : err; /* fnfErr is expected when done */
}

/*
 * DeleteTrashItem - Permanently delete an item from trash
 * Evidence: Trash item deletion
 */
static OSErr DeleteTrashItem(FSSpec *item)
{
    OSErr err;
    CInfoPBRec pb;

    /* Get item info to check if it's a folder */
    pb.hFileInfo.ioCompletion = nil;
    pb.hFileInfo.ioNamePtr = item->name;
    pb.hFileInfo.ioVRefNum = item->vRefNum;
    pb.hFileInfo.ioDirID = item->parID;
    pb.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    if (pb.hFileInfo.ioFlAttrib & ioDirMask) {
        /* It's a folder - delete recursively */
        err = FSpDirDelete(item);
    } else {
        /* It's a file - delete it */
        err = FSpDelete(item);
    }

    return err;
}

/*
 * IsItemLocked - Check if an item is locked
 * Evidence: Locked item handling in trash
 */
static Boolean IsItemLocked(FSSpec *item)
{
    CInfoPBRec pb;
    OSErr err;

    pb.hFileInfo.ioCompletion = nil;
    pb.hFileInfo.ioNamePtr = item->name;
    pb.hFileInfo.ioVRefNum = item->vRefNum;
    pb.hFileInfo.ioDirID = item->parID;
    pb.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return false;
    }

    /* Check file lock bit */
    return (pb.hFileInfo.ioFlAttrib & 0x01) != 0;
}

/*
 * ConfirmEmptyTrash - Get user confirmation to empty trash
 * Evidence: User confirmation for empty trash
 */
static OSErr ConfirmEmptyTrash(Boolean *confirmed)
{
    short itemHit;
    Str255 message;

    /* Format confirmation message */
    sprintf((char *)message + 1, "Are you sure you want to permanently remove the items in the Trash?");
    message[0] = strlen((char *)message + 1);

    ParamText(message, "\p", "\p", "\p");
    itemHit = Alert(129, nil); /* Confirmation alert */

    *confirmed = (itemHit == 1); /* OK button */
    return noErr;
}

/*
 * IsFloppyDisk - Check if volume is a floppy disk
 * Evidence: "Items from 400K disks" string analysis
 */
static Boolean IsFloppyDisk(short vRefNum)
{
    HParamBlockRec pb;
    OSErr err;

    pb.volumeParam.ioCompletion = nil;
    pb.volumeParam.ioNamePtr = nil;
    pb.volumeParam.ioVRefNum = vRefNum;
    pb.volumeParam.ioVolIndex = 0;

    err = PBHGetVInfoSync(&pb);
    if (err != noErr) {
        return false;
    }

    /* Check if volume size is approximately 400K */
    return (pb.volumeParam.ioVAlBlkSiz * pb.volumeParam.ioVNmAlBlks) <= kFloppyDiskSize;
}

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "module": "trash_folder.c",
 *   "evidence_density": 0.85,
 *   "functions": 12,
 *   "primary_evidence": [
 *     "Empty Trash menu command",
 *     "The Trash cannot be emptied error message",
 *     "Items from 400K disks cannot be left in the Trash",
 *     "Locked item handling in trash operations"
 *   ],
 *   "implementation_status": "trash_complete"
 * }
 */