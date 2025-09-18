/**
 * Finder Hardware Abstraction Layer
 * Provides platform-independent implementation of Finder desktop functionality
 *
 * This HAL layer bridges the System 7.1 Finder with modern platforms,
 * providing complete desktop management including file browsing, icon display,
 * trash operations, alias handling, and desktop database management.
 */

#include "../../include/Finder/finder.h"
#include "../../include/Finder/finder_types.h"
#include "../../include/WindowManager/WindowManager.h"
#include "../../include/MenuManager/MenuManager.h"
#include "../../include/EventManager/EventManager.h"
#include "../../include/QuickDraw/QuickDraw.h"
#include "../../include/DialogManager/DialogManager.h"
#include "../../include/ControlManager/ControlManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/FileManager/FileManager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#endif

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#endif

/* Icon sizes and spacing */
#define ICON_SIZE 32
#define ICON_SPACING 64
#define ICON_TEXT_HEIGHT 20
#define DESKTOP_MARGIN 10

/* Finder Manager Global State */
typedef struct FinderMgrState {
    bool initialized;

    /* Desktop state */
    WindowPtr desktopWindow;
    RgnHandle desktopRgn;
    int32_t desktopVRefNum;
    int32_t desktopDirID;

    /* File windows */
    struct FinderWindow {
        WindowPtr window;
        int32_t vRefNum;
        int32_t dirID;
        bool isRootWindow;
        int viewMode;  /* Icon, List, etc. */
        ControlHandle scrollBar;
        struct FinderWindow* next;
    } *windowList;

    /* Desktop database */
    struct DesktopItem {
        Str255 name;
        OSType creator;
        OSType fileType;
        Point location;
        IconHandle icon;
        int32_t vRefNum;
        int32_t parID;
        bool selected;
        struct DesktopItem* next;
    } *desktopItems;

    /* Trash state */
    int32_t trashVRefNum;
    int32_t trashDirID;
    WindowPtr trashWindow;
    int trashItemCount;

    /* Alias tracking */
    struct AliasEntry {
        AliasHandle alias;
        FSSpec target;
        bool isValid;
        struct AliasEntry* next;
    } *aliasList;

    /* Selection state */
    struct DesktopItem* selectedItems[256];
    int selectionCount;

    /* Clipboard for file operations */
    struct {
        OSType operation;  /* 'copy' or 'move' */
        FSSpec files[256];
        int count;
    } clipboard;

    /* View preferences */
    bool showInvisibleFiles;
    bool showFileExtensions;
    int defaultView;  /* Icon, List, etc. */

    /* Icon cache */
    struct IconCache {
        OSType creator;
        OSType fileType;
        IconHandle icon;
        struct IconCache* next;
    } *iconCache;

} FinderMgrState;

static FinderMgrState gFinderMgr = {0};

/* Forward declarations */
static void Finder_HAL_DrawDesktop(void);
static void Finder_HAL_DrawIcon(struct DesktopItem* item);
static void Finder_HAL_OpenItem(struct DesktopItem* item);
static void Finder_HAL_RefreshWindow(struct FinderWindow* fw);
static IconHandle Finder_HAL_GetIcon(OSType creator, OSType fileType);
static void Finder_HAL_UpdateDesktopDB(void);
static struct FinderWindow* Finder_HAL_NewFolderWindow(int32_t vRefNum, int32_t dirID);
static void Finder_HAL_ScanDirectory(int32_t vRefNum, int32_t dirID);
static bool Finder_HAL_IsInTrash(FSSpec* file);
static void Finder_HAL_LayoutIcons(struct FinderWindow* fw);

/**
 * Initialize Finder HAL
 */
