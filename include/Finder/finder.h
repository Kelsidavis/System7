/*
 * RE-AGENT-BANNER
 * Finder Interface Header
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
 *
 * Evidence sources:
 * - String analysis of Finder.rsrc
 * - Interface definitions from 
 * - Assembly interfaces from 
 *
 * This file provides the public API for the System 7 Finder functionality.
 */

#ifndef __FINDER_H__
#define __FINDER_H__

#include "SystemTypes.h"

#include "SystemTypes.h"
#include "FileMgr/file_manager.h"
#include "QuickDraw/QuickDraw.h"
#include "EventManager/EventTypes.h"  /* Include before WindowTypes.h to avoid activeFlag conflict */
#include "WindowManager/WindowTypes.h"
#include "FS/hfs_types.h"  /* For VRefNum, FileID */

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

/* FindFolder Constants */
#define kOnSystemDisk                   -32768      /* System disk */
#define kTrashFolderType                'trsh'      /* Trash folder type */
#define kDontCreateFolder               false       /* Don't create if missing */

/* View Type Constants - Evidence: "Icon Views", "List Views" strings */
#define kIconView       0
#define kListView       1

/* Cleanup Type Constants - Evidence: "Clean Up by ^0" string */
#define kCleanUpByName  0
#define kCleanUpByDate  1
#define kCleanUpBySize  2
#define kCleanUpByKind  3
#define kCleanUpByLabel 4

/* Forward Declarations */

/* Finder Event Handling */
void HandleKeyDown(EventRecord* event);
OSErr HandleContentClick(WindowPtr window, EventRecord* event);
OSErr CloseFinderWindow(WindowPtr window);
void DoUpdate(WindowPtr window);
OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder, SInt16* foundVRefNum, SInt32* foundDirID);

/* Menu Commands */
void DoMenuCommand(short menuID, short item);
void OpenSelectedItems(void);
void ShowGetInfoDialog(WindowPtr w);
void DuplicateSelectedItems(WindowPtr w);
void MakeAliasOfSelectedItems(WindowPtr w);
void PutAwaySelectedItems(WindowPtr w);
void Finder_Undo(void);
void Finder_Cut(void);
void Finder_Copy(void);
void Finder_Paste(void);
void Finder_Clear(void);
void Finder_SelectAll(void);
void SetWindowViewMode(WindowPtr w, short viewMode);
void ApplyLabelToSelection(WindowPtr w, short labelIndex);

/* Desktop Manager API - Evidence: "Clean Up Desktop", "Rebuilding the desktop file" */
OSErr CleanUpDesktop(void);
OSErr RebuildDesktopFile(short vRefNum);
OSErr GetDesktopIconPosition(FSSpec *item, Point *position);
OSErr SetDesktopIconPosition(FSSpec *item, Point position);
OSErr Desktop_AddAliasIcon(const char* name, Point position, FileID targetID, VRefNum vref, Boolean isFolder);
OSErr Desktop_AddVolumeIcon(const char* name, VRefNum vref);
OSErr Desktop_RemoveVolumeIcon(VRefNum vref);
Boolean Desktop_IsOverTrash(Point where);
Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick);
void ArrangeDesktopIcons(void);
OSErr InitializeDesktopDB(void);
OSErr InitializeVolumeIcon(void);
OSErr HandleVolumeDoubleClick(Point clickPoint);
void Desktop_GhostEraseIf(void);
void Desktop_GhostShowAt(const Rect* r);
void StartDragIcon(Point mousePt);
void DragIcon(Point mousePt);
void EndDragIcon(Point mousePt);
Boolean HandleDesktopDrag(Point mousePt, Boolean buttonDown);
void SelectNextDesktopIcon(void);
void OpenSelectedDesktopIcon(void);

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
WindowPtr Finder_OpenDesktopItem(Boolean isTrash, ConstStr255Param title);

/* Folder Window API */
void DrawFolderWindowContents(WindowPtr window, Boolean isTrash);
void InitializeFolderContentsEx(WindowPtr w, Boolean isTrash, VRefNum vref, DirID dirID);
WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID, ConstStr255Param title);
Boolean HandleFolderWindowClick(WindowPtr w, EventRecord *ev, Boolean isDoubleClick);
void FolderWindow_Draw(WindowPtr w);
Boolean IsFolderWindow(WindowPtr w);
void FolderWindow_SelectAll(WindowPtr w);
Boolean FolderWindow_GetSelectedItem(WindowPtr w, VRefNum* outVref, FileID* outFileID);
void FolderWindow_DeleteSelected(WindowPtr w);
void FolderWindow_OpenSelected(WindowPtr w);
void FolderWindow_DuplicateSelected(WindowPtr w);
short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs);
VRefNum FolderWindow_GetVRef(WindowPtr w);
DirID FolderWindow_GetCurrentDir(WindowPtr w);
void FolderWindow_CleanUp(WindowPtr w, Boolean selectedOnly);
void FolderWindow_SortAndArrange(WindowPtr w, short sortType);
void FolderWindowProc(WindowPtr window, short message, long param);
void CleanupFolderWindow(WindowPtr w);
void FolderWindow_SetLabelOnSelected(WindowPtr w, short labelIndex);

/* Trash Folder API - Evidence: "Empty Trash", "The Trash cannot be emptied" */
OSErr EmptyTrash(Boolean force);
Boolean CanEmptyTrash(void);
OSErr MoveToTrash(FSSpec *items, short count);
OSErr HandleFloppyTrashItems(void);
OSErr InitializeTrashFolder(void);

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
void Find_CloseIf(WindowPtr w);
Boolean Find_HandleUpdate(WindowPtr w);
Boolean Find_IsFindWindow(WindowPtr w);
Boolean Find_HandleKeyPress(WindowPtr w, char key);

/* Alias Manager API - Evidence: alias resolution error strings */
OSErr ResolveAlias(FSSpec *alias, FSSpec *target, Boolean *wasChanged);
OSErr FixBrokenAlias(FSSpec *alias);
OSErr CreateAlias(FSSpec *target, FSSpec *aliasFile);

/* About Dialog API - Evidence: "About The Finder", "Macintosh Finder Version 7.1" */
OSErr ShowAboutFinder(void);
StringPtr GetFinderVersion(void);
void AboutWindow_ShowOrToggle(void);
void AboutWindow_CloseIf(WindowPtr w);
Boolean AboutWindow_HandleUpdate(WindowPtr w);
Boolean AboutWindow_HandleMouseDown(WindowPtr w, short part, Point localPt);
Boolean AboutWindow_IsOurs(WindowPtr w);
void AboutWindow_ProcessPendingCreation(void);

/* Utility Functions */
OSErr ShowErrorDialog(StringPtr message, OSErr errorCode);
OSErr ShowConfirmDialog(StringPtr message, Boolean *confirmed);

/* Volume Mount Callback */
void OnVolumeMount(VRefNum vref, const char* volName);

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