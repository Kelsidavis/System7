/*
 * folder_window.c - Folder window content display with double-click support
 *
 * Displays folder contents in windows opened from desktop icons.
 * Supports single-click selection and double-click to open items.
 */

/* Include stdio.h first to avoid FILE type conflict with SystemTypes.h */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Define _FILE_DEFINED to prevent SystemTypes.h from redefining FILE */
#define _FILE_DEFINED

#include "SystemTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include "ToolboxCompat.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "Finder/Icon/icon_types.h"
#include "Finder/Icon/icon_label.h"
#include "Finder/Icon/icon_system.h"
#include "Finder/finder.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"
#include "System71StdLib.h"
#include "Finder/FinderLogging.h"
#include "EventManager/EventManager.h"
#include "ControlPanels/DesktopPatterns.h"
#include "ControlPanels/Sound.h"
#include "ControlPanels/Mouse.h"
#include "ControlPanels/Keyboard.h"
#include "ControlPanels/ControlStrip.h"
#include "Datetime/datetime_cdev.h"
#include "ProcessMgr/ProcessTypes.h"
extern OSErr LaunchApplication(LaunchParamBlockRec* launchParams);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void FrameRect(const Rect* r);
extern void ClipRect(const Rect* r);
extern void EraseRect(const Rect* r);
extern void GlobalToLocal(Point* pt);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern UInt32 TickCount(void);
extern UInt32 GetDblTime(void);
/* PostEvent declared in EventManager.h */
extern QDGlobals qd;
extern void GetMouse(Point* pt);
extern volatile UInt8 gCurrentButtons;
extern short FindWindow(Point thePoint, WindowPtr* theWindow);
extern bool VFS_Delete(VRefNum vref, FileID id);

/* Drag threshold for distinguishing clicks from drags */
#define kDragThreshold 4

#define kControlPanelsDirID    (-100)
#define kControlPanelDesktopID (-101)
#define kControlPanelTimeID    (-102)
#define kControlPanelSoundID   (-103)
#define kControlPanelMouseID   (-104)
#define kControlPanelKeyboardID (-105)
#define kControlStripID         (-106)

/* Folder item representation with file system integration */
typedef struct FolderItem {
    char name[256];
    Boolean isFolder;      /* true = folder, false = document/app */
    uint32_t size;         /* File size in bytes (0 for folders) */
    uint32_t modTime;      /* Modification time (seconds since 1904) */
    short label;           /* Label color index (0-7: None, Essential, Hot, etc.) */
    Point position;        /* position in window (for icon view) */
    FileID fileID;         /* File system ID (CNID) */
    DirID parentID;        /* Parent directory ID */
    uint32_t type;         /* File type (OSType) */
    uint32_t creator;      /* Creator code (OSType) */
} FolderItem;

/* Folder window state (per window) */
typedef struct FolderWindowState {
    FolderItem* items;     /* Array of items in this folder */
    short itemCount;       /* Number of items */
    short selectedIndex;   /* Currently selected item (-1 = none) - primary selection for keyboard navigation */
    Boolean* selectedItems; /* Array of selection flags (true = selected) for multi-select */
    Boolean isDragging;    /* Currently dragging an item */
    UInt32 lastClickTime;  /* For double-click detection */
    Point lastClickPos;    /* Last click position */
    short lastClickIndex;  /* Index of last clicked item */
    VRefNum vref;          /* Volume reference for this folder */
    DirID currentDir;      /* Directory ID being displayed */
    Point dragStartGlobal; /* Global coordinates where drag started */
    short draggingIndex;   /* Index of item being dragged (-1 = none) */
} FolderWindowState;

/* Global folder window states (indexed by window pointer for now) */
#define MAX_FOLDER_WINDOWS 16
static struct {
    WindowPtr window;
    FolderWindowState state;
} gFolderWindows[MAX_FOLDER_WINDOWS] = {0};

/* Get or create state for a folder window */
FolderWindowState* GetFolderState(WindowPtr w);
void InitializeFolderContents(WindowPtr w, Boolean isTrash);
static void GhostEraseIf(void);  /* Forward declaration for ghost system */

/* Draw a simple file/folder icon */
static void DrawFileIcon(short x, short y, Boolean isFolder)
{
    
    Rect iconRect;
    SetRect(&iconRect, x, y, x + 32, y + 32);

    FINDER_LOG_DEBUG("[ICON] res=%d at(l)={%d,%d} port=0x%08x\n",
                     isFolder ? 1 : 2, x, y, (unsigned int)P2UL(NULL));

    if (isFolder) {
        /* Draw folder shape - paint with gray then frame */
        extern void PaintRect(const Rect* r);
        extern void InvertRect(const Rect* r);
        PaintRect(&iconRect);  /* Fill with black */
        FrameRect(&iconRect);
        /* Tab on top */
        Rect tabRect;
        SetRect(&tabRect, x, y - 4, x + 12, y);
        PaintRect(&tabRect);  /* Fill with black */
        FrameRect(&tabRect);
    } else {
        /* Draw document shape - just frame it (white background) */
        FrameRect(&iconRect);
        /* Folded corner */
        MoveTo(x + 24, y);
        LineTo(x + 32, y + 8);
        LineTo(x + 24, y + 8);
        LineTo(x + 24, y);
    }
}

/* Draw folder window contents - Content Only, No Chrome */
void DrawFolderWindowContents(WindowPtr window, Boolean isTrash)
{
    if (!window) return;

    FINDER_LOG_DEBUG("Finder: Drawing contents of '%s'\n",
                  isTrash ? "Trash" : "Macintosh HD");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* CRITICAL: Log port and coordinate mapping for debugging */
    FINDER_LOG_DEBUG("DrawFolder: window=0x%08x savePort=0x%08x\n",
                     (unsigned int)P2UL(window),
                     (unsigned int)P2UL(savePort));
    FINDER_LOG_DEBUG("DrawFolder: portBits.bounds(GLOBAL)=(%d,%d,%d,%d)\n",
                  window->port.portBits.bounds.left, window->port.portBits.bounds.top,
                  window->port.portBits.bounds.right, window->port.portBits.bounds.bottom);

    /* Use window's portRect which is in LOCAL (port-relative) coordinates */
    /* In Mac Toolbox, portRect should always start at (0,0) */
    Rect localBounds = window->port.portRect;
    FINDER_LOG_DEBUG("DrawFolder: portRect(LOCAL)=(%d,%d,%d,%d)\n",
                  localBounds.left, localBounds.top, localBounds.right, localBounds.bottom);

    /* Calculate content area in LOCAL coordinates */
    /* Content = full port minus title bar (20px) */
    Rect contentRect;
    contentRect.left = localBounds.left;
    contentRect.top = 20;  /* Skip title bar */
    contentRect.right = localBounds.right;
    contentRect.bottom = localBounds.bottom;

    FINDER_LOG_DEBUG("Finder: portRect (local) = (%d,%d,%d,%d)\n",
                  localBounds.left, localBounds.top, localBounds.right, localBounds.bottom);

    FINDER_LOG_DEBUG("Finder: contentRect (local) = (%d,%d,%d,%d)\n",
                  contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    /* Set clipping to content area to prevent drawing outside bounds */
    ClipRect(&contentRect);

    /* Fill background with white - EraseRect uses LOCAL coords, DrawPrimitive converts to GLOBAL */
    extern void EraseRect(const Rect* r);
    EraseRect(&contentRect);

    FINDER_LOG_DEBUG("Finder: Erased contentRect (%d,%d,%d,%d) for white backfill\n",
                  contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    /* Text drawing disabled until Font Manager is linked */

    if (isTrash) {
        /* Draw trash contents */
        MoveTo(contentRect.left + 10, contentRect.top + 30);
        DrawText("Trash is empty", 0, 14);
        FINDER_LOG_DEBUG("[TEXT] 'Trash is empty' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.top + 30, 0, 9);

        MoveTo(contentRect.left + 10, contentRect.top + 50);
        DrawText("Drag items here to delete them", 0, 30);
        FINDER_LOG_DEBUG("[TEXT] 'Drag items here to delete them' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.top + 50, 0, 9);
    } else {
        /* Draw volume contents - sample items in icon grid */
        /* Ensure margins: 80px left (room for labels), 30px top */
        short x = contentRect.left + 80;
        short y = contentRect.top + 30;
        short iconSpacing = 100;
        short rowHeight = 90;

        /* System Folder - icon 32px wide, label ~78px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 23, y + 40);
        DrawText("System Folder", 0, 13);
        FINDER_LOG_DEBUG("[TEXT] 'System Folder' at(l)={%d,%d} font=%d size=%d\n", x - 23, y + 40, 0, 9);

        x += iconSpacing;

        /* Applications folder - label ~72px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 20, y + 40);
        DrawText("Applications", 0, 12);
        FINDER_LOG_DEBUG("[TEXT] 'Applications' at(l)={%d,%d} font=%d size=%d\n", x - 20, y + 40, 0, 9);

        x += iconSpacing;

        /* Documents folder - label ~54px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 11, y + 40);
        DrawText("Documents", 0, 9);
        FINDER_LOG_DEBUG("[TEXT] 'Documents' at(l)={%d,%d} font=%d size=%d\n", x - 11, y + 40, 0, 9);

        /* Second row */
        x = contentRect.left + 80;
        y += rowHeight;

        /* SimpleText document - label ~60px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 14, y + 40);
        DrawText("ReadMe.txt", 0, 10);
        FINDER_LOG_DEBUG("[TEXT] 'ReadMe.txt' at(l)={%d,%d} font=%d size=%d\n", x - 14, y + 40, 0, 9);

        x += iconSpacing;

        /* TeachText document - label ~84px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 26, y + 40);
        DrawText("About System 7", 0, 14);
        FINDER_LOG_DEBUG("[TEXT] 'About System 7' at(l)={%d,%d} font=%d size=%d\n", x - 26, y + 40, 0, 9);

        /* Show disk space at bottom */
        MoveTo(contentRect.left + 10, contentRect.bottom - 10);
        DrawText("5 items     42.3 MB in disk     193.7 MB available", 0, 52);
        FINDER_LOG_DEBUG("[TEXT] 'disk info' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.bottom - 10, 0, 9);
    }

    SetPort(savePort);
}

/* Helper: Find folder window state slot */
FolderWindowState* GetFolderState(WindowPtr w) {
        FINDER_LOG_DEBUG("GetFolderState: ENTRY\n");
    if (!w) {
        FINDER_LOG_DEBUG("GetFolderState: w is NULL\n");
        return NULL;
    }

    /* Search for existing slot */
    for (int i = 0; i < MAX_FOLDER_WINDOWS; i++) {
        if (gFolderWindows[i].window == w) {
            FINDER_LOG_DEBUG("GetFolderState: Found existing slot %d\n", i);
            return &gFolderWindows[i].state;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < MAX_FOLDER_WINDOWS; i++) {
        if (gFolderWindows[i].window == NULL) {
            FINDER_LOG_DEBUG("GetFolderState: Creating new slot %d, refCon=0x%08x\n", i, (unsigned int)w->refCon);
            gFolderWindows[i].window = w;
            gFolderWindows[i].state.items = NULL;
            gFolderWindows[i].state.itemCount = 0;
            gFolderWindows[i].state.selectedIndex = -1;
            gFolderWindows[i].state.selectedItems = NULL;
            gFolderWindows[i].state.isDragging = false;
            gFolderWindows[i].state.lastClickTime = 0;
            gFolderWindows[i].state.lastClickPos.h = 0;
            gFolderWindows[i].state.lastClickPos.v = 0;
            gFolderWindows[i].state.lastClickIndex = -1;
            gFolderWindows[i].state.vref = 0;
            gFolderWindows[i].state.currentDir = 0;
            gFolderWindows[i].state.dragStartGlobal.h = 0;
            gFolderWindows[i].state.dragStartGlobal.v = 0;
            gFolderWindows[i].state.draggingIndex = -1;

            /* Initialize folder contents */
            FINDER_LOG_DEBUG("GetFolderState: About to call InitializeFolderContents\n");
            InitializeFolderContents(w, (w->refCon == 'TRSH'));
            FINDER_LOG_DEBUG("GetFolderState: InitializeFolderContents returned\n");
            return &gFolderWindows[i].state;
        }
    }

    FINDER_LOG_DEBUG("GetFolderState: No slots available!\n");
    return NULL;  /* No slots available */
}

/* Initialize folder contents from VFS - Extended version with custom dirID */
void InitializeFolderContentsEx(WindowPtr w, Boolean isTrash, VRefNum vref, DirID dirID) {
        FolderWindowState* state = NULL;

    FINDER_LOG_DEBUG("InitializeFolderContentsEx: ENTRY, w=0x%08x isTrash=%d vref=%d dirID=%d\n",
                 (unsigned int)w, (int)isTrash, (int)vref, (int)dirID);

    /* Find the state we just created */
    for (int i = 0; i < MAX_FOLDER_WINDOWS; i++) {
        if (gFolderWindows[i].window == w) {
            state = &gFolderWindows[i].state;
            break;
        }
    }

    if (!state) {
        return;
    }

    state->vref = vref;
    state->currentDir = dirID;

    if (state->items) {
        DisposePtr((Ptr)state->items);
        state->items = NULL;
        state->itemCount = 0;
    }

    if (state->selectedItems) {
        DisposePtr((Ptr)state->selectedItems);
        state->selectedItems = NULL;
    }

    if (isTrash) {
        /* Trash folder - enumerate actual trash directory */
        FINDER_LOG_DEBUG("InitializeFolderContentsEx: enumerating trash folder\n");

        /* Get trash directory ID using FindFolder */
        extern OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder, SInt16* foundVRefNum, SInt32* foundDirID);
        SInt16 trashVRefNum;
        SInt32 trashDirID;
        OSErr err = FindFolder(kOnSystemDisk, kTrashFolderType, kDontCreateFolder, &trashVRefNum, &trashDirID);

        if (err != noErr || trashDirID == 0) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: FindFolder failed or returned invalid ID, err=%d dirID=%d\n",
                         err, (int)trashDirID);
            state->itemCount = 0;
            state->items = NULL;
            return;
        }

        FINDER_LOG_DEBUG("InitializeFolderContentsEx: trash folder found, vref=%d dirID=%d\n",
                     (int)trashVRefNum, (int)trashDirID);

        /* Update state with actual trash directory info */
        state->vref = trashVRefNum;
        state->currentDir = trashDirID;

        /* Enumerate trash directory contents using VFS */
        #define MAX_TRASH_ITEMS 128
        CatEntry entries[MAX_TRASH_ITEMS];
        int count = 0;

        if (!VFS_Enumerate(trashVRefNum, trashDirID, entries, MAX_TRASH_ITEMS, &count)) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: VFS_Enumerate failed for trash\n");
            state->itemCount = 0;
            state->items = NULL;
            return;
        }

        FINDER_LOG_DEBUG("InitializeFolderContentsEx: trash contains %d items\n", count);

        if (count == 0) {
            state->itemCount = 0;
            state->items = NULL;
            return;
        }

        /* Allocate item array */
        state->items = (FolderItem*)NewPtr(sizeof(FolderItem) * count);
        if (!state->items) {
            FINDER_LOG_WARN("InitializeFolderContentsEx: allocation failed for %d trash items\n", count);
            state->itemCount = 0;
            return;
        }

        state->itemCount = count;

        /* Convert CatEntry to FolderItem */
        for (int i = 0; i < count; i++) {
            /* Copy name (ensure null termination) */
            size_t nameLen = strlen(entries[i].name);
            if (nameLen >= 256) nameLen = 255;
            memcpy(state->items[i].name, entries[i].name, nameLen);
            state->items[i].name[nameLen] = '\0';

            /* File system metadata */
            state->items[i].isFolder = (entries[i].kind == kNodeDir);
            state->items[i].size = entries[i].size;
            state->items[i].modTime = entries[i].modTime;
            state->items[i].label = 0;
            state->items[i].fileID = entries[i].id;
            state->items[i].parentID = entries[i].parent;
            state->items[i].type = entries[i].type;
            state->items[i].creator = entries[i].creator;

            FINDER_LOG_DEBUG("InitializeFolderContentsEx: trash item %d: %s (id=%d, folder=%d)\n",
                         i, state->items[i].name, (int)state->items[i].fileID, state->items[i].isFolder);
        }

        /* Layout in grid (same as other folders) */
        const int startX = 80;
        const int startY = 30;
        const int colSpacing = 100;
        const int rowHeight = 90;
        const int numCols = 3;

        for (int i = 0; i < count; i++) {
            int col = i % numCols;
            int row = i / numCols;
            state->items[i].position.h = startX + (col * colSpacing);
            state->items[i].position.v = startY + (row * rowHeight);
        }

        /* Allocate selection array */
        state->selectedItems = (Boolean*)NewPtr(count * sizeof(Boolean));
        if (!state->selectedItems) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: Failed to allocate selectedItems for trash folder\n");
            state->itemCount = 0;
            return;
        }
        for (int i = 0; i < count; i++) {
            state->selectedItems[i] = false;
        }

        FINDER_LOG_DEBUG("InitializeFolderContentsEx: trash folder populated with %d items\n", count);
        return;
    }

    /* Handle Applications folder with virtual apps */
    if (dirID == 18) {
        state->itemCount = 3;  /* SimpleText, TextEdit, MacPaint */
        state->items = (FolderItem*)NewPtr(sizeof(FolderItem) * state->itemCount);
        if (!state->items) {
            state->itemCount = 0;
            return;
        }

        /* SimpleText */
        FolderItem *simpleText = &state->items[0];
        memset(simpleText, 0, sizeof(FolderItem));
        strncpy(simpleText->name, "SimpleText", sizeof(simpleText->name) - 1);
        simpleText->isFolder = false;
        simpleText->fileID = 200;  /* Virtual ID */
        simpleText->parentID = dirID;
        simpleText->type = 'APPL';
        simpleText->creator = 'ttxt';

        /* TextEdit */
        FolderItem *textEdit = &state->items[1];
        memset(textEdit, 0, sizeof(FolderItem));
        strncpy(textEdit->name, "TextEdit", sizeof(textEdit->name) - 1);
        textEdit->isFolder = false;
        textEdit->fileID = 201;  /* Virtual ID */
        textEdit->parentID = dirID;
        textEdit->type = 'APPL';
        textEdit->creator = 'tedt';

        /* MacPaint */
        FolderItem *macPaint = &state->items[2];
        memset(macPaint, 0, sizeof(FolderItem));
        strncpy(macPaint->name, "MacPaint", sizeof(macPaint->name) - 1);
        macPaint->isFolder = false;
        macPaint->fileID = 202;  /* Virtual ID */
        macPaint->parentID = dirID;
        macPaint->type = 'APPL';
        macPaint->creator = 'MAPP';

        const int startX = 80;
        const int startY = 30;
        const int colSpacing = 100;
        const int rowHeight = 90;
        const int maxCols = 3;

        for (int i = 0; i < state->itemCount; i++) {
            int col = i % maxCols;
            int row = i / maxCols;
            state->items[i].position.h = startX + (col * colSpacing);
            state->items[i].position.v = startY + (row * rowHeight);
        }

        /* Allocate selection array */
        state->selectedItems = (Boolean*)NewPtr(sizeof(Boolean) * state->itemCount);
        if (!state->selectedItems) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: Failed to allocate selectedItems for Applications folder\n");
            state->itemCount = 0;
            return;
        }
        for (int i = 0; i < state->itemCount; i++) {
            state->selectedItems[i] = false;
        }

        FINDER_LOG_DEBUG("InitializeFolderContentsEx: populated Applications folder with %d items\n",
                         state->itemCount);
        return;
    }

    if (dirID == kControlPanelsDirID) {
        state->itemCount = 6;
        state->items = (FolderItem*)NewPtr(sizeof(FolderItem) * state->itemCount);
        if (!state->items) {
            state->itemCount = 0;
            return;
        }

        /* Desktop Patterns */
        FolderItem *desktop = &state->items[0];
        memset(desktop, 0, sizeof(FolderItem));
        strncpy(desktop->name, "Desktop Patterns", sizeof(desktop->name) - 1);
        desktop->isFolder = false;
        desktop->fileID = kControlPanelDesktopID;
        desktop->parentID = dirID;
        desktop->type = 'APPL';
        desktop->creator = 'cdev';

        /* Date & Time */
        FolderItem *dateTime = &state->items[1];
        memset(dateTime, 0, sizeof(FolderItem));
        strncpy(dateTime->name, "Date & Time", sizeof(dateTime->name) - 1);
        dateTime->isFolder = false;
        dateTime->fileID = kControlPanelTimeID;
        dateTime->parentID = dirID;
        dateTime->type = 'APPL';
        dateTime->creator = 'cdev';

        /* Sound */
        FolderItem *sound = &state->items[2];
        memset(sound, 0, sizeof(FolderItem));
        strncpy(sound->name, "Sound", sizeof(sound->name) - 1);
        sound->isFolder = false;
        sound->fileID = kControlPanelSoundID;
        sound->parentID = dirID;
        sound->type = 'APPL';
        sound->creator = 'cdev';

        /* Mouse */
        FolderItem *mouse = &state->items[3];
        memset(mouse, 0, sizeof(FolderItem));
        strncpy(mouse->name, "Mouse", sizeof(mouse->name) - 1);
        mouse->isFolder = false;
        mouse->fileID = kControlPanelMouseID;
        mouse->parentID = dirID;
        mouse->type = 'APPL';
        mouse->creator = 'cdev';

        /* Keyboard */
        FolderItem *keyboard = &state->items[4];
        memset(keyboard, 0, sizeof(FolderItem));
        strncpy(keyboard->name, "Keyboard", sizeof(keyboard->name) - 1);
        keyboard->isFolder = false;
        keyboard->fileID = kControlPanelKeyboardID;
        keyboard->parentID = dirID;
        keyboard->type = 'APPL';
        keyboard->creator = 'cdev';

        FolderItem *strip = &state->items[5];
        memset(strip, 0, sizeof(FolderItem));
        strncpy(strip->name, "Control Strip", sizeof(strip->name) - 1);
        strip->isFolder = false;
        strip->fileID = kControlStripID;
        strip->parentID = dirID;
        strip->type = 'APPL';
        strip->creator = 'cdev';

        const int startX = 80;
        const int startY = 30;
        const int colSpacing = 100;
        const int rowHeight = 90;
        const int maxCols = 3;

        for (int i = 0; i < state->itemCount; i++) {
            int col = i % maxCols;
            int row = i / maxCols;
            state->items[i].position.h = startX + (col * colSpacing);
            state->items[i].position.v = startY + (row * rowHeight);
        }

        /* Allocate selection array */
        state->selectedItems = (Boolean*)NewPtr(sizeof(Boolean) * state->itemCount);
        if (!state->selectedItems) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: Failed to allocate selectedItems for Control Panels folder\n");
            state->itemCount = 0;
            return;
        }
        for (int i = 0; i < state->itemCount; i++) {
            state->selectedItems[i] = false;
        }

        FINDER_LOG_DEBUG("InitializeFolderContentsEx: populated Control Panels folder with %d items\n",
                         state->itemCount);
        return;
    } else {
        /* Use provided directory ID */
        /* Enumerate directory contents using VFS */
        #define MAX_ITEMS 128
        CatEntry entries[MAX_ITEMS];
        int count = 0;

        FINDER_LOG_DEBUG("InitializeFolderContents: calling VFS_Enumerate\n");

        if (!VFS_Enumerate(vref, state->currentDir, entries, MAX_ITEMS, &count)) {
            FINDER_LOG_DEBUG("InitializeFolderContents: VFS_Enumerate failed\n");
            state->itemCount = 0;
            state->items = NULL;
            return;
        }

        FINDER_LOG_DEBUG("InitializeFolderContents: VFS_Enumerate OK, count=%d\n", count);

        int extraItems = (dirID == 2) ? 1 : 0;
        int totalItems = count + extraItems;
        if (totalItems == 0) {
            state->itemCount = 0;
            state->items = NULL;
            return;
        }

        /* Allocate item array */
        state->items = (FolderItem*)NewPtr(sizeof(FolderItem) * totalItems);
        if (!state->items) {
            FINDER_LOG_WARN("FW: malloc failed for %d items\n", count);
            state->itemCount = 0;
            return;
        }

        state->itemCount = totalItems;

        /* Convert CatEntry to FolderItem and lay out in grid
         * Grid: 3 columns, spacing 100px horizontal, 90px vertical
         * Start at (80, 30) for margins */
        for (int i = 0; i < count; i++) {
            /* Copy name (ensure null termination) */
            size_t nameLen = strlen(entries[i].name);
            if (nameLen >= 256) nameLen = 255;
            memcpy(state->items[i].name, entries[i].name, nameLen);
            state->items[i].name[nameLen] = '\0';

            /* File system metadata */
            state->items[i].isFolder = (entries[i].kind == kNodeDir);
            state->items[i].size = entries[i].size;
            state->items[i].modTime = entries[i].modTime;
            state->items[i].label = 0;  /* Default: no label */
            state->items[i].fileID = entries[i].id;
            state->items[i].parentID = entries[i].parent;
            state->items[i].type = entries[i].type;
            state->items[i].creator = entries[i].creator;
        }

        if (extraItems) {
            FolderItem *cpFolder = &state->items[count];
            memset(cpFolder, 0, sizeof(FolderItem));
            strncpy(cpFolder->name, "Control Panels", sizeof(cpFolder->name) - 1);
            cpFolder->isFolder = true;
            cpFolder->fileID = kControlPanelsDirID;
            cpFolder->parentID = state->currentDir;
            cpFolder->type = 'CPLF';
            cpFolder->creator = 'cdev';
            FINDER_LOG_DEBUG("FW: Added virtual 'Control Panels' folder entry\n");
        }

        const int startX = 80;
        const int startY = 30;
        const int colSpacing = 100;
        const int rowHeight = 90;
        const int maxCols = 3;

        for (int i = 0; i < state->itemCount; i++) {
            int col = i % maxCols;
            int row = i / maxCols;
            state->items[i].position.h = startX + (col * colSpacing);
            state->items[i].position.v = startY + (row * rowHeight);
            FINDER_LOG_DEBUG("FW: Item %d: '%s' %s id=%d pos=(%d,%d)\n",
                         i, state->items[i].name,
                         state->items[i].isFolder ? "DIR" : "FILE",
                         state->items[i].fileID,
                         state->items[i].position.h, state->items[i].position.v);
        }

        /* Allocate selection array */
        state->selectedItems = (Boolean*)NewPtr(sizeof(Boolean) * state->itemCount);
        if (!state->selectedItems) {
            FINDER_LOG_DEBUG("InitializeFolderContentsEx: Failed to allocate selectedItems for VFS directory\n");
            state->itemCount = 0;
            return;
        }
        for (int i = 0; i < state->itemCount; i++) {
            state->selectedItems[i] = false;
        }

        FINDER_LOG_DEBUG("FW: Initialized %d items from VFS\n", count);
    }
}

/* Backward-compatible wrapper for InitializeFolderContentsEx - opens root directory */
void InitializeFolderContents(WindowPtr w, Boolean isTrash) {
    VRefNum vref = VFS_GetBootVRef();
    DirID rootDir = 2;  /* HFS root directory CNID is always 2 */
    InitializeFolderContentsEx(w, isTrash, vref, rootDir);
}

/* Open a folder window for a specific directory */
WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID, ConstStr255Param title) {
        extern WindowPtr NewWindow(void *, const Rect *, ConstStr255Param, Boolean, short,
                               WindowPtr, Boolean, long);
    extern void ShowWindow(WindowPtr);
    extern void SelectWindow(WindowPtr);

    FINDER_LOG_DEBUG("FolderWindow_OpenFolder: vref=%d dirID=%d\n", (int)vref, (int)dirID);

    /* Create window with same geometry as generic folder windows */
    static Rect r;
    r.left = 10;
    r.top = 80;
    r.right = 490;
    r.bottom = 420;

    WindowPtr w = NewWindow(NULL, &r, title, true, 0 /* documentProc */, (WindowPtr)-1, true, 'DISK');

    if (!w) {
        FINDER_LOG_DEBUG("FolderWindow_OpenFolder: Failed to create window\n");
        return NULL;
    }

    /* Initialize folder state with specific directory */
    FolderWindowState* state = GetFolderState(w);
    if (state) {
        /* Override the default initialization with specific directory */
        InitializeFolderContentsEx(w, false, vref, dirID);
    }

    ShowWindow(w);
    SelectWindow(w);

    FINDER_LOG_DEBUG("FolderWindow_OpenFolder: Window created successfully\n");
    return w;
}

/* Icon hit-testing: find icon at point (local window coordinates)
 * Returns index of item or -1 if no hit.
 * Icon rect is 32x32, label is below with 20px left/right padding.
 */
static short FW_IconAtPoint(WindowPtr w, Point localPt) {
    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return -1;

    FINDER_LOG_DEBUG("FW: hit test at local (%d,%d), itemCount=%d\n",
                 localPt.h, localPt.v, state->itemCount);

    for (short i = 0; i < state->itemCount; i++) {
        /* Icon rect (32x32 centered at position.h+16) */
        Rect iconRect;
        iconRect.left = state->items[i].position.h;
        iconRect.top = state->items[i].position.v;
        iconRect.right = state->items[i].position.h + 32;
        iconRect.bottom = state->items[i].position.v + 32;

        /* Label rect (text centered, with padding)
         * Label baseline is at iconTop + 40, background extends from (baseline - 12) to (baseline + 2) */
        int textWidth, textHeight;
        IconLabel_Measure(state->items[i].name, &textWidth, &textHeight);

        Rect labelRect;
        int centerX = state->items[i].position.h + 16;
        labelRect.left = centerX - (textWidth / 2) - 2;
        labelRect.top = state->items[i].position.v + 28;  /* 40 - 12 = 28 */
        labelRect.right = centerX + (textWidth / 2) + 2;
        labelRect.bottom = state->items[i].position.v + 42;  /* 40 + 2 = 42 */

        /* Check if point is in icon or label */
        if ((localPt.h >= iconRect.left && localPt.h < iconRect.right &&
             localPt.v >= iconRect.top && localPt.v < iconRect.bottom) ||
            (localPt.h >= labelRect.left && localPt.h < labelRect.right &&
             localPt.v >= labelRect.top && localPt.v < labelRect.bottom)) {
            FINDER_LOG_DEBUG("FW: hit index %d name='%s'\n", i, state->items[i].name);
            return i;
        }
    }

    return -1;
}

/* Track folder item drag - detects drag threshold and sets drag state
 * Returns true if drag was started, false if click (button released before threshold)
 */
static Boolean TrackFolderItemDrag(WindowPtr w, FolderWindowState* state, short itemIndex, Point startGlobal) {
    if (!w || !state || itemIndex < 0 || itemIndex >= state->itemCount) {
        return false;
    }

    FINDER_LOG_DEBUG("FW: TrackFolderItemDrag: item %d '%s' from global (%d,%d)\n",
                 itemIndex, state->items[itemIndex].name, startGlobal.h, startGlobal.v);

    /* Ensure window port is set for GetMouse calls */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)w);

    /* Wait for drag threshold or button release */
    Point cur;
    extern void ProcessModernInput(void);

    while ((gCurrentButtons & 1) != 0) {
        ProcessModernInput();  /* Update gCurrentButtons */
        GetMouse(&cur);  /* Returns global coordinates */
        FINDER_LOG_DEBUG("FW: Threshold check - GetMouse returned cur=(%d,%d) (global)\n", cur.h, cur.v);

        /* Check if we've exceeded drag threshold */
        int dx = cur.h - startGlobal.h;
        int dy = cur.v - startGlobal.v;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        if (dx >= kDragThreshold || dy >= kDragThreshold) {
            /* Threshold exceeded - start drag */
            FINDER_LOG_DEBUG("FW: Drag threshold exceeded: delta=(%d,%d)\n",
                         cur.h - startGlobal.h, cur.v - startGlobal.v);

            state->isDragging = true;
            state->draggingIndex = itemIndex;
            state->dragStartGlobal = startGlobal;

            FINDER_LOG_DEBUG("FW: DRAG STARTED: item='%s' fileID=%d vref=%d dir=%d\n",
                         state->items[itemIndex].name,
                         state->items[itemIndex].fileID,
                         state->vref,
                         state->currentDir);

            /* Calculate ghost rect from item position (32x32 icon) */
            FolderItem* item = &state->items[itemIndex];

            FINDER_LOG_DEBUG("FW: Item '%s' position (local) = (%d,%d)\n",
                         item->name, item->position.h, item->position.v);
            FINDER_LOG_DEBUG("FW: Window bounds (global) = (%d,%d,%d,%d)\n",
                         w->port.portBits.bounds.left, w->port.portBits.bounds.top,
                         w->port.portBits.bounds.right, w->port.portBits.bounds.bottom);

            Rect ghost;
            ghost.left = item->position.h;
            ghost.top = item->position.v;
            ghost.right = ghost.left + 32;
            ghost.bottom = ghost.top + 32;

            FINDER_LOG_DEBUG("FW: Ghost rect BEFORE LocalToGlobal (local) = (%d,%d,%d,%d)\n",
                         ghost.left, ghost.top, ghost.right, ghost.bottom);

            /* Convert to global coordinates */
            Point topLeft, botRight;
            topLeft.h = ghost.left;
            topLeft.v = ghost.top;
            botRight.h = ghost.right;
            botRight.v = ghost.bottom;

            FINDER_LOG_DEBUG("FW: topLeft BEFORE L2G: h=%d v=%d\n", topLeft.h, topLeft.v);
            FINDER_LOG_DEBUG("FW: portBits.bounds: left=%d top=%d\n",
                         w->port.portBits.bounds.left, w->port.portBits.bounds.top);

            LocalToGlobal(&topLeft);
            LocalToGlobal(&botRight);

            FINDER_LOG_DEBUG("FW: topLeft AFTER L2G: h=%d v=%d\n", topLeft.h, topLeft.v);

            ghost.left = topLeft.h;
            ghost.top = topLeft.v;
            ghost.right = botRight.h;
            ghost.bottom = botRight.v;

            FINDER_LOG_DEBUG("FW: Ghost rect AFTER LocalToGlobal (global) X=%d-%d Y=%d-%d\n",
                         ghost.left, ghost.right, ghost.top, ghost.bottom);

            /* Add padding like desktop (left -20, right +20, bottom +16) */
            ghost.left -= 20;
            ghost.right += 20;
            ghost.bottom += 16;

            FINDER_LOG_DEBUG("FW: Ghost rect with padding (global) X=%d-%d Y=%d-%d\n",
                         ghost.left, ghost.right, ghost.top, ghost.bottom);

            /* Switch to screen port for XOR ghost drawing - we need GetMouse to return global coords */
            GrafPort screenDragPort;
            OpenPort(&screenDragPort);
            screenDragPort.portBits = qd.screenBits;
            screenDragPort.portRect = qd.screenBits.bounds;
            SetPort(&screenDragPort);
            ClipRect(&qd.screenBits.bounds);

            /* Show initial ghost */
            extern void Desktop_GhostShowAt(const Rect* r);
            Desktop_GhostShowAt(&ghost);
            FINDER_LOG_DEBUG("FW: Ghost visible, entering drag loop\n");

            /* Track drag with visual feedback */
            Point lastPos = cur;  /* cur is already in global coords from threshold check */
            FINDER_LOG_DEBUG("FW: Starting drag loop, lastPos=(%d,%d) in GLOBAL coords\n", lastPos.h, lastPos.v);
            while ((gCurrentButtons & 1) != 0) {
                ProcessModernInput();  /* Update gCurrentButtons */
                GetMouse(&cur);
                /* g_mousePos is in window-local coords, need to convert using WINDOW port */
                SetPort((GrafPtr)w);  /* Temporarily switch back to window port for LocalToGlobal */
                LocalToGlobal(&cur);
                SetPort(&screenDragPort);  /* Switch back to screen port for drawing */
                FINDER_LOG_DEBUG("FW: GetMouse+LocalToGlobal returned cur=(%d,%d)\n", cur.h, cur.v);

                /* Update ghost position if mouse moved */
                if (cur.h != lastPos.h || cur.v != lastPos.v) {
                    extern void OffsetRect(Rect* r, short dh, short dv);
                    FINDER_LOG_DEBUG("FW: Mouse moved, offsetting ghost by (%d,%d)\n",
                                 cur.h - lastPos.h, cur.v - lastPos.v);
                    OffsetRect(&ghost, cur.h - lastPos.h, cur.v - lastPos.v);
                    Desktop_GhostShowAt(&ghost);
                    lastPos = cur;
                }
            }

            /* Erase ghost before processing drop */
            extern void Desktop_GhostEraseIf(void);
            Desktop_GhostEraseIf();

            /* Restore port */
            SetPort((GrafPtr)w);

            FINDER_LOG_DEBUG("FW: Drag ended at global (%d,%d)\n", cur.h, cur.v);

            /* Phase 4: Check for drop-to-trash first */
            if (Desktop_IsOverTrash(cur)) {
                FINDER_LOG_DEBUG("FW: DROP TO TRASH detected - deleting '%s' (fileID=%d, vref=%d)\n",
                             item->name, item->fileID, state->vref);

                bool deleted = VFS_Delete(state->vref, item->fileID);

                if (deleted) {
                    FINDER_LOG_DEBUG("FW: File deleted successfully from VFS\n");

                    /* Remove item from folder window display by shifting array */
                    for (int i = itemIndex; i < state->itemCount - 1; i++) {
                        state->items[i] = state->items[i + 1];
                    }
                    state->itemCount--;

                    /* Clear selection if we deleted the selected item */
                    if (state->selectedIndex == itemIndex) {
                        state->selectedIndex = -1;
                    } else if (state->selectedIndex > itemIndex) {
                        state->selectedIndex--;  /* Adjust index after shift */
                    }

                    FINDER_LOG_DEBUG("FW: Item removed from display, new itemCount=%d\n", state->itemCount);

                    /* Request window redraw */
                    PostEvent(updateEvt, (UInt32)(uintptr_t)w);
                } else {
                    FINDER_LOG_DEBUG("FW: ERROR: VFS_Delete failed\n");
                }
            } else {
                /* Phase 3: Check for drop-to-desktop and create alias */
                WindowPtr hitWindow = NULL;
                short partCode = FindWindow(cur, &hitWindow);

                if (partCode == inDesk) {
                    /* Dropped on desktop - create alias */
                    Boolean isFolder = item->isFolder;

                    FINDER_LOG_DEBUG("FW: DROP TO DESKTOP detected - creating alias for '%s'\n", item->name);

                    OSErr err = Desktop_AddAliasIcon(item->name, cur, item->fileID,
                                                    state->vref, isFolder);

                    if (err == noErr) {
                        FINDER_LOG_DEBUG("FW: Alias created successfully on desktop\n");
                    } else {
                        FINDER_LOG_DEBUG("FW: ERROR: Desktop_AddAliasIcon failed with error %d\n", err);
                    }
                } else {
                    FINDER_LOG_DEBUG("FW: Dropped over window (partCode=%d) - no action\n", partCode);
                }
            }

            /* Clear drag state */
            state->isDragging = false;
            state->draggingIndex = -1;

            SetPort(savePort);
            return true;  /* Drag occurred */
        }
    }

    /* Button released before threshold - treat as click */
    FINDER_LOG_DEBUG("FW: Button released before threshold - treating as click\n");
    SetPort(savePort);
    return false;
}

/* Handle click in folder window - called from EventDispatcher
 * Point is in GLOBAL coordinates, isDoubleClick from event system
 */
Boolean HandleFolderWindowClick(WindowPtr w, EventRecord *ev, Boolean isDoubleClick) {
    if (!w || !ev) return false;

    FolderWindowState* state = GetFolderState(w);
    if (!state) return false;

    /* Convert global mouse to local window coordinates
     *
     * CRITICAL: With Direct Framebuffer approach, GlobalToLocal is a no-op.
     * Use GlobalToLocalWindow which uses contRgn for actual conversion.
     */
    Point localPt = ev->where;  /* Start with global coords */
    extern void GlobalToLocalWindow(WindowPtr window, Point *pt);
    GlobalToLocalWindow(w, &localPt);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)w);

    FINDER_LOG_DEBUG("FW: down at (global %d,%d) local (%d,%d) dbl=%d\n",
                 ev->where.h, ev->where.v, localPt.h, localPt.v, isDoubleClick);

    /* Hit test against icons */
    short hitIndex = FW_IconAtPoint(w, localPt);

    if (hitIndex == -1) {
        /* Clicked empty space - deselect */
        if (state->selectedIndex != -1) {
            FINDER_LOG_DEBUG("FW: deselect (empty click)\n");
            state->selectedIndex = -1;
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);
        }
        FINDER_LOG_DEBUG("FW: empty click - clearing lastClickIndex (was %d)\n", state->lastClickIndex);
        state->lastClickIndex = -1;
        SetPort(savePort);
        return true;
    }

    /* Clicked an icon */
    UInt32 currentTime = TickCount();
    UInt32 timeSinceLast = currentTime - state->lastClickTime;
    Boolean isSameIcon = (hitIndex == state->lastClickIndex);
    Boolean withinDblTime = (timeSinceLast <= GetDblTime());

    /* Check if this could potentially be a double-click based on timing and position
     * Note: We check this BEFORE isDoubleClick flag because we need to handle
     * the second click of a double-click specially */
    Boolean couldBeDoubleClick = isSameIcon && withinDblTime;
    Boolean isRealDoubleClick = couldBeDoubleClick && isDoubleClick;

    FINDER_LOG_DEBUG("FW: hit index %d, same=%d, dt=%d, threshold=%d, couldBeDbl=%d, realDbl=%d\n",
                 hitIndex, isSameIcon, (int)timeSinceLast, (int)GetDblTime(), couldBeDoubleClick, isRealDoubleClick);

    if (isRealDoubleClick) {
        /* DOUBLE-CLICK on same icon: open it */
        FINDER_LOG_DEBUG("FW: OPEN_FOLDER \"%s\"\n", state->items[hitIndex].name);

        if (state->items[hitIndex].isFolder) {
            /* Open folder: create new window showing the subfolder's contents */
            extern WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID, ConstStr255Param title);

            /* Convert C string to Pascal string for window title */
            Str255 pTitle;
            size_t len = strlen(state->items[hitIndex].name);
            if (len > 255) len = 255;
            pTitle[0] = (unsigned char)len;
            memcpy(&pTitle[1], state->items[hitIndex].name, len);

            /* Open folder window with the subfolder's directory ID */
            WindowPtr newWin = FolderWindow_OpenFolder(state->vref, state->items[hitIndex].fileID, pTitle);
            if (newWin) {
        FINDER_LOG_DEBUG("FW: opened subfolder window 0x%08x (dirID=%d)\n",
                         (unsigned int)P2UL(newWin),
                         (int)state->items[hitIndex].fileID);
                /* Post update for the new window and old window */
                PostEvent(updateEvt, (UInt32)(uintptr_t)newWin);
                PostEvent(updateEvt, (UInt32)(uintptr_t)w);
            }
        } else {
            /* Document/app: check if it's a text file */
            Boolean isTextFile = false;
            const char* name = state->items[hitIndex].name;

            /* Check file type */
            if (state->items[hitIndex].type == 'TEXT') {
                isTextFile = true;
            }

            /* Check filename patterns for text files */
            if (!isTextFile) {
                int len = strlen(name);
                /* Check for .txt extension */
                if (len >= 4) {
                    const char* ext = name + len - 4;
                    if (ext[0] == '.' &&
                        (ext[1] == 't' || ext[1] == 'T') &&
                        (ext[2] == 'x' || ext[2] == 'X') &&
                        (ext[3] == 't' || ext[3] == 'T')) {
                        isTextFile = true;
                    }
                }
                /* Check for "readme" or "README" prefix */
                if (!isTextFile && len >= 6) {
                    const char* prefix = name;
                    if ((prefix[0] == 'r' || prefix[0] == 'R') &&
                        (prefix[1] == 'e' || prefix[1] == 'E') &&
                        (prefix[2] == 'a' || prefix[2] == 'A') &&
                        (prefix[3] == 'd' || prefix[3] == 'D') &&
                        (prefix[4] == 'm' || prefix[4] == 'M') &&
                        (prefix[5] == 'e' || prefix[5] == 'E')) {
                        isTextFile = true;
                    }
                }
            }

            if (isTextFile) {
                FINDER_LOG_DEBUG("FW: Opening text file \"%s\" with SimpleText\n", name);
                /* Launch SimpleText if not already running */
                extern void SimpleText_Launch(void);
                extern Boolean SimpleText_IsRunning(void);
                extern void SimpleText_OpenFile(const char* path);

                FINDER_LOG_DEBUG("FW: name BEFORE Launch = '%s'\n", name);
                if (!SimpleText_IsRunning()) {
                    SimpleText_Launch();
                    FINDER_LOG_DEBUG("FW: Launched SimpleText\n");
                    FINDER_LOG_DEBUG("FW: name AFTER Launch = '%s'\n", name);
                }
                FINDER_LOG_DEBUG("FW: name AFTER check = '%s'\n", name);

                /* Build full path to the file */
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "/%s", name);  /* Simple path for now */
                FINDER_LOG_DEBUG("FW: fullPath after snprintf = '%s'\n", fullPath);

                /* Load file content into SimpleText window */
                SimpleText_OpenFile(fullPath);
                FINDER_LOG_DEBUG("FW: called SimpleText_OpenFile with '%s'\n", fullPath);
                FINDER_LOG_DEBUG("FW: Opened file '%s' in SimpleText\n", name);
            } else if (state->items[hitIndex].type == 'APPL') {
                /* Application file */
                if (strcmp(name, "TextEdit") == 0) {
                    FINDER_LOG_DEBUG("FW: Launching TextEdit application\n");
                    extern void TextEdit_InitApp(void);
                    TextEdit_InitApp();
                } else if (strcmp(name, "SimpleText") == 0) {
                    FINDER_LOG_DEBUG("FW: Launching SimpleText application\n");
                    extern void SimpleText_Launch(void);
                    SimpleText_Launch();
                } else if (strcmp(name, "MacPaint") == 0) {
                    FINDER_LOG_DEBUG("FW: Launching MacPaint application\n");
                    extern void MacPaint_Launch(void);
                    MacPaint_Launch();
                } else if (strcmp(name, "Desktop Patterns") == 0) {
                    FINDER_LOG_DEBUG("FW: Opening Desktop Patterns control panel\n");
                    OpenDesktopCdev();
                } else if (strcmp(name, "Date & Time") == 0) {
                    FINDER_LOG_DEBUG("FW: Opening Date & Time control panel\n");
                    DateTimePanel_Open();
                } else if (strcmp(name, "Sound") == 0) {
                    FINDER_LOG_DEBUG("FW: Opening Sound control panel\n");
                    SoundPanel_Open();
                } else if (strcmp(name, "Mouse") == 0) {
                    FINDER_LOG_DEBUG("FW: Opening Mouse control panel\n");
                    MousePanel_Open();
                } else if (strcmp(name, "Keyboard") == 0) {
                    FINDER_LOG_DEBUG("FW: Opening Keyboard control panel\n");
                    KeyboardPanel_Open();
                } else if (strcmp(name, "Control Strip") == 0) {
                    FINDER_LOG_DEBUG("FW: Toggling Control Strip palette\n");
                    ControlStrip_Toggle();
                } else {
                    /* Generic application launcher */
                    FINDER_LOG_DEBUG("FW: Launching generic application \"%s\"\n", name);

                    LaunchParamBlockRec launchParams;
                    FSSpec appSpec;
                    OSErr err;

                    /* Build FSSpec for the application file */
                    /* For now, use simple path - in full implementation would resolve full path */
                    appSpec.vRefNum = 0;  /* Default volume */
                    appSpec.parID = 0;     /* Root directory for now */

                    /* Convert name to Pascal string */
                    UInt8 nameLen = strlen(name);
                    if (nameLen > 63) nameLen = 63;
                    appSpec.name[0] = nameLen;
                    memcpy(&appSpec.name[1], name, nameLen);

                    /* Set up launch parameters */
                    memset(&launchParams, 0, sizeof(launchParams));
                    launchParams.launchAppSpec = &appSpec;
                    launchParams.launchPreferredSize = 512 * 1024;  /* 512KB default */
                    launchParams.launchControlFlags = 0;  /* Default flags */

                    /* Launch the application */
                    err = LaunchApplication(&launchParams);
                    if (err != noErr) {
                        FINDER_LOG_DEBUG("FW: Failed to launch \"%s\" (err=%d)\n", name, err);
                    } else {
                        FINDER_LOG_DEBUG("FW: Successfully launched \"%s\"\n", name);
                    }
                }
            } else {
                /* Document file - try to open with associated application */
                FINDER_LOG_DEBUG("FW: Opening document \"%s\"\n", name);

                /* For now, route common document types to their apps */
                Boolean handled = false;

                /* Check file extension for text files (.txt, README, etc.) */
                if (!handled) {
                    int len = strlen(name);
                    Boolean isTextDoc = false;

                    if (len >= 4) {
                        const char* ext = name + len - 4;
                        if (ext[0] == '.' &&
                            (ext[1] == 't' || ext[1] == 'T') &&
                            (ext[2] == 'x' || ext[2] == 'X') &&
                            (ext[3] == 't' || ext[3] == 'T')) {
                            isTextDoc = true;
                        }
                    }

                    if (isTextDoc) {
                        FINDER_LOG_DEBUG("FW: Opening text document with SimpleText\n");
                        extern void SimpleText_Launch(void);
                        extern Boolean SimpleText_IsRunning(void);
                        extern void SimpleText_OpenFile(const char* path);

                        if (!SimpleText_IsRunning()) {
                            SimpleText_Launch();
                        }

                        /* Build path and open file */
                        char fullPath[512];
                        snprintf(fullPath, sizeof(fullPath), "/%s", name);
                        SimpleText_OpenFile(fullPath);
                        handled = true;
                    }
                }

                if (!handled) {
                    FINDER_LOG_DEBUG("FW: No application associated with document \"%s\"\n", name);
                }
            }
        }

        /* Clear double-click tracking */
        state->lastClickIndex = -1;
        state->lastClickTime = 0;
    } else if (couldBeDoubleClick && !isDoubleClick) {
        /* This is potentially the second click of a double-click but the event
         * system hasn't flagged it as a double-click yet. Just select and track timing. */
        FINDER_LOG_DEBUG("FW: potential double-click (waiting for confirmation), selecting icon %d\n", hitIndex);

        short oldSel = state->selectedIndex;
        state->selectedIndex = hitIndex;
        state->lastClickIndex = hitIndex;
        state->lastClickTime = currentTime;

        /* Update multi-selection array */
        if (state->selectedItems) {
            for (short i = 0; i < state->itemCount; i++) {
                state->selectedItems[i] = (i == hitIndex);
            }
        }

        if (oldSel != hitIndex) {
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);
        }
    } else {
        /* SINGLE-CLICK: Track for potential drag, or select if just a click */
        FINDER_LOG_DEBUG("FW: single-click on icon %d, tracking for drag...\n", hitIndex);

        /* Call drag tracking - this will wait for threshold or button release */
        Boolean wasDrag = TrackFolderItemDrag(w, state, hitIndex, ev->where);

        if (!wasDrag) {
            /* No drag occurred - treat as normal click/selection */
            short oldSel = state->selectedIndex;
            state->selectedIndex = hitIndex;
            state->lastClickIndex = hitIndex;
            state->lastClickTime = currentTime;

            /* Update multi-selection array */
            if (state->selectedItems) {
                for (short i = 0; i < state->itemCount; i++) {
                    state->selectedItems[i] = (i == hitIndex);
                }
            }

            (void)oldSel;  /* Used only in debug logging */
            FINDER_LOG_DEBUG("FW: select %d -> %d, SET lastClickIndex=%d, lastClickTime=%lu\n",
                         oldSel, hitIndex, hitIndex, (unsigned long)currentTime);

            /* Post update to redraw selection */
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);
        } else {
            /* Drag occurred - selection/timing was handled by drag tracking */
            FINDER_LOG_DEBUG("FW: drag completed, NOT setting lastClick values\n");
        }
    }

    SetPort(savePort);
    return true;
}

/* Safe folder window drawing with ghost integration
 * Called from EventDispatcher HandleUpdate when window is a folder window
 */
static bool FolderWindow_EnsureIconSystemInitialized(void) {
    static bool sIconInitAttempted = false;
    static bool sIconInitResult = false;

    if (!sIconInitAttempted) {
        sIconInitResult = Icon_Init();
        sIconInitAttempted = true;
    }
    return sIconInitResult;
}

void FolderWindow_Draw(WindowPtr w) {
    static Boolean gInFolderPaint = false;

    if (!w) {
        return;
    }

    /* Re-entrancy guard */
    if (gInFolderPaint) {
        return;
    }
    gInFolderPaint = true;

    /* Erase any ghost outline before drawing */
    GhostEraseIf();

    FolderWindowState* state = GetFolderState(w);
    Boolean isTrash = (w->refCon == 'TRSH');

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)w);

    /* Debug: log portBits bounds at draw time */
    if (w->refCon == 0x4449534b) {
        extern void serial_puts(const char *str);
        extern int sprintf(char* buf, const char* fmt, ...);
        char dbgbuf[256];
        sprintf(dbgbuf, "[FLDRAW] portBits.bounds at draw time: (%d,%d,%d,%d) portRect: (%d,%d,%d,%d)\n",
                w->port.portBits.bounds.left, w->port.portBits.bounds.top,
                w->port.portBits.bounds.right, w->port.portBits.bounds.bottom,
                w->port.portRect.left, w->port.portRect.top,
                w->port.portRect.right, w->port.portRect.bottom);
        serial_puts(dbgbuf);
    }

    /* Fill with white background - pattern is 1-bit bitmap where 0=white, 1=black */
    Pattern whitePat;
    for (int i = 0; i < 8; i++) whitePat.pat[i] = 0x00; /* solid white */
    FillRect(&w->port.portRect, &whitePat);

    /* If trash is empty, draw empty message */
    if (isTrash && state && state->itemCount == 0) {
        MoveTo(10, 30);
        DrawText("Trash is empty", 0, 14);
        MoveTo(10, 50);
        DrawText("Drag items here to delete them", 0, 30);
    }
    /* If we have state, draw icons with selection highlighting */
    else if (state && state->items) {
        bool iconSystemReady = FolderWindow_EnsureIconSystemInitialized();

        for (short i = 0; i < state->itemCount; i++) {
            /* Check selection from array if available, otherwise fall back to selectedIndex */
            Boolean selected = (state->selectedItems && state->selectedItems[i]) || (i == state->selectedIndex);
            IconHandle iconHandle = {0};
            bool resolved = false;

            if (iconSystemReady) {
                FileKind fk = {0};
                fk.type = state->items[i].type;
                fk.creator = state->items[i].creator;
                fk.isFolder = state->items[i].isFolder;
                fk.isVolume = false;
                fk.isTrash = false;
                fk.isTrashFull = false;
                fk.hasCustomIcon = false;
                fk.path = NULL;

                resolved = Icon_ResolveForNode(&fk, &iconHandle);
            }

            if (!resolved || !iconHandle.fam) {
                if (state->items[i].isFolder) {
                    iconHandle.fam = IconSys_DefaultFolder();
                } else {
                    iconHandle.fam = IconSys_DefaultDoc();
                }
            }

            iconHandle.selected = selected;

            int localX = state->items[i].position.h;
            int localY = state->items[i].position.v;

            /* Draw icon with label using window-local coordinates */
            Icon_DrawWithLabel(&iconHandle, state->items[i].name,
                              localX + 16,   /* center X (local) */
                              localY,        /* top Y (local) */
                              selected);
        }
    }

    SetPort(savePort);
    gInFolderPaint = false;
}

/* Check if window is a folder window (by refCon) */
Boolean IsFolderWindow(WindowPtr w) {
    if (!w) return false;
    /* Use lowercase hex to match runtime exactly */
    return (w->refCon == 0x4449534b || w->refCon == 0x54525348);  /* 'DISK' or 'TRSH' */
}

/* Select all items in a folder window */
void FolderWindow_SelectAll(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->selectedItems) return;

    /* Select all items */
    for (short i = 0; i < state->itemCount; i++) {
        state->selectedItems[i] = true;
    }

    /* Update primary selection to first item for keyboard navigation */
    if (state->itemCount > 0) {
        state->selectedIndex = 0;
    }

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);
}

/* Get selected item info from folder window */
Boolean FolderWindow_GetSelectedItem(WindowPtr w, VRefNum* outVref, FileID* outFileID) {
    if (!w || !IsFolderWindow(w) || !outVref || !outFileID) return false;

    FolderWindowState* state = GetFolderState(w);
    if (!state || state->selectedIndex < 0 || state->selectedIndex >= state->itemCount) {
        return false;
    }

    /* Get the selected item */
    FolderItem* item = &state->items[state->selectedIndex];

    *outVref = state->vref;
    *outFileID = item->fileID;

    FINDER_LOG_DEBUG("FolderWindow_GetSelectedItem: vref=%d fileID=%d name=%s\n",
                     (int)*outVref, (int)*outFileID, item->name);

    return true;
}

/* Delete selected items from folder window */
void FolderWindow_DeleteSelected(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return;

    extern bool VFS_Delete(VRefNum vref, FileID id);

    FINDER_LOG_DEBUG("FolderWindow_DeleteSelected: itemCount=%d\n", state->itemCount);

    /* Delete items in reverse order to avoid index shifting issues */
    for (short i = state->itemCount - 1; i >= 0; i--) {
        Boolean shouldDelete = false;

        /* Check if this item is selected */
        if (state->selectedItems) {
            shouldDelete = state->selectedItems[i];
        } else if (i == state->selectedIndex) {
            shouldDelete = true;
        }

        if (shouldDelete) {
            FolderItem* item = &state->items[i];
            FINDER_LOG_DEBUG("FolderWindow_DeleteSelected: Deleting '%s' (fileID=%d)\n",
                           item->name, item->fileID);

            /* Delete from VFS */
            bool deleted = VFS_Delete(state->vref, item->fileID);

            if (deleted) {
                FINDER_LOG_DEBUG("FolderWindow_DeleteSelected: VFS_Delete succeeded\n");

                /* Remove from items array by shifting */
                for (int j = i; j < state->itemCount - 1; j++) {
                    state->items[j] = state->items[j + 1];
                    if (state->selectedItems) {
                        state->selectedItems[j] = state->selectedItems[j + 1];
                    }
                }
                state->itemCount--;

                /* Clear selection for removed item */
                if (state->selectedIndex == i) {
                    state->selectedIndex = -1;
                } else if (state->selectedIndex > i) {
                    state->selectedIndex--;
                }
            } else {
                FINDER_LOG_DEBUG("FolderWindow_DeleteSelected: VFS_Delete failed\n");
            }
        }
    }

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);
}

/*
 * FolderWindow_OpenSelected - Open the currently selected item
 * Extracted from double-click handler for use with Return/Enter keys and File > Open
 */
void FolderWindow_OpenSelected(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return;

    /* Check if there's a valid selection */
    if (state->selectedIndex < 0 || state->selectedIndex >= state->itemCount) {
        FINDER_LOG_DEBUG("FolderWindow_OpenSelected: No valid selection\n");
        return;
    }

    short itemIndex = state->selectedIndex;

    /* Check if this is a folder */
    if (state->items[itemIndex].isFolder) {
        /* Open folder: create new window showing the subfolder's contents */
        extern WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID, ConstStr255Param title);

        /* Convert C string to Pascal string for window title */
        Str255 pTitle;
        size_t len = strlen(state->items[itemIndex].name);
        if (len > 255) len = 255;
        pTitle[0] = (unsigned char)len;
        memcpy(&pTitle[1], state->items[itemIndex].name, len);

        /* Open folder window with the subfolder's directory ID */
        WindowPtr newWin = FolderWindow_OpenFolder(state->vref, state->items[itemIndex].fileID, pTitle);
        if (newWin) {
            FINDER_LOG_DEBUG("FW: opened subfolder window 0x%08x (dirID=%d)\n",
                           (unsigned int)P2UL(newWin),
                           (int)state->items[itemIndex].fileID);
            /* Post update for the new window and old window */
            PostEvent(updateEvt, (UInt32)(uintptr_t)newWin);
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);
        }
    } else {
        /* Document/app: check if it's a text file */
        Boolean isTextFile = false;
        const char* name = state->items[itemIndex].name;

        /* Check file type */
        if (state->items[itemIndex].type == 'TEXT') {
            isTextFile = true;
        }

        /* Check filename patterns for text files */
        if (!isTextFile) {
            int len = strlen(name);
            /* Check for .txt extension */
            if (len >= 4) {
                const char* ext = name + len - 4;
                if (ext[0] == '.' &&
                    (ext[1] == 't' || ext[1] == 'T') &&
                    (ext[2] == 'x' || ext[2] == 'X') &&
                    (ext[3] == 't' || ext[3] == 'T')) {
                    isTextFile = true;
                }
            }
            /* Check for "readme" or "README" prefix */
            if (!isTextFile && len >= 6) {
                const char* prefix = name;
                if ((prefix[0] == 'r' || prefix[0] == 'R') &&
                    (prefix[1] == 'e' || prefix[1] == 'E') &&
                    (prefix[2] == 'a' || prefix[2] == 'A') &&
                    (prefix[3] == 'd' || prefix[3] == 'D') &&
                    (prefix[4] == 'm' || prefix[4] == 'M') &&
                    (prefix[5] == 'e' || prefix[5] == 'E')) {
                    isTextFile = true;
                }
            }
        }

        if (isTextFile) {
            FINDER_LOG_DEBUG("FW: Opening text file \"%s\" with SimpleText\n", name);
            /* Launch SimpleText if not already running */
            extern void SimpleText_Launch(void);
            extern Boolean SimpleText_IsRunning(void);
            extern void SimpleText_OpenFile(const char* path);

            if (!SimpleText_IsRunning()) {
                SimpleText_Launch();
                FINDER_LOG_DEBUG("FW: Launched SimpleText\n");
            }

            /* Build full path to the file */
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "/%s", name);  /* Simple path for now */

            /* Load file content into SimpleText window */
            SimpleText_OpenFile(fullPath);
            FINDER_LOG_DEBUG("FW: Opened file '%s' in SimpleText\n", name);
        } else if (state->items[itemIndex].type == 'APPL') {
            /* Application file */
            if (strcmp(name, "TextEdit") == 0) {
                FINDER_LOG_DEBUG("FW: Launching TextEdit application\n");
                extern void TextEdit_InitApp(void);
                TextEdit_InitApp();
            } else if (strcmp(name, "SimpleText") == 0) {
                FINDER_LOG_DEBUG("FW: Launching SimpleText application\n");
                extern void SimpleText_Launch(void);
                SimpleText_Launch();
            } else if (strcmp(name, "MacPaint") == 0) {
                FINDER_LOG_DEBUG("FW: Launching MacPaint application\n");
                extern void MacPaint_Launch(void);
                MacPaint_Launch();
            } else if (strcmp(name, "Desktop Patterns") == 0) {
                FINDER_LOG_DEBUG("FW: Opening Desktop Patterns control panel\n");
                extern void OpenDesktopCdev(void);
                OpenDesktopCdev();
            } else if (strcmp(name, "Date & Time") == 0) {
                FINDER_LOG_DEBUG("FW: Opening Date & Time control panel\n");
                extern void DateTimePanel_Open(void);
                DateTimePanel_Open();
            } else if (strcmp(name, "Sound") == 0) {
                FINDER_LOG_DEBUG("FW: Opening Sound control panel\n");
                extern void SoundPanel_Open(void);
                SoundPanel_Open();
            } else if (strcmp(name, "Mouse") == 0) {
                FINDER_LOG_DEBUG("FW: Opening Mouse control panel\n");
                extern void MousePanel_Open(void);
                MousePanel_Open();
            } else if (strcmp(name, "Keyboard") == 0) {
                FINDER_LOG_DEBUG("FW: Opening Keyboard control panel\n");
                extern void KeyboardPanel_Open(void);
                KeyboardPanel_Open();
            } else if (strcmp(name, "Control Strip") == 0) {
                FINDER_LOG_DEBUG("FW: Toggling Control Strip palette\n");
                extern void ControlStrip_Toggle(void);
                ControlStrip_Toggle();
            } else {
                FINDER_LOG_DEBUG("FW: OPEN app \"%s\" not implemented\n", name);
            }
        } else {
            FINDER_LOG_DEBUG("FW: OPEN doc \"%s\" not implemented\n", name);
        }
    }

    FINDER_LOG_DEBUG("FolderWindow_OpenSelected: Opened item '%s'\n", state->items[itemIndex].name);
}

/*
 * FolderWindow_DuplicateSelected - Duplicate selected items in current folder
 */
void FolderWindow_DuplicateSelected(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return;

    extern bool VFS_GenerateUniqueName(VRefNum vref, DirID dir, const char* base, char* out);
    extern bool VFS_Copy(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName, FileID* newID);
    extern bool VFS_GetByID(VRefNum vref, FileID id, CatEntry* outEntry);

    FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: itemCount=%d\n", state->itemCount);

    /* Duplicate each selected item */
    for (short i = 0; i < state->itemCount; i++) {
        Boolean shouldDuplicate = false;

        /* Check if this item is selected */
        if (state->selectedItems) {
            shouldDuplicate = state->selectedItems[i];
        } else if (i == state->selectedIndex) {
            shouldDuplicate = true;
        }

        if (shouldDuplicate) {
            FolderItem* item = &state->items[i];
            FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Duplicating '%s' (fileID=%d)\n",
                           item->name, item->fileID);

            /* Generate unique name for the copy */
            char copyName[256];
            char baseName[256];
            snprintf(baseName, sizeof(baseName), "%s copy", item->name);

            if (!VFS_GenerateUniqueName(state->vref, state->currentDir, baseName, copyName)) {
                FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Failed to generate unique name\n");
                continue;
            }

            FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Generated name '%s'\n", copyName);

            /* Copy the file/folder */
            FileID newID = 0;
            bool copied = VFS_Copy(state->vref, state->currentDir, item->fileID,
                                  state->currentDir, copyName, &newID);

            if (copied && newID != 0) {
                FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: VFS_Copy succeeded, newID=%d\n", newID);

                /* Get catalog entry for the new item */
                CatEntry newEntry;
                if (VFS_GetByID(state->vref, newID, &newEntry)) {
                    /* Add to items array - reallocate if needed */
                    /* Check for integer overflow: itemCount + 1 and sizeof multiplication */
                    if (state->itemCount >= INT16_MAX ||
                        (size_t)state->itemCount > SIZE_MAX / sizeof(FolderItem) - 1) {
                        FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Integer overflow in items count\n");
                        continue;
                    }
                    Size oldItemsSize = state->itemCount * sizeof(FolderItem);
                    FolderItem* newItems = (FolderItem*)NewPtr(sizeof(FolderItem) * (state->itemCount + 1));
                    if (!newItems) {
                        FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Failed to reallocate items array\n");
                        continue;
                    }

                    if (state->items) {
                        BlockMove(state->items, newItems, oldItemsSize);
                        DisposePtr((Ptr)state->items);
                    }
                    state->items = newItems;

                    /* Reallocate selectedItems array too */
                    if (state->selectedItems) {
                        /* Check for integer overflow: itemCount + 1 and sizeof multiplication */
                        if (state->itemCount >= INT16_MAX ||
                            (size_t)state->itemCount > SIZE_MAX / sizeof(Boolean) - 1) {
                            FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Integer overflow in selectedItems count\n");
                            continue;
                        }
                        Size oldSelectedSize = state->itemCount * sizeof(Boolean);
                        Boolean* newSelected = (Boolean*)NewPtr(sizeof(Boolean) * (state->itemCount + 1));
                        if (!newSelected) {
                            FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Failed to reallocate selectedItems array\n");
                            continue;
                        }
                        BlockMove(state->selectedItems, newSelected, oldSelectedSize);
                        DisposePtr((Ptr)state->selectedItems);
                        state->selectedItems = newSelected;
                        state->selectedItems[state->itemCount] = false;
                    }

                    /* Add the new item */
                    FolderItem* newItem = &state->items[state->itemCount];
                    strncpy(newItem->name, newEntry.name, sizeof(newItem->name) - 1);
                    newItem->name[sizeof(newItem->name) - 1] = '\0';
                    newItem->fileID = newID;
                    newItem->isFolder = (newEntry.kind == kNodeDir);
                    newItem->type = newEntry.type;
                    newItem->creator = newEntry.creator;

                    state->itemCount++;

                    FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Added new item to window, count=%d\n",
                                   state->itemCount);
                } else {
                    FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: Failed to get catalog entry for new item\n");
                }
            } else {
                FINDER_LOG_DEBUG("FolderWindow_DuplicateSelected: VFS_Copy failed\n");
            }
        }
    }

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);
}

/*
 * FolderWindow_GetSelectedAsSpecs - Get selected items as FSSpec array
 * Returns count of selected items, fills specs array (caller must free)
 */
short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs) {
    if (!w || !IsFolderWindow(w) || !outSpecs) return 0;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return 0;

    /* Count selected items */
    short selectedCount = 0;
    for (short i = 0; i < state->itemCount; i++) {
        Boolean isSelected = false;
        if (state->selectedItems) {
            isSelected = state->selectedItems[i];
        } else if (i == state->selectedIndex) {
            isSelected = true;
        }
        if (isSelected) {
            selectedCount++;
        }
    }

    if (selectedCount == 0) {
        *outSpecs = NULL;
        return 0;
    }

    /* Allocate FSSpec array */
    FSSpec* specs = NewPtr(sizeof(FSSpec) * selectedCount);
    if (!specs) {
        FINDER_LOG_DEBUG("FolderWindow_GetSelectedAsSpecs: malloc failed\n");
        *outSpecs = NULL;
        return 0;
    }

    /* Fill FSSpec array */
    short specIndex = 0;
    for (short i = 0; i < state->itemCount; i++) {
        Boolean isSelected = false;
        if (state->selectedItems) {
            isSelected = state->selectedItems[i];
        } else if (i == state->selectedIndex) {
            isSelected = true;
        }

        if (isSelected) {
            FolderItem* item = &state->items[i];

            /* Build FSSpec */
            specs[specIndex].vRefNum = state->vref;
            specs[specIndex].parID = state->currentDir;

            /* Convert name to Pascal string */
            size_t len = strlen(item->name);
            if (len > 255) len = 255;
            specs[specIndex].name[0] = (unsigned char)len;
            memcpy(&specs[specIndex].name[1], item->name, len);

            FINDER_LOG_DEBUG("FolderWindow_GetSelectedAsSpecs: [%d] name='%s' vref=%d parID=%d\n",
                           specIndex, item->name, state->vref, state->currentDir);

            specIndex++;
        }
    }

    *outSpecs = specs;
    return selectedCount;
}

/*
 * FolderWindow_GetVRef - Get volume reference for folder window
 */
VRefNum FolderWindow_GetVRef(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return 0;

    FolderWindowState* state = GetFolderState(w);
    if (!state) return 0;

    return state->vref;
}

/*
 * FolderWindow_GetCurrentDir - Get current directory ID for folder window
 */
DirID FolderWindow_GetCurrentDir(WindowPtr w) {
    if (!w || !IsFolderWindow(w)) return 0;

    FolderWindowState* state = GetFolderState(w);
    if (!state) return 0;

    return state->currentDir;
}

/*
 * Sort items by name (folders first, then alphabetically)
 */
static void SortFolderItemsByName(FolderItem* items, short count) {
    /* Simple bubble sort - adequate for typical folder sizes */
    for (short i = 0; i < count - 1; i++) {
        for (short j = 0; j < count - i - 1; j++) {
            Boolean shouldSwap = false;

            /* Folders always come first */
            if (!items[j].isFolder && items[j+1].isFolder) {
                shouldSwap = true;
            } else if (items[j].isFolder == items[j+1].isFolder) {
                /* Both are folders or both are files - compare names */
                if (strcmp(items[j].name, items[j+1].name) > 0) {
                    shouldSwap = true;
                }
            }

            if (shouldSwap) {
                FolderItem temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }
}

/*
 * Sort items by kind/type (folders first, then by file type)
 */
static void SortFolderItemsByKind(FolderItem* items, short count) {
    /* Simple bubble sort */
    for (short i = 0; i < count - 1; i++) {
        for (short j = 0; j < count - i - 1; j++) {
            Boolean shouldSwap = false;

            /* Folders always come first */
            if (!items[j].isFolder && items[j+1].isFolder) {
                shouldSwap = true;
            } else if (items[j].isFolder == items[j+1].isFolder) {
                /* Both are folders or both are files */
                if (items[j].type > items[j+1].type) {
                    shouldSwap = true;
                } else if (items[j].type == items[j+1].type) {
                    /* Same type - sort by name */
                    if (strcmp(items[j].name, items[j+1].name) > 0) {
                        shouldSwap = true;
                    }
                }
            }

            if (shouldSwap) {
                FolderItem temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }
}

/*
 * Sort items by size (folders first, then by size descending, then by name)
 */
static void SortFolderItemsBySize(FolderItem* items, short count) {
    /* Simple bubble sort */
    for (short i = 0; i < count - 1; i++) {
        for (short j = 0; j < count - i - 1; j++) {
            Boolean shouldSwap = false;

            /* Folders always come first */
            if (!items[j].isFolder && items[j+1].isFolder) {
                shouldSwap = true;
            } else if (items[j].isFolder == items[j+1].isFolder) {
                /* Both are folders or both are files */
                /* Sort by size descending (larger files first) */
                if (items[j].size < items[j+1].size) {
                    shouldSwap = true;
                } else if (items[j].size == items[j+1].size) {
                    /* Same size - sort by name */
                    if (strcmp(items[j].name, items[j+1].name) > 0) {
                        shouldSwap = true;
                    }
                }
            }

            if (shouldSwap) {
                FolderItem temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }
}

/*
 * Sort items by date (folders first, then by modification date descending, then by name)
 */
static void SortFolderItemsByDate(FolderItem* items, short count) {
    /* Simple bubble sort */
    for (short i = 0; i < count - 1; i++) {
        for (short j = 0; j < count - i - 1; j++) {
            Boolean shouldSwap = false;

            /* Folders always come first */
            if (!items[j].isFolder && items[j+1].isFolder) {
                shouldSwap = true;
            } else if (items[j].isFolder == items[j+1].isFolder) {
                /* Both are folders or both are files */
                /* Sort by modification date descending (most recent first) */
                if (items[j].modTime < items[j+1].modTime) {
                    shouldSwap = true;
                } else if (items[j].modTime == items[j+1].modTime) {
                    /* Same date - sort by name */
                    if (strcmp(items[j].name, items[j+1].name) > 0) {
                        shouldSwap = true;
                    }
                }
            }

            if (shouldSwap) {
                FolderItem temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }
}

/*
 * Sort items by label (folders first, then by label index, then by name)
 */
static void SortFolderItemsByLabel(FolderItem* items, short count) {
    /* Simple bubble sort */
    for (short i = 0; i < count - 1; i++) {
        for (short j = 0; j < count - i - 1; j++) {
            Boolean shouldSwap = false;

            /* Folders always come first */
            if (!items[j].isFolder && items[j+1].isFolder) {
                shouldSwap = true;
            } else if (items[j].isFolder == items[j+1].isFolder) {
                /* Both are folders or both are files */
                /* Sort by label index (higher label numbers first for visual grouping) */
                if (items[j].label < items[j+1].label) {
                    shouldSwap = true;
                } else if (items[j].label == items[j+1].label) {
                    /* Same label - sort by name */
                    if (strcmp(items[j].name, items[j+1].name) > 0) {
                        shouldSwap = true;
                    }
                }
            }

            if (shouldSwap) {
                FolderItem temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }
}

/*
 * FolderWindow_SortAndArrange - Sort items and arrange in grid
 * sortType: 0=none, 1=by Icon, 2=by Name, 3=by Size, 4=by Kind, 5=by Label, 6=by Date
 */
void FolderWindow_SortAndArrange(WindowPtr w, short sortType) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items || state->itemCount == 0) return;

    FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: sortType=%d, itemCount=%d\n",
                     sortType, state->itemCount);

    /* Sort the items array */
    switch (sortType) {
        case 2:  /* by Name */
            SortFolderItemsByName(state->items, state->itemCount);
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: Sorted by name\n");
            break;

        case 3:  /* by Size */
            SortFolderItemsBySize(state->items, state->itemCount);
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: Sorted by size\n");
            break;

        case 4:  /* by Kind */
            SortFolderItemsByKind(state->items, state->itemCount);
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: Sorted by kind\n");
            break;

        case 5:  /* by Label */
            SortFolderItemsByLabel(state->items, state->itemCount);
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: Sorted by label\n");
            break;

        case 6:  /* by Date */
            SortFolderItemsByDate(state->items, state->itemCount);
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: Sorted by date\n");
            break;

        case 1:  /* by Icon - just arrange, don't sort */
        default:
            FINDER_LOG_DEBUG("FolderWindow_SortAndArrange: No sorting, just arranging\n");
            break;
    }

    /* Arrange all items in grid after sorting */
    FolderWindow_CleanUp(w, false);  /* false = arrange all items */
}

/*
 * FolderWindow_SetLabelOnSelected - Apply label to selected items
 */
void FolderWindow_SetLabelOnSelected(WindowPtr w, short labelIndex) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return;

    FINDER_LOG_DEBUG("FolderWindow_SetLabelOnSelected: labelIndex=%d, itemCount=%d\n",
                     labelIndex, state->itemCount);

    /* Apply label to all selected items */
    int labeledCount = 0;
    for (short i = 0; i < state->itemCount; i++) {
        Boolean isSelected = false;

        /* Check if this item is selected */
        if (state->selectedItems && state->selectedItems[i]) {
            isSelected = true;
        } else if (i == state->selectedIndex) {
            isSelected = true;
        }

        if (isSelected) {
            state->items[i].label = labelIndex;
            labeledCount++;
            FINDER_LOG_DEBUG("FolderWindow_SetLabelOnSelected: Set label %d on item %d '%s'\n",
                           labelIndex, i, state->items[i].name);
        }
    }

    FINDER_LOG_DEBUG("FolderWindow_SetLabelOnSelected: Labeled %d items\n", labeledCount);

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);
}

/*
 * FolderWindow_CleanUp - Arrange icons in a grid pattern
 */
void FolderWindow_CleanUp(WindowPtr w, Boolean selectedOnly) {
    if (!w || !IsFolderWindow(w)) return;

    FolderWindowState* state = GetFolderState(w);
    if (!state || !state->items) return;

    /* Grid parameters - matching classic Mac OS */
    const short ICON_WIDTH = 80;   /* Width of icon + label */
    const short ICON_HEIGHT = 64;  /* Height of icon + label */
    const short LEFT_MARGIN = 20;
    const short TOP_MARGIN = 40;   /* Below title bar */
    const short SPACING_H = 10;    /* Horizontal spacing between icons */
    const short SPACING_V = 10;    /* Vertical spacing between icons */

    /* Calculate grid dimensions */
    short gridX = LEFT_MARGIN;
    short gridY = TOP_MARGIN;
    short col = 0;
    short maxCols = 5;  /* Maximum icons per row */

    FINDER_LOG_DEBUG("FolderWindow_CleanUp: Arranging %d items, selectedOnly=%d\n",
                     state->itemCount, selectedOnly);

    for (short i = 0; i < state->itemCount; i++) {
        /* Check if we should arrange this item */
        Boolean shouldArrange = true;
        if (selectedOnly) {
            shouldArrange = false;
            if (state->selectedItems && state->selectedItems[i]) {
                shouldArrange = true;
            } else if (i == state->selectedIndex) {
                shouldArrange = true;
            }
        }

        if (shouldArrange) {
            /* Set grid position */
            state->items[i].position.h = gridX;
            state->items[i].position.v = gridY;

            FINDER_LOG_DEBUG("FolderWindow_CleanUp: Item %d '%s' -> (%d, %d)\n",
                           i, state->items[i].name, gridX, gridY);

            /* Advance to next grid position */
            col++;
            if (col >= maxCols) {
                /* Move to next row */
                col = 0;
                gridX = LEFT_MARGIN;
                gridY += ICON_HEIGHT + SPACING_V;
            } else {
                /* Move to next column */
                gridX += ICON_WIDTH + SPACING_H;
            }
        }
    }

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);

    FINDER_LOG_DEBUG("FolderWindow_CleanUp: Complete\n");
}

/* Update window proc for folder windows */
void FolderWindowProc(WindowPtr window, short message, long param)
{
    switch (message) {
        case 0:  /* wDraw = 0, draw content only */
            /* Draw window contents - NO CHROME! */
            FINDER_LOG_DEBUG("Finder: FolderWindowProc drawing content\n");
            FolderWindow_Draw(window);
            break;

        case 1:  /* wHit = 1, handle click in content */
            /* Handle click in content */
            FINDER_LOG_DEBUG("Click in folder window at (%d,%d)\n",
                         (short)(param >> 16), (short)(param & 0xFFFF));
            break;

        default:
            break;
    }
}

/*
 * GhostEraseIf stub - desktop_manager.c has the real implementation
 * This is needed because FolderWindow_Draw calls it to be safe
 */
static void GhostEraseIf(void) {
    /* Call the real ghost eraser from desktop_manager.c */
    extern void Desktop_GhostEraseIf(void);
    Desktop_GhostEraseIf();
}

/*
 * CleanupFolderWindow - Clean up state when a folder window is closed
 * This prevents crashes when the window is closed and then File > Close is used
 */

/* Temporarily disable ALL FINDER logging to prevent heap corruption from variadic serial_logf */
#undef FINDER_LOG_DEBUG
#undef FINDER_LOG_WARN
#undef FINDER_LOG_ERROR
#define FINDER_LOG_DEBUG(...) do {} while(0)
#define FINDER_LOG_WARN(...) do {} while(0)
#define FINDER_LOG_ERROR(...) do {} while(0)

void CleanupFolderWindow(WindowPtr w) {
    if (!w) return;

FINDER_LOG_DEBUG("CleanupFolderWindow: cleaning up window 0x%08x\n", (unsigned int)P2UL(w));

    /* Debug: Show all window slots before search */
    for (int j = 0; j < MAX_FOLDER_WINDOWS; j++) {
        if (gFolderWindows[j].window != NULL) {
            FINDER_LOG_DEBUG("CleanupFolderWindow: slot[%d] = 0x%08x\n", j,
                          (unsigned int)P2UL(gFolderWindows[j].window));
        }
    }

    /* Find and clear this window's state */
    for (int i = 0; i < MAX_FOLDER_WINDOWS; i++) {
        if (gFolderWindows[i].window == w) {
            FINDER_LOG_DEBUG("CleanupFolderWindow: found slot %d, freeing items\n", i);
            FINDER_LOG_DEBUG("CleanupFolderWindow: items ptr=0x%08x, count=%d\n",
                          (unsigned int)gFolderWindows[i].state.items,
                          gFolderWindows[i].state.itemCount);

            /* Sanity check before free - itemCount should be reasonable */
            if (gFolderWindows[i].state.itemCount > 1000) {
                FINDER_LOG_WARN("CleanupFolderWindow: CORRUPTED itemCount=%d, skipping free\n",
                             gFolderWindows[i].state.itemCount);
            } else if (gFolderWindows[i].state.items) {
                /* Free the items array if allocated */
                FINDER_LOG_DEBUG("CleanupFolderWindow: calling DisposePtr((Ptr))\n");
                DisposePtr((Ptr)gFolderWindows[i].state.items);
                FINDER_LOG_DEBUG("CleanupFolderWindow: DisposePtr((Ptr)) returned\n");
            }

            /* Free the selection array if allocated */
            if (gFolderWindows[i].state.selectedItems) {
                FINDER_LOG_DEBUG("CleanupFolderWindow: freeing selectedItems\n");
                DisposePtr((Ptr)gFolderWindows[i].state.selectedItems);
            }

            /* Clear the slot */
            FINDER_LOG_DEBUG("CleanupFolderWindow: clearing window pointer\n");
            gFolderWindows[i].window = NULL;
            FINDER_LOG_DEBUG("CleanupFolderWindow: window pointer now=0x%08x\n",
                          (unsigned int)P2UL(gFolderWindows[i].window));
            FINDER_LOG_DEBUG("CleanupFolderWindow: clearing items pointer\n");
            gFolderWindows[i].state.items = NULL;
            FINDER_LOG_DEBUG("CleanupFolderWindow: clearing itemCount\n");
            gFolderWindows[i].state.itemCount = 0;
            FINDER_LOG_DEBUG("CleanupFolderWindow: clearing selectedIndex\n");
            gFolderWindows[i].state.selectedIndex = -1;

            FINDER_LOG_DEBUG("CleanupFolderWindow: slot %d cleared\n", i);
            FINDER_LOG_DEBUG("CleanupFolderWindow: about to return\n");
            FINDER_LOG_DEBUG("CleanupFolderWindow: RETURN NOW\n");
            return;
        }
    }

    /* Window not found - this is OK if it was already closed, but warn about potential double-close */
    FINDER_LOG_WARN("CleanupFolderWindow: window 0x%08x not found in state table (already cleaned up or never registered)\n", (unsigned int)P2UL(w));
}