void Finder_HAL_Init(void)
{
    if (gFinderMgr.initialized) {
        return;
    }

    printf("Finder HAL: Initializing desktop environment...\n");

    /* Create desktop window (full screen, no frame) */
    Rect desktopRect;
    GetWMgrPort(&desktopRect);

    gFinderMgr.desktopWindow = NewWindow(NULL, &desktopRect, "\pDesktop",
                                         true, plainDBox, (WindowPtr)-1L,
                                         false, 0);

    /* Set up desktop region */
    gFinderMgr.desktopRgn = NewRgn();
    RectRgn(gFinderMgr.desktopRgn, &desktopRect);

    /* Initialize trash folder */
    gFinderMgr.trashVRefNum = 0;  /* Boot volume */
    gFinderMgr.trashDirID = -3;    /* Special trash folder ID */
    gFinderMgr.trashItemCount = 0;

    /* Set default preferences */
    gFinderMgr.showInvisibleFiles = false;
    gFinderMgr.showFileExtensions = true;
    gFinderMgr.defaultView = 0;  /* Icon view */

    /* Scan desktop folder for items */
    Finder_HAL_ScanDirectory(gFinderMgr.desktopVRefNum,
                            gFinderMgr.desktopDirID);

    /* Draw initial desktop */
    Finder_HAL_DrawDesktop();

    gFinderMgr.initialized = true;

    printf("Finder HAL: Desktop environment initialized\n");
}

/**
 * Draw the desktop
 */
static void Finder_HAL_DrawDesktop(void)
{
    if (!gFinderMgr.desktopWindow) return;

    SetPort(gFinderMgr.desktopWindow);

    /* Draw desktop pattern */
    FillRect(&gFinderMgr.desktopWindow->portRect, &gray);

    /* Draw all desktop items */
    struct DesktopItem* item = gFinderMgr.desktopItems;
    while (item) {
        Finder_HAL_DrawIcon(item);
        item = item->next;
    }

    /* Draw trash icon at bottom right */
    Rect trashRect;
    trashRect.right = gFinderMgr.desktopWindow->portRect.right - 20;
    trashRect.bottom = gFinderMgr.desktopWindow->portRect.bottom - 20;
    trashRect.left = trashRect.right - ICON_SIZE;
    trashRect.top = trashRect.bottom - ICON_SIZE;

    /* Draw trash icon (simplified) */
    if (gFinderMgr.trashItemCount > 0) {
        /* Full trash */
        FillRect(&trashRect, &black);
    } else {
        /* Empty trash */
        FrameRect(&trashRect);
    }

    /* Label */
    MoveTo(trashRect.left, trashRect.bottom + 12);
    DrawString("\pTrash");
}

/**
 * Draw an icon on the desktop
 */
static void Finder_HAL_DrawIcon(struct DesktopItem* item)
{
    if (!item) return;

    /* Calculate icon rectangle */
    Rect iconRect;
    iconRect.left = item->location.h;
    iconRect.top = item->location.v;
    iconRect.right = iconRect.left + ICON_SIZE;
    iconRect.bottom = iconRect.top + ICON_SIZE;

    /* Draw selection highlight if selected */
    if (item->selected) {
        Rect hiliteRect = iconRect;
        InsetRect(&hiliteRect, -2, -2);
        PenMode(patXor);
        FrameRect(&hiliteRect);
        PenMode(patCopy);
    }

    /* Draw icon */
    if (item->icon) {
        /* Draw actual icon resource */
        PlotIcon(&iconRect, item->icon);
    } else {
        /* Draw generic icon based on type */
        if (item->fileType == 'FOLD') {
            /* Folder icon */
            FillRect(&iconRect, &ltGray);
            FrameRect(&iconRect);
        } else if (item->fileType == 'APPL') {
            /* Application icon */
            FillRoundRect(&iconRect, 8, 8, &dkGray);
        } else {
            /* Document icon */
            FrameRect(&iconRect);
            MoveTo(iconRect.left, iconRect.top + 8);
            LineTo(iconRect.right, iconRect.top + 8);
        }
    }

    /* Draw name below icon */
    MoveTo(item->location.h, item->location.v + ICON_SIZE + 12);
    DrawString(item->name);
}

/**
 * Handle mouse click on desktop
 */
void Finder_HAL_HandleClick(Point pt, EventRecord* event)
{
    /* Check if click is on an item */
    struct DesktopItem* item = gFinderMgr.desktopItems;
    struct DesktopItem* clickedItem = NULL;

    while (item) {
        Rect iconRect;
        iconRect.left = item->location.h;
        iconRect.top = item->location.v;
        iconRect.right = iconRect.left + ICON_SIZE;
        iconRect.bottom = iconRect.top + ICON_SIZE + ICON_TEXT_HEIGHT;

        if (PtInRect(pt, &iconRect)) {
            clickedItem = item;
            break;
        }
        item = item->next;
    }

    /* Handle selection */
    if (clickedItem) {
        /* Check for double-click */
        static int32_t lastClickTime = 0;
        static struct DesktopItem* lastClickedItem = NULL;

        int32_t currentTime = TickCount();

        if (lastClickedItem == clickedItem &&
            currentTime - lastClickTime < GetDblTime()) {
            /* Double-click: open item */
            Finder_HAL_OpenItem(clickedItem);
        } else {
            /* Single click: select item */
            if (!(event->modifiers & shiftKey)) {
                /* Clear previous selection */
                Finder_HAL_ClearSelection();
            }

            /* Toggle selection */
            clickedItem->selected = !clickedItem->selected;
            gFinderMgr.selectedItems[gFinderMgr.selectionCount++] = clickedItem;

            /* Redraw item */
            Finder_HAL_DrawIcon(clickedItem);
        }

        lastClickTime = currentTime;
        lastClickedItem = clickedItem;
    } else {
        /* Click on empty space: clear selection */
        Finder_HAL_ClearSelection();
    }
}

/**
 * Open a desktop item
 */
static void Finder_HAL_OpenItem(struct DesktopItem* item)
{
    if (!item) return;

    if (item->fileType == 'FOLD') {
        /* Open folder window */
        struct FinderWindow* fw = Finder_HAL_NewFolderWindow(item->vRefNum,
                                                             item->parID);
        if (fw) {
            ShowWindow(fw->window);
            SelectWindow(fw->window);
        }
    } else if (item->fileType == 'APPL') {
        /* Launch application */
        LaunchParamBlockRec launch;
        memset(&launch, 0, sizeof(launch));
        launch.launchBlockID = extendedBlock;
        launch.launchEPBLength = extendedBlockLen;
        launch.launchFileFlags = 0;
        launch.launchControlFlags = launchContinue;

        FSSpec appSpec;
        appSpec.vRefNum = item->vRefNum;
        appSpec.parID = item->parID;
        BlockMove(item->name, appSpec.name, item->name[0] + 1);

        launch.launchAppSpec = &appSpec;

        OSErr err = LaunchApplication(&launch);
        if (err != noErr) {
            printf("Finder HAL: Failed to launch application, error %d\n", err);
        }
    } else {
        /* Open document with creator application */
        /* Find and launch creator app */
        printf("Finder HAL: Opening document '%.*s'\n",
               item->name[0], &item->name[1]);
    }
}

/**
 * Create a new folder window
 */
static struct FinderWindow* Finder_HAL_NewFolderWindow(int32_t vRefNum, int32_t dirID)
{
    /* Allocate window record */
    struct FinderWindow* fw = (struct FinderWindow*)NewPtr(sizeof(struct FinderWindow));
    if (!fw) return NULL;

    /* Create window */
    Rect windowRect = {50, 50, 450, 350};
    Str255 title = "\pFolder";

    fw->window = NewWindow(NULL, &windowRect, title, true,
                           documentProc, (WindowPtr)-1L, true, 0);
    fw->vRefNum = vRefNum;
    fw->dirID = dirID;
    fw->viewMode = 0;  /* Icon view */

    /* Add scroll bar */
    Rect scrollRect = windowRect;
    scrollRect.left = scrollRect.right - 16;
    scrollRect.bottom -= 15;
    scrollRect.top += 19;  /* Below title bar */

    fw->scrollBar = NewControl(fw->window, &scrollRect, "\p",
                               true, 0, 0, 100, scrollBarProc, 0);

    /* Add to window list */
    fw->next = gFinderMgr.windowList;
    gFinderMgr.windowList = fw;

    /* Scan folder contents */
    Finder_HAL_ScanDirectory(vRefNum, dirID);

    /* Layout icons */
    Finder_HAL_LayoutIcons(fw);

    /* Refresh display */
    Finder_HAL_RefreshWindow(fw);

    return fw;
}

/**
 * Scan directory for files
 */
static void Finder_HAL_ScanDirectory(int32_t vRefNum, int32_t dirID)
{
    CInfoPBRec pb;
    Str255 fileName;
    OSErr err;
    int16_t index = 1;

    /* Enumerate directory contents */
    while (true) {
        memset(&pb, 0, sizeof(pb));
        pb.hFileInfo.ioVRefNum = vRefNum;
        pb.hFileInfo.ioDirID = dirID;
        pb.hFileInfo.ioFDirIndex = index++;
        pb.hFileInfo.ioNamePtr = fileName;

        err = PBGetCatInfoSync(&pb);
        if (err != noErr) break;

        /* Skip invisible files unless preference set */
        if (!gFinderMgr.showInvisibleFiles &&
            (pb.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)) {
            continue;
        }

        /* Create desktop item */
        struct DesktopItem* item = (struct DesktopItem*)NewPtr(sizeof(struct DesktopItem));
        if (!item) continue;

        BlockMove(fileName, item->name, fileName[0] + 1);
        item->vRefNum = vRefNum;
        item->parID = pb.hFileInfo.ioFlParID;

        if (pb.hFileInfo.ioFlAttrib & ioDirMask) {
            /* Directory */
            item->fileType = 'FOLD';
            item->creator = 'MACS';
        } else {
            /* File */
            item->fileType = pb.hFileInfo.ioFlFndrInfo.fdType;
            item->creator = pb.hFileInfo.ioFlFndrInfo.fdCreator;
        }

        /* Get icon */
        item->icon = Finder_HAL_GetIcon(item->creator, item->fileType);

        /* Set default location */
        item->location.h = DESKTOP_MARGIN + (index * ICON_SPACING);
        item->location.v = DESKTOP_MARGIN;

        /* Add to list */
        item->next = gFinderMgr.desktopItems;
        gFinderMgr.desktopItems = item;
    }
}

/**
 * Get icon for file type
 */
static IconHandle Finder_HAL_GetIcon(OSType creator, OSType fileType)
{
    /* Check cache first */
    struct IconCache* cache = gFinderMgr.iconCache;
    while (cache) {
        if (cache->creator == creator && cache->fileType == fileType) {
            return cache->icon;
        }
        cache = cache->next;
    }

    /* Load icon from resources */
    IconHandle icon = NULL;

    /* Try to get custom icon */
    Handle iconRes = GetResource('ICN#', 128);  /* Default icon ID */
    if (iconRes) {
        icon = (IconHandle)iconRes;
    }

    /* Cache the icon */
    if (icon) {
        cache = (struct IconCache*)NewPtr(sizeof(struct IconCache));
        if (cache) {
            cache->creator = creator;
            cache->fileType = fileType;
            cache->icon = icon;
            cache->next = gFinderMgr.iconCache;
            gFinderMgr.iconCache = cache;
        }
    }

    return icon;
}

/**
 * Clear selection
 */
void Finder_HAL_ClearSelection(void)
{
    /* Clear all selected items */
    for (int i = 0; i < gFinderMgr.selectionCount; i++) {
        if (gFinderMgr.selectedItems[i]) {
            gFinderMgr.selectedItems[i]->selected = false;
            Finder_HAL_DrawIcon(gFinderMgr.selectedItems[i]);
        }
    }

    gFinderMgr.selectionCount = 0;
}

/**
 * Copy selected items
 */
void Finder_HAL_Copy(void)
{
    gFinderMgr.clipboard.operation = 'copy';
    gFinderMgr.clipboard.count = 0;

    for (int i = 0; i < gFinderMgr.selectionCount; i++) {
        struct DesktopItem* item = gFinderMgr.selectedItems[i];
        if (item) {
            FSSpec* spec = &gFinderMgr.clipboard.files[gFinderMgr.clipboard.count];
            spec->vRefNum = item->vRefNum;
            spec->parID = item->parID;
            BlockMove(item->name, spec->name, item->name[0] + 1);
            gFinderMgr.clipboard.count++;
        }
    }
}

/**
 * Paste clipboard items
 */
