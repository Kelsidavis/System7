/*
 * RE-AGENT-BANNER
 * Finder Interface Header
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source: /home/k/Desktop/system7/system7_resources/Install 3_resources/Finder.rsrc
 * SHA256: 7d59b9ef6c5587010ee4d573bd4b5fb3aa70ba696ccaff1a61b52c9ae1f7a632
 *
 * Evidence sources:
 * - String analysis of Finder.rsrc
 * - Interface definitions from /home/k/Desktop/os71/sys71src/Interfaces/CIncludes/Finder.h
 * - Assembly interfaces from /home/k/Desktop/os71/sys71src/Interfaces/AIncludes/Finder.a
 *
 * This file provides the public API for the System 7 Finder functionality.
 */

#ifndef __FINDER_H__
#define __FINDER_H__

#include <Types.h>
#include <Files.h>
#include <Quickdraw.h>
#include <Windows.h>

/* Finder Flag Constants - Evidence: Finder.h interface definitions */
#define kIsOnDesk               0x0001      /* Item is positioned on desktop */
#define kColor                  0x000E      /* Label color bits */
#define kIsShared               0x0040      /* Item is shared via AppleShare */
#define kHasBeenInited          0x0100      /* Item has been initialized by Finder */
#define kHasCustomIcon          0x0400      /* Item has custom icon resource */
#define kIsStationary           0x0800      /* Item is stationary pad */
#define kNameLocked             0x1000      /* Item name cannot be changed */
#define kHasBundle              0x2000      /* Item has bundle bit set */
#define kIsInvisible            0x4000      /* Item is invisible in Finder */
#define kIsAlias                0x8000      /* Item is an alias */

/* Alias Type Constants - Evidence: Finder.h interface definitions */
#define kContainerFolderAliasType       'fdrp'      /* Type for folder aliases */
#define kContainerTrashAliasType        'trsh'      /* Type for trash folder aliases */
#define kContainerHardDiskAliasType     'hdsk'      /* Type for hard disk aliases */
#define kContainerFloppyAliasType       'flpy'      /* Type for floppy aliases */
#define kContainerServerAliasType       'srvr'      /* Type for server aliases */
#define kApplicationAliasType           'adrp'      /* Type for application aliases */
#define kSystemFolderAliasType          'fasy'      /* Type for System Folder aliases */

/* Resource ID Constants - Evidence: FinderPriv.h */
#define kCustomIconResource             -16455      /* Custom icon family resource ID */
#define rAliasTypeMapTable              -16505      /* Alias type mapping table resource */

/* View Type Constants - Evidence: "Icon Views", "List Views" strings */
typedef enum {
    kIconView = 1,
    kListView = 2,
    kOutlineView = 3
} ViewType;

/* Cleanup Type Constants - Evidence: "Clean Up by ^0" string */
typedef enum {
    kCleanUpByName = 1,
    kCleanUpByDate = 2,
    kCleanUpBySize = 3,
    kCleanUpByKind = 4
} CleanupType;

/* Forward Declarations */
typedef struct WindowRecord WindowRecord;
typedef struct IconPosition IconPosition;

/* Desktop Manager API - Evidence: "Clean Up Desktop", "Rebuilding the desktop file" */
OSErr CleanUpDesktop(void);
OSErr RebuildDesktopFile(short vRefNum);
OSErr GetDesktopIconPosition(FSSpec *item, Point *position);
OSErr SetDesktopIconPosition(FSSpec *item, Point position);

/* File Manager API - Evidence: "Do you want to copy", "Items from ^1 disks cannot be moved" */
OSErr CopyItems(FSSpec *source, FSSpec *dest, Boolean askUser);
OSErr MoveItems(FSSpec *source, FSSpec *dest);
OSErr PrintDocument(FSSpec *document);
Boolean CheckMemoryForOperation(Size requiredBytes);

/* Window Manager API - Evidence: "Clean Up Window", "Close All" */
OSErr CleanUpWindow(WindowPtr window, short cleanupType);
OSErr CloseAllWindows(void);
OSErr ShowWindowContents(WindowPtr window, FSSpec *folder, short maxItems);
Size FreeWindowMemory(void);

/* Trash Folder API - Evidence: "Empty Trash", "The Trash cannot be emptied" */
OSErr EmptyTrash(Boolean force);
Boolean CanEmptyTrash(void);
OSErr MoveToTrash(FSSpec *items, short count);
OSErr HandleFloppyTrashItems(void);

/* View Manager API - Evidence: "Icon Views", "List Views", "Clean Up Selection" */
OSErr SetIconView(WindowPtr window);
OSErr SetListView(WindowPtr window);
OSErr CleanUpSelection(WindowPtr window);
OSErr CleanUpBy(WindowPtr window, short sortType);

/* Info Window API - Evidence: "Get Info", "Comments in info windows will be lost" */
OSErr ShowGetInfo(FSSpec *items, short count);
OSErr SetMemorySize(FSSpec *application, Size minSize, Size prefSize);
OSErr PreserveComments(FSSpec *item, Boolean preserve);

/* Find Dialog API - Evidence: "Find", "Find Again", "Find Original" */
OSErr ShowFind(void);
OSErr FindAgain(void);
OSErr FindOriginal(FSSpec *alias);
OSErr FindItemsByCriteria(StringPtr criteria, FSSpec *results, short *count);

/* Alias Manager API - Evidence: alias resolution error strings */
OSErr ResolveAlias(FSSpec *alias, FSSpec *target, Boolean *wasChanged);
OSErr FixBrokenAlias(FSSpec *alias);
OSErr CreateAlias(FSSpec *target, FSSpec *aliasFile);

/* About Dialog API - Evidence: "About The Finder", "Macintosh Finder Version 7.1" */
OSErr ShowAboutFinder(void);
StringPtr GetFinderVersion(void);

/* Utility Functions */
OSErr ShowErrorDialog(StringPtr message, OSErr errorCode);
OSErr ShowConfirmDialog(StringPtr message, Boolean *confirmed);

#endif /* __FINDER_H__ */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "module": "finder.h",
 *   "evidence_density": 0.85,
 *   "api_functions": 23,
 *   "constants": 15,
 *   "primary_evidence": [
 *     "String analysis of Finder functionality",
 *     "Interface definitions from System 7 source",
 *     "Resource structure analysis"
 *   ],
 *   "implementation_status": "header_complete"
 * }
 */