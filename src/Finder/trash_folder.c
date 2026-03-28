/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Trash Folder Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
 *
 * Evidence sources:
 * - String analysis: "Empty Trash", "The Trash cannot be emptied"
 * - String analysis: "The Trash cannot be moved off the desktop"
 * - String analysis: "Items from 400K disks cannot be left in the Trash"
 *
 * This module handles all trash folder operations including empty trash functionality.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "Finder/finder.h"
#include "Finder/finder_types.h"
#include "FileMgr/file_manager.h"
/* Use local headers instead of system headers */
#include "MemoryMgr/memory_manager_types.h"
#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogManager.h"
#include "EventManager/EventManager.h"
#include "Finder/FinderLogging.h"
#include "sys71_stubs.h"
#include "FS/hfs_types.h"


/* Trash Folder Constants */
#define kTrashFolderName        "\005Trash"
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
        /* ShowErrorDialog("\pThe Trash cannot be emptied because some items are locked.", noErr); */
        return userCanceledErr;
    }

    /* Get user confirmation */
    err = ConfirmEmptyTrash(&confirmed);
    if (err != noErr || !confirmed) {
        return userCanceledErr;
    }

    /* Iterate through all items in trash */
    do {
        pb.ioCompletion = nil;
        pb.ioNamePtr = itemName;
        pb.ioVRefNum = gTrashFolder.vRefNum;
        pb.u.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.u.hFileInfo.ioFDirIndex = itemIndex;

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
    UInt32 tempItemCount, tempTotalSize;
    err = CountTrashItems(&tempItemCount, &tempTotalSize);
    gTrashInfo.itemCount = (UInt16)tempItemCount;
    gTrashInfo.totalSize = tempTotalSize;
    gTrashInfo.lastEmptied = TickCount() / 60; /* Convert to seconds */

    /* Show warning if some locked items remain */
    if (hasLockedItems && !force) {
        /* ShowErrorDialog("\pSome items could not be deleted because they are locked.", noErr); */
    }

    /* Refresh trash icon on desktop to show empty state */
    {
        extern void Desktop_RefreshTrashIcon(void);
        Desktop_RefreshTrashIcon();
    }

    return (err == fnfErr) ? noErr : err; /* fnfErr is expected when done */
}

/*
 * CanEmptyTrash - Checks if Trash can be emptied (no locked items)

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
        pb.ioCompletion = nil;
        pb.ioNamePtr = itemName;
        pb.ioVRefNum = gTrashFolder.vRefNum;
        pb.u.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.u.hFileInfo.ioFDirIndex = itemIndex;

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
            err = GenerateUniqueTrashName(items[itemIndex].name, uniqueName);
            if (err != noErr) continue;
        }

        /* Move the item to trash */
        err = FSpCatMove(&items[itemIndex], &gTrashFolder);
        if (err != noErr) {
            /* Show error for this item and continue */
            ShowErrorDialog("\036Could not move item to Trash.", err);
        }
    }

    /* Update trash statistics */
    UInt32 tempItemCount, tempTotalSize;
    err = CountTrashItems(&tempItemCount, &tempTotalSize);
    gTrashInfo.itemCount = (UInt16)tempItemCount;
    gTrashInfo.totalSize = tempTotalSize;
    return err;
}

/*
 * HandleFloppyTrashItems - Handles special case of floppy disk items in Trash

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
        pb.ioCompletion = nil;
        pb.ioNamePtr = itemName;
        pb.ioVRefNum = gTrashFolder.vRefNum;
        pb.u.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.u.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            /* Check if item originated from floppy disk */
            if (IsFloppyDisk(pb.ioVRefNum)) {
                foundFloppyItems = true;
                break;
            }
            itemIndex++;
        }
    } while (err == noErr);

    if (foundFloppyItems) {
        /* err = ShowConfirmDialog("\pItems from 400K disks cannot be left in the Trash. Do you want to delete them?", &confirmed); */
        if (err == noErr && confirmed) {
            /* Delete floppy items immediately */
            itemIndex = 1;
            do {
                pb.ioCompletion = nil;
                pb.ioNamePtr = itemName;
                pb.ioVRefNum = gTrashFolder.vRefNum;
                pb.u.hFileInfo.ioDirID = gTrashFolder.parID;
                pb.u.hFileInfo.ioFDirIndex = itemIndex;

                err = PBGetCatInfoSync(&pb);
                if (err == noErr) {
                    if (IsFloppyDisk(pb.ioVRefNum)) {
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
    UInt32 tempItemCount, tempTotalSize;
    err = CountTrashItems(&tempItemCount, &tempTotalSize);
    gTrashInfo.itemCount = (UInt16)tempItemCount;
    gTrashInfo.totalSize = tempTotalSize;

    gTrashInitialized = true;
    return err;
}

/*
 * FindTrashFolder - Locate the Trash folder on the system volume

 */
static OSErr FindTrashFolder(FSSpec *trashSpec)
{
    OSErr err;
    short vRefNum;
    SInt32 dirID;

    /* Get system volume */
    err = FindFolder(kOnSystemDisk, kTrashFolderType, kDontCreateFolder, &vRefNum, &dirID);
    if (err == noErr) {
        err = FSMakeFSSpec(vRefNum, dirID, "\000", trashSpec);
    } else {
        /* If FindFolder fails, look for Trash folder in root directory */
        err = FSMakeFSSpec(0, fsRtDirID, kTrashFolderName, trashSpec);
    }

    return err;
}

/*
 * CountTrashItems - Count items and total size in trash

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
        pb.ioCompletion = nil;
        pb.ioNamePtr = itemName;
        pb.ioVRefNum = gTrashFolder.vRefNum;
        pb.u.hFileInfo.ioDirID = gTrashFolder.parID;
        pb.u.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            count++;
            if (!(pb.u.hFileInfo.ioFlAttrib & ioDirMask)) {
                /* It's a file, add its size */
                size += pb.u.hFileInfo.ioFlLgLen + pb.u.hFileInfo.ioFlRLgLen;
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

 */
static OSErr DeleteTrashItem(FSSpec *item)
{
    /* Convert FSSpec name to C string for VFS lookup */
    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);
    extern bool VFS_Delete(VRefNum vref, FileID id);
    extern bool VFS_DeleteTree(VRefNum vref, DirID parent, FileID id);

    char cName[32];
    unsigned char len = item->name[0];
    if (len > 31) len = 31;
    for (int i = 0; i < len; i++) cName[i] = item->name[i + 1];
    cName[len] = '\0';

    CatEntry entry;
    if (!VFS_Lookup(item->vRefNum, item->parID, cName, &entry)) {
        return fnfErr;
    }

    bool ok;
    if (entry.kind == kNodeDir) {
        ok = VFS_DeleteTree(item->vRefNum, item->parID, entry.id);
    } else {
        ok = VFS_Delete(item->vRefNum, entry.id);
    }

    return ok ? noErr : ioErr;
}

/*
 * IsItemLocked - Check if an item is locked

 */
static Boolean IsItemLocked(FSSpec *item)
{
    CInfoPBRec pb;
    OSErr err;

    pb.ioCompletion = nil;
    pb.ioNamePtr = item->name;
    pb.ioVRefNum = item->vRefNum;
    pb.u.hFileInfo.ioDirID = item->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return false;
    }

    /* Check file lock bit */
    return (pb.u.hFileInfo.ioFlAttrib & 0x01) != 0;
}

/*
 * ConfirmEmptyTrash - Get user confirmation to empty trash

 */
static OSErr ConfirmEmptyTrash(Boolean *confirmed)
{
    extern void ShowWindow(WindowPtr);
    extern void SystemTask(void);

    if (!confirmed) return paramErr;
    *confirmed = false;

    /* Build DITL: prompt (1), OK button (2), Cancel button (3) */
    Handle ditl = NewHandleClear(256);
    if (!ditl) {
        *confirmed = true;  /* Can't show dialog, proceed anyway */
        return noErr;
    }

    HLock(ditl);
    unsigned char* p = (unsigned char*)*ditl;

    /* 3 items, count-1 = 2 */
    *p++ = 0; *p++ = 2;

    /* Item 1: Warning text */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 10;  /* top */
    *p++ = 0; *p++ = 10;  /* left */
    *p++ = 0; *p++ = 50;  /* bottom */
    *p++ = 1; *p++ = 20;  /* right = 276 */
    *p++ = 8;              /* statText */
    {
        const char* msg = "Are you sure you want to permanently remove the items in the Trash?";
        unsigned char len = 66;
        *p++ = len;
        memcpy(p, msg, len);
        p += len;
    }

    /* Item 2: OK button */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 60;  /* top */
    *p++ = 0; *p++ = 190; /* left */
    *p++ = 0; *p++ = 80;  /* bottom */
    *p++ = 1; *p++ = 20;  /* right = 276 */
    *p++ = 4;              /* btnCtrl */
    *p++ = 2; *p++ = 'O'; *p++ = 'K';

    /* Item 3: Cancel button */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 60;
    *p++ = 0; *p++ = 90;
    *p++ = 0; *p++ = 80;
    *p++ = 0; *p++ = 180;
    *p++ = 4;
    *p++ = 6; *p++ = 'C'; *p++ = 'a'; *p++ = 'n'; *p++ = 'c'; *p++ = 'e'; *p++ = 'l';

    HUnlock(ditl);

    Rect bounds = {140, 100, 240, 400};
    static unsigned char title[] = {0};
    DialogPtr dlg = NewDialog(NULL, &bounds, title, true, 1 /* dBoxProc */,
                              (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        *confirmed = true;
        return noErr;
    }

    ShowWindow((WindowPtr)dlg);

    /* Modal event loop */
    short itemHit = 0;
    Boolean done = false;
    while (!done) {
        EventRecord event;
        if (GetNextEvent(everyEvent, &event)) {
            if (IsDialogEvent(&event)) {
                DialogPtr whichDlg;
                short item;
                if (DialogSelect(&event, &whichDlg, &item)) {
                    if (whichDlg == dlg) {
                        itemHit = item;
                        done = (item == 2 || item == 3);
                    }
                }
            }
            if (event.what == keyDown || event.what == autoKey) {
                char ch = event.message & 0xFF;
                if (ch == '\r' || ch == 0x03) { itemHit = 2; done = true; }
                if (ch == 0x1B) { itemHit = 3; done = true; }
            }
        }
        SystemTask();
    }

    DisposeDialog(dlg);

    *confirmed = (itemHit == 2);  /* OK = confirmed */
    FINDER_LOG_DEBUG("Trash: Empty trash %s by user\n",
                     *confirmed ? "confirmed" : "cancelled");
    return noErr;
}

/*
 * IsFloppyDisk - Check if volume is a floppy disk

 */
static Boolean IsFloppyDisk(short vRefNum)
{
    HParamBlockRec pb;
    OSErr err;

    pb.ioCompletion = nil;
    pb.ioNamePtr = nil;
    pb.ioVRefNum = vRefNum;
    pb.u.volumeParam.ioVolIndex = 0;

    err = PBHGetVInfoSync(&pb);
    if (err != noErr) {
        return false;
    }

    /* Check if volume size is approximately 400K */
    return (pb.u.volumeParam.ioVAlBlkSiz * pb.u.volumeParam.ioVNmAlBlks) <= kFloppyDiskSize;
}

/*
 * GenerateUniqueTrashName - Generate unique name for item in Trash
 *
 * If the base name conflicts, appends " copy", " copy 2", etc.
 * Uses a simple static counter since checking the actual Trash directory
 * for name conflicts requires VFS enumeration that may not be available.
 */
OSErr GenerateUniqueTrashName(Str255 baseName, Str255 uniqueName) {
    static UInt16 sTrashNameCounter = 0;

    if (!uniqueName || !baseName) {
        return paramErr;
    }

    if (sTrashNameCounter == 0) {
        /* First call: use base name as-is */
        BlockMoveData(baseName, uniqueName, baseName[0] + 1);
    } else {
        /* Subsequent calls: append " copy N" suffix */
        unsigned char baseLen = baseName[0];
        char suffix[16];
        int suffixLen;
        if (sTrashNameCounter == 1) {
            suffixLen = snprintf(suffix, sizeof(suffix), " copy");
        } else {
            suffixLen = snprintf(suffix, sizeof(suffix), " copy %u", sTrashNameCounter);
        }
        if (suffixLen < 0) suffixLen = 0;
        if (suffixLen > (int)sizeof(suffix) - 1) suffixLen = (int)sizeof(suffix) - 1;
        /* Truncate base name if needed to fit suffix within 255 chars */
        if (baseLen + suffixLen > 255) {
            baseLen = 255 - (unsigned char)suffixLen;
        }
        uniqueName[0] = baseLen + (unsigned char)suffixLen;
        memcpy(&uniqueName[1], &baseName[1], baseLen);
        memcpy(&uniqueName[1 + baseLen], suffix, (size_t)suffixLen);
    }

    sTrashNameCounter++;
    return noErr;
}