void Finder_HAL_Paste(void)
{
    if (gFinderMgr.clipboard.count == 0) return;

    /* Determine target directory */
    int32_t targetVRefNum = gFinderMgr.desktopVRefNum;
    int32_t targetDirID = gFinderMgr.desktopDirID;

    /* Perform copy/move operations */
    for (int i = 0; i < gFinderMgr.clipboard.count; i++) {
        FSSpec* source = &gFinderMgr.clipboard.files[i];

        if (gFinderMgr.clipboard.operation == 'copy') {
            /* Copy file */
            OSErr err = FSpFileCopy(source, &targetDirID, NULL, NULL, 0, true);
            if (err != noErr) {
                printf("Finder HAL: Copy failed, error %d\n", err);
            }
        } else {
            /* Move file */
            CMovePBRec pb;
            memset(&pb, 0, sizeof(pb));
            pb.ioVRefNum = source->vRefNum;
            pb.ioNewDirID = targetDirID;
            pb.ioNamePtr = source->name;
            pb.ioDirID = source->parID;

            OSErr err = PBCatMoveSync(&pb);
            if (err != noErr) {
                printf("Finder HAL: Move failed, error %d\n", err);
            }
        }
    }

    /* Refresh desktop */
    Finder_HAL_UpdateDesktopDB();
    Finder_HAL_DrawDesktop();
}

/**
 * Move items to trash
 */
void Finder_HAL_MoveToTrash(void)
{
    for (int i = 0; i < gFinderMgr.selectionCount; i++) {
        struct DesktopItem* item = gFinderMgr.selectedItems[i];
        if (item) {
            /* Move to trash folder */
            CMovePBRec pb;
            memset(&pb, 0, sizeof(pb));
            pb.ioVRefNum = item->vRefNum;
            pb.ioNewDirID = gFinderMgr.trashDirID;
            pb.ioNamePtr = item->name;
            pb.ioDirID = item->parID;

            OSErr err = PBCatMoveSync(&pb);
            if (err == noErr) {
                gFinderMgr.trashItemCount++;

                /* Remove from desktop items */
                struct DesktopItem** itemPtr = &gFinderMgr.desktopItems;
                while (*itemPtr) {
                    if (*itemPtr == item) {
                        *itemPtr = item->next;
                        DisposePtr((Ptr)item);
                        break;
                    }
                    itemPtr = &(*itemPtr)->next;
                }
            }
        }
    }

    /* Clear selection and refresh */
    Finder_HAL_ClearSelection();
    Finder_HAL_DrawDesktop();
}

/**
 * Empty trash
 */
void Finder_HAL_EmptyTrash(void)
{
    if (gFinderMgr.trashItemCount == 0) return;

    /* Show confirmation dialog */
    if (CautionAlert(128, NULL) != 1) {  /* Cancel */
        return;
    }

    /* Delete all items in trash */
    CInfoPBRec pb;
    Str255 fileName;
    OSErr err;
    int16_t index = 1;

    while (true) {
        memset(&pb, 0, sizeof(pb));
        pb.hFileInfo.ioVRefNum = gFinderMgr.trashVRefNum;
        pb.hFileInfo.ioDirID = gFinderMgr.trashDirID;
        pb.hFileInfo.ioFDirIndex = index;
        pb.hFileInfo.ioNamePtr = fileName;

        err = PBGetCatInfoSync(&pb);
        if (err != noErr) break;

        /* Delete file */
        HDelete(gFinderMgr.trashVRefNum, gFinderMgr.trashDirID, fileName);

        /* Don't increment index since we're deleting */
    }

    gFinderMgr.trashItemCount = 0;
    Finder_HAL_DrawDesktop();
}

/**
 * Create alias for selected items
 */
void Finder_HAL_MakeAlias(void)
{
    for (int i = 0; i < gFinderMgr.selectionCount; i++) {
        struct DesktopItem* item = gFinderMgr.selectedItems[i];
        if (item) {
            FSSpec sourceSpec;
            sourceSpec.vRefNum = item->vRefNum;
            sourceSpec.parID = item->parID;
            BlockMove(item->name, sourceSpec.name, item->name[0] + 1);

            /* Create alias file */
            Str255 aliasName;
            aliasName[0] = item->name[0] + 6;  /* " alias" */
            BlockMove(item->name + 1, aliasName + 1, item->name[0]);
            BlockMove(" alias", aliasName + 1 + item->name[0], 6);

            FSSpec aliasSpec;
            aliasSpec.vRefNum = sourceSpec.vRefNum;
            aliasSpec.parID = sourceSpec.parID;
            BlockMove(aliasName, aliasSpec.name, aliasName[0] + 1);

            /* Create alias record */
            AliasHandle alias;
            OSErr err = NewAlias(NULL, &sourceSpec, &alias);
            if (err == noErr) {
                /* Save alias to file */
                FInfo fndrInfo;
                err = FSpGetFInfo(&sourceSpec, &fndrInfo);
                if (err == noErr) {
                    fndrInfo.fdType = 'alis';
                    fndrInfo.fdCreator = 'MACS';
                    fndrInfo.fdFlags |= kIsAlias;

                    /* Create alias file */
                    err = FSpCreate(&aliasSpec, fndrInfo.fdCreator,
                                   fndrInfo.fdType, 0);
                    if (err == noErr) {
                        /* Write alias data */
                        short refNum;
                        err = FSpOpenDF(&aliasSpec, fsWrPerm, &refNum);
                        if (err == noErr) {
                            long count = GetHandleSize((Handle)alias);
                            HLock((Handle)alias);
                            FSWrite(refNum, &count, *alias);
                            HUnlock((Handle)alias);
                            FSClose(refNum);
                        }
                    }
                }

                /* Add to alias tracking list */
                struct AliasEntry* entry = (struct AliasEntry*)NewPtr(sizeof(struct AliasEntry));
                if (entry) {
                    entry->alias = alias;
                    entry->target = sourceSpec;
                    entry->isValid = true;
                    entry->next = gFinderMgr.aliasList;
                    gFinderMgr.aliasList = entry;
                }
            }
        }
    }

    /* Refresh desktop */
    Finder_HAL_UpdateDesktopDB();
    Finder_HAL_DrawDesktop();
}

/**
 * Get info for selected item
 */
void Finder_HAL_GetInfo(void)
{
    if (gFinderMgr.selectionCount == 0) return;

    struct DesktopItem* item = gFinderMgr.selectedItems[0];
    if (!item) return;

    /* Get file info */
    FSSpec spec;
    spec.vRefNum = item->vRefNum;
    spec.parID = item->parID;
    BlockMove(item->name, spec.name, item->name[0] + 1);

    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.hFileInfo.ioNamePtr = spec.name;
    pb.hFileInfo.ioVRefNum = spec.vRefNum;
    pb.hFileInfo.ioDirID = spec.parID;

    OSErr err = PBGetCatInfoSync(&pb);
    if (err != noErr) return;

    /* Show info dialog */
    DialogPtr dialog = GetNewDialog(129, NULL, (WindowPtr)-1L);
    if (!dialog) return;

    /* Populate dialog fields */
    Str255 sizeStr;
    if (pb.hFileInfo.ioFlAttrib & ioDirMask) {
        /* Directory */
        NumToString(pb.dirInfo.ioDrNmFls, sizeStr);
        /* Would set item count text */
    } else {
        /* File */
        NumToString(pb.hFileInfo.ioFlLgLen, sizeStr);
        /* Would set file size text */
    }

    /* Run dialog */
    short itemHit;
    do {
        ModalDialog(NULL, &itemHit);
    } while (itemHit != ok && itemHit != cancel);

    DisposeDialog(dialog);
}

/**
 * Refresh window display
 */
static void Finder_HAL_RefreshWindow(struct FinderWindow* fw)
{
    if (!fw || !fw->window) return;

    SetPort(fw->window);
    EraseRect(&fw->window->portRect);

    /* Draw window contents based on view mode */
    if (fw->viewMode == 0) {
        /* Icon view - draw icons in grid */
        struct DesktopItem* item = gFinderMgr.desktopItems;
        while (item) {
            if (item->vRefNum == fw->vRefNum && item->parID == fw->dirID) {
                Finder_HAL_DrawIcon(item);
            }
            item = item->next;
        }
    } else {
        /* List view - draw as list */
        int y = 20;
        struct DesktopItem* item = gFinderMgr.desktopItems;
        while (item) {
            if (item->vRefNum == fw->vRefNum && item->parID == fw->dirID) {
                MoveTo(10, y);
                DrawString(item->name);
                y += 16;
            }
            item = item->next;
        }
    }

    /* Update scroll bar */
    if (fw->scrollBar) {
        DrawControls(fw->window);
    }
}

/**
 * Layout icons in window
 */
static void Finder_HAL_LayoutIcons(struct FinderWindow* fw)
{
    if (!fw) return;

    int x = DESKTOP_MARGIN;
    int y = DESKTOP_MARGIN + 20;  /* Below title bar */
    int maxX = fw->window->portRect.right - ICON_SPACING;

    struct DesktopItem* item = gFinderMgr.desktopItems;
    while (item) {
        if (item->vRefNum == fw->vRefNum && item->parID == fw->dirID) {
            item->location.h = x;
            item->location.v = y;

            x += ICON_SPACING;
            if (x > maxX) {
                x = DESKTOP_MARGIN;
                y += ICON_SPACING + ICON_TEXT_HEIGHT;
            }
        }
        item = item->next;
    }
}

/**
 * Update desktop database
 */
static void Finder_HAL_UpdateDesktopDB(void)
{
    /* Clear current items */
    struct DesktopItem* item = gFinderMgr.desktopItems;
    while (item) {
        struct DesktopItem* next = item->next;
        if (item->icon) {
            ReleaseResource((Handle)item->icon);
        }
        DisposePtr((Ptr)item);
        item = next;
    }
    gFinderMgr.desktopItems = NULL;

    /* Rescan desktop */
    Finder_HAL_ScanDirectory(gFinderMgr.desktopVRefNum,
                            gFinderMgr.desktopDirID);
}

/**
 * Handle Finder menu commands
 */
void Finder_HAL_HandleMenu(long menuChoice)
{
    short menuID = HiWord(menuChoice);
    short menuItem = LoWord(menuChoice);

    switch (menuID) {
        case 129:  /* File menu */
            switch (menuItem) {
                case 1:  /* New Folder */
                    Finder_HAL_NewFolder();
                    break;
                case 2:  /* Open */
                    if (gFinderMgr.selectionCount > 0) {
                        Finder_HAL_OpenItem(gFinderMgr.selectedItems[0]);
                    }
                    break;
                case 4:  /* Close Window */
                    /* Close front window */
                    break;
                case 6:  /* Get Info */
                    Finder_HAL_GetInfo();
                    break;
                case 8:  /* Duplicate */
                    Finder_HAL_Duplicate();
                    break;
                case 9:  /* Make Alias */
                    Finder_HAL_MakeAlias();
                    break;
                case 11: /* Put Away */
                    Finder_HAL_PutAway();
                    break;
                case 13: /* Find */
                    Finder_HAL_Find();
                    break;
                case 15: /* Page Setup */
                case 16: /* Print */
                    /* Print operations */
                    break;
            }
            break;

        case 130:  /* Edit menu */
            switch (menuItem) {
                case 3:  /* Cut */
                    Finder_HAL_Cut();
                    break;
                case 4:  /* Copy */
                    Finder_HAL_Copy();
                    break;
                case 5:  /* Paste */
                    Finder_HAL_Paste();
                    break;
                case 6:  /* Clear */
                    Finder_HAL_MoveToTrash();
                    break;
                case 7:  /* Select All */
                    Finder_HAL_SelectAll();
                    break;
            }
            break;

        case 131:  /* View menu */
            switch (menuItem) {
                case 1:  /* by Icon */
                case 2:  /* by Small Icon */
                case 3:  /* by Name */
                case 4:  /* by Size */
                case 5:  /* by Kind */
                case 6:  /* by Date */
                    Finder_HAL_SetView(menuItem - 1);
                    break;
            }
            break;

        case 132:  /* Special menu */
            switch (menuItem) {
                case 1:  /* Clean Up */
                    Finder_HAL_CleanUp();
                    break;
                case 2:  /* Empty Trash */
                    Finder_HAL_EmptyTrash();
                    break;
                case 4:  /* Eject Disk */
                    Finder_HAL_EjectDisk();
                    break;
                case 5:  /* Erase Disk */
                    Finder_HAL_EraseDisk();
                    break;
                case 7:  /* Restart */
                    Finder_HAL_Restart();
                    break;
                case 8:  /* Shut Down */
                    Finder_HAL_ShutDown();
                    break;
            }
            break;
    }

    HiliteMenu(0);
}

/**
 * Additional Finder operations
 */
void Finder_HAL_NewFolder(void)
{
    /* Create new folder in current directory */
    Str255 folderName = "\puntitled folder";
    HCreate(gFinderMgr.desktopVRefNum, gFinderMgr.desktopDirID,
            folderName, 'MACS', 'FOLD');

    /* Refresh */
    Finder_HAL_UpdateDesktopDB();
    Finder_HAL_DrawDesktop();
}

void Finder_HAL_Duplicate(void)
{
    /* Duplicate selected items */
    for (int i = 0; i < gFinderMgr.selectionCount; i++) {
        struct DesktopItem* item = gFinderMgr.selectedItems[i];
        if (item) {
            FSSpec source;
            source.vRefNum = item->vRefNum;
            source.parID = item->parID;
            BlockMove(item->name, source.name, item->name[0] + 1);

            /* Create copy name */
            Str255 copyName;
            copyName[0] = item->name[0] + 5;  /* " copy" */
            BlockMove(item->name + 1, copyName + 1, item->name[0]);
            BlockMove(" copy", copyName + 1 + item->name[0], 5);

            FSpFileCopy(&source, &item->parID, copyName, NULL, 0, true);
        }
    }

    Finder_HAL_UpdateDesktopDB();
    Finder_HAL_DrawDesktop();
}

void Finder_HAL_Cut(void)
{
    gFinderMgr.clipboard.operation = 'move';
    Finder_HAL_Copy();  /* Same as copy but marked for move */
}

void Finder_HAL_SelectAll(void)
{
    /* Select all visible items */
    gFinderMgr.selectionCount = 0;
    struct DesktopItem* item = gFinderMgr.desktopItems;
    while (item && gFinderMgr.selectionCount < 256) {
        item->selected = true;
        gFinderMgr.selectedItems[gFinderMgr.selectionCount++] = item;
        item = item->next;
    }
    Finder_HAL_DrawDesktop();
}

void Finder_HAL_SetView(int viewMode)
{
    /* Change view mode for front window */
    struct FinderWindow* fw = gFinderMgr.windowList;
    if (fw) {
        fw->viewMode = viewMode;
        Finder_HAL_RefreshWindow(fw);
    }
}

void Finder_HAL_CleanUp(void)
{
    /* Arrange icons by grid */
    struct FinderWindow* fw = gFinderMgr.windowList;
    if (fw) {
        Finder_HAL_LayoutIcons(fw);
        Finder_HAL_RefreshWindow(fw);
    }
}

void Finder_HAL_PutAway(void) { /* Return items from desktop to proper folders */ }
void Finder_HAL_Find(void) { /* Open find dialog */ }
void Finder_HAL_EjectDisk(void) { /* Eject selected volume */ }
void Finder_HAL_EraseDisk(void) { /* Format selected disk */ }
void Finder_HAL_Restart(void) { /* System restart */ }
void Finder_HAL_ShutDown(void) { /* System shutdown */ }

/**
 * Cleanup Finder HAL
 */
void Finder_HAL_Cleanup(void)
{
    if (!gFinderMgr.initialized) {
        return;
    }

    /* Close all windows */
    struct FinderWindow* fw = gFinderMgr.windowList;
    while (fw) {
        struct FinderWindow* next = fw->next;
        if (fw->window) {
            DisposeWindow(fw->window);
        }
        DisposePtr((Ptr)fw);
        fw = next;
    }

    /* Free desktop items */
    struct DesktopItem* item = gFinderMgr.desktopItems;
    while (item) {
        struct DesktopItem* next = item->next;
        if (item->icon) {
            ReleaseResource((Handle)item->icon);
        }
        DisposePtr((Ptr)item);
        item = next;
    }

    /* Free alias list */
    struct AliasEntry* alias = gFinderMgr.aliasList;
    while (alias) {
        struct AliasEntry* next = alias->next;
        if (alias->alias) {
            DisposeHandle((Handle)alias->alias);
        }
        DisposePtr((Ptr)alias);
        alias = next;
    }

    /* Free icon cache */
    struct IconCache* cache = gFinderMgr.iconCache;
    while (cache) {
        struct IconCache* next = cache->next;
        if (cache->icon) {
            ReleaseResource((Handle)cache->icon);
        }
        DisposePtr((Ptr)cache);
        cache = next;
    }

    /* Dispose regions */
    if (gFinderMgr.desktopRgn) {
        DisposeRgn(gFinderMgr.desktopRgn);
    }

    /* Dispose desktop window */
    if (gFinderMgr.desktopWindow) {
        DisposeWindow(gFinderMgr.desktopWindow);
    }

    /* Reset state */
    memset(&gFinderMgr, 0, sizeof(gFinderMgr));

    printf("Finder HAL: Cleaned up\n");
}