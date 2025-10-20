#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Desktop Manager Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
 *
 * Evidence sources:
 * - String analysis: "Clean Up Desktop", "Rebuilding the desktop file"
 * - String analysis: "was found on the desktop", "on the desktop"
 * - Desktop database management functionality
 *
 * This module handles desktop icon positioning and desktop database management.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include <string.h>
#include <stdlib.h>  /* For abs() */

#include "Finder/finder.h"
#include "Finder/finder_types.h"
#include "FileMgr/file_manager.h"
/* Use local headers instead of system headers */
#include "MemoryMgr/memory_manager_types.h"
#include "ResourceManager.h"
#include "PatternMgr/pattern_manager.h"
#include "FS/vfs.h"
#include "chicago_font.h"  /* For ChicagoCharInfo */
#include "Finder/Icon/icon_types.h"
#include "Finder/Icon/icon_label.h"
#include "Finder/Icon/icon_system.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/DisplayBezel.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"
#include "Finder/FinderLogging.h"
#include "EventManager/EventManager.h"
#include "WindowManager/WindowManager.h"

/* HD Icon data - still needed by icon_system.c */
extern const uint8_t g_HDIcon[128];
extern const uint8_t g_HDIconMask[128];

/* Debug output */
extern void serial_puts(const char* str);

/* External function declarations */
extern int sprintf(char* str, const char* format, ...);
extern int snprintf(char* str, size_t size, const char* format, ...);
extern bool Trash_IsEmptyAll(void);
/* NewPtr now provided by MemoryManager.h */
extern void InvalRect(const Rect* badRect);
extern void SetDeskHook(void (*hookProc)(RgnHandle));

/* Chicago font data */
extern const uint8_t chicago_bitmap[];
extern GrafPtr g_currentPort;
/* CHICAGO_HEIGHT is defined in chicago_font.h */
#define CHICAGO_ASCENT 12
#define CHICAGO_ROW_BYTES 140


/* Desktop Database Constants */
#define kDesktopDatabaseName    "\012Desktop DB"
#define kDesktopIconSpacing     80          /* Pixel spacing between icons */
#define kDesktopMargin          20          /* Margin from screen edge */
#define kMaxDesktopIcons        256         /* Maximum icons on desktop */

/* Grid and icon sizing (System 7-ish) */
enum { kGridW = 8, kGridH = 12, kIconW = 32, kIconH = 32 };

/* Drag threshold for distinguishing clicks from drags */
#define kDragThreshold 4

/* External globals */
extern QDGlobals qd;  /* QuickDraw globals from main.c */
extern void* framebuffer;
extern uint32_t fb_width, fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* Global tracking guard for modal drag loops */
volatile Boolean gInMouseTracking = false;

/* Index of icon being dragged (-1 if none) */
static short gDraggingIconIndex = -1;

/* Global Desktop State */
static DesktopItem *gDesktopIcons = nil;   /* Array of desktop items (universal system) */
static short gDesktopIconCount = 0;         /* Number of icons on desktop */
static Boolean gDesktopNeedsCleanup = false; /* Desktop needs reorganization */
static VRefNum gBootVolumeRef = 0;          /* Boot volume reference */
static Boolean gVolumeIconVisible = false;   /* Is volume icon shown on desktop */
static DesktopItem gDesktopIconStatic[kMaxDesktopIcons]; /* Static fallback storage */
static Boolean gDesktopIconStaticInUse = false;

/* Icon selection and dragging state */
static short gSelectedIcon = -1;             /* Index of selected icon (-1 = none) */
static Boolean gDraggingIcon = false;        /* Currently dragging an icon */
static Point gDragOffset = {0, 0};           /* Offset from icon origin to mouse */
/* Unused - reserved for future drag/click tracking:
static Point gOriginalPos = {0, 0};
static UInt32 gLastClickTime = 0;
static Point gLastClickPos = {0, 0};
*/

/* Same-icon double-click tracking (Classic Finder behavior) */
static short sLastClickIcon = -1;            /* Icon clicked last time */
static UInt32 sLastClickTicks = 0;           /* When it was clicked */

/* Forward Declarations */
static OSErr LoadDesktopDatabase(short vRefNum);
static OSErr SaveDesktopDatabase(short vRefNum);
static OSErr AllocateDesktopIcons(void);
/* Unused functions in #if 0 block:
static Point CalculateNextIconPosition(void);
static Boolean IsPositionOccupied(Point position);
*/
static OSErr ScanDirectoryForDesktopEntries(short vRefNum, long dirID, short databaseRefNum);
static Point SnapToGrid(Point p);
static void UpdateIconRect(short iconIndex, Rect *outRect);
static short IconAtPoint(Point where);
static void GhostXOR(const Rect* r);
static void GhostEraseIf(void);
static void GhostShowAt(const Rect* r);
static void DesktopYield(void);
static void TrackIconDragSync(short iconIndex, Point startPt);

/* Helper forward declarations */
static WindowPtr Finder_GetFrontVisibleWindow(void);
static Boolean Finder_GetWindowBounds(WindowPtr window, Rect* outBounds);
static void Finder_EraseRegionExcludingRect(RgnHandle baseRgn, const Rect* excludeRect);

static WindowPtr Finder_GetFrontVisibleWindow(void)
{
    WindowPtr front = FrontWindow();
    while (front && !front->visible) {
        front = front->nextWindow;
    }
    return front;
}

static Boolean Finder_GetWindowBounds(WindowPtr window, Rect* outBounds)
{
    if (!window || !outBounds) {
        return false;
    }

    if (window->visRgn && *window->visRgn) {
        *outBounds = (*window->visRgn)->rgnBBox;
    } else if (window->strucRgn && *window->strucRgn) {
        *outBounds = (*window->strucRgn)->rgnBBox;
    } else {
        *outBounds = window->port.portBits.bounds;
    }

    return (outBounds->left < outBounds->right) && (outBounds->top < outBounds->bottom);
}

static void Finder_EraseRectSection(const Rect* rect)
{
    if (!rect || rect->left >= rect->right || rect->top >= rect->bottom) {
        return;
    }

    RgnHandle temp = NewRgn();
    if (temp) {
        RectRgn(temp, rect);
        EraseRgn(temp);
        DisposeRgn(temp);
    } else {
        EraseRect(rect);
    }
}

static void Finder_EraseRegionExcludingRect(RgnHandle baseRgn, const Rect* excludeRect)
{
    if (!baseRgn || !*baseRgn) {
        return;
    }

    if (!excludeRect) {
        EraseRgn(baseRgn);
        return;
    }

    Rect baseBounds = (*baseRgn)->rgnBBox;
    Rect overlap;
    if (!SectRect(&baseBounds, excludeRect, &overlap)) {
        EraseRgn(baseRgn);
        return;
    }

    /* Top strip */
    if (baseBounds.top < overlap.top) {
        Rect topRect = baseBounds;
        topRect.bottom = overlap.top;
        Finder_EraseRectSection(&topRect);
    }

    /* Bottom strip */
    if (overlap.bottom < baseBounds.bottom) {
        Rect bottomRect = baseBounds;
        bottomRect.top = overlap.bottom;
        Finder_EraseRectSection(&bottomRect);
    }

    /* Middle strips (left/right) */
    Rect middleRect = baseBounds;
    middleRect.top = (baseBounds.top > overlap.top) ? baseBounds.top : overlap.top;
    middleRect.bottom = (baseBounds.bottom < overlap.bottom) ? baseBounds.bottom : overlap.bottom;
    if (middleRect.top < middleRect.bottom) {
        if (baseBounds.left < overlap.left) {
            Rect leftRect = middleRect;
            leftRect.right = overlap.left;
            Finder_EraseRectSection(&leftRect);
        }
        if (overlap.right < baseBounds.right) {
            Rect rightRect = middleRect;
            rightRect.left = overlap.right;
            Finder_EraseRectSection(&rightRect);
        }
    }
}

static bool EnsureIconSystemInitialized(void)
{
    static bool sIconInitAttempted = false;
    static bool sIconInitResult = false;

    if (!sIconInitAttempted) {
        sIconInitResult = Icon_Init();
        sIconInitAttempted = true;
    }
    return sIconInitResult;
}

static void Desktop_BuildFileKind(const DesktopItem* item, FileKind* outKind)
{
    if (!item || !outKind) {
        return;
    }

    FileKind fk = {0};
    fk.path = NULL;
    fk.hasCustomIcon = false;

    switch (item->type) {
        case kDesktopItemTrash:
            fk.isTrash = true;
            fk.isTrashFull = !Trash_IsEmptyAll();
            fk.isFolder = true;
            break;
        case kDesktopItemVolume:
            fk.isVolume = true;
            break;
        case kDesktopItemFolder:
            fk.isFolder = true;
            break;
        case kDesktopItemApplication:
            fk.type = item->data.file.fileType ? item->data.file.fileType : 'APPL';
            fk.creator = item->data.file.creator;
            break;
        case kDesktopItemFile:
            fk.type = item->data.file.fileType;
            fk.creator = item->data.file.creator;
            break;
        case kDesktopItemAlias:
            fk.type = 'alis';
            break;
        default:
            break;
    }

    *outKind = fk;
}

static int Desktop_LabelOffsetForItem(const DesktopItem* item)
{
    if (!item) {
        return kIconH;
    }

    switch (item->type) {
        case kDesktopItemTrash:
            return 48;
        case kDesktopItemVolume:
            return 34;
        default:
            return kIconH;
    }
}

static void Desktop_DrawIconsCommon(RgnHandle clip)
{
    if (!EnsureIconSystemInitialized()) {
        return;
    }

    (void)clip;  /* Clipping handled by QuickDraw port */

    for (int i = 0; i < gDesktopIconCount; i++) {
        if (i == gDraggingIconIndex) {
            continue;
        }

        if (gDesktopIcons[i].type == kDesktopItemVolume && !gVolumeIconVisible) {
            continue;
        }

        FileKind fk = {0};
        Desktop_BuildFileKind(&gDesktopIcons[i], &fk);

        IconHandle handle = {0};
        bool resolved = Icon_ResolveForNode(&fk, &handle);

        if (!resolved || !handle.fam) {
            switch (gDesktopIcons[i].type) {
                case kDesktopItemFolder:
                    handle.fam = IconSys_DefaultFolder();
                    break;
                case kDesktopItemVolume:
                    handle.fam = IconSys_DefaultVolume();
                    break;
                case kDesktopItemTrash:
                    handle.fam = fk.isTrashFull ? IconSys_TrashFull() : IconSys_TrashEmpty();
                    break;
                default:
                    handle.fam = IconSys_DefaultDoc();
                    break;
            }
        }

        bool selected = (gSelectedIcon == i);
        handle.selected = selected;

        /* Desktop icons use global screen coordinates - no conversion needed
         * gDesktopIcons[i].position is already in global screen coordinates */
        Point screenPos = gDesktopIcons[i].position;

        int centerX = screenPos.h + (kIconW / 2);
        int topY = screenPos.v;
        int labelOffset = Desktop_LabelOffsetForItem(&gDesktopIcons[i]);

        /* Debug icon names */
        extern void serial_puts(const char* str);
        static int icon_debug = 0;
        static char icdbg[256];
        if (icon_debug < 5) {
            int namelen = gDesktopIcons[i].name[0];
            sprintf(icdbg, "[ICON] Drawing icon %d: namelen=%d pos=(%d,%d) type=%d\n",
                   i, namelen, screenPos.h, screenPos.v, gDesktopIcons[i].type);
            serial_puts(icdbg);
            if (namelen > 0 && namelen < 64) {
                sprintf(icdbg, "[ICON]   name='%.*s'\n", namelen, &gDesktopIcons[i].name[1]);
                serial_puts(icdbg);
            }
            icon_debug++;
        }

        Icon_DrawWithLabelOffset(&handle,
                                 gDesktopIcons[i].name,
                                 centerX,
                                 topY,
                                 labelOffset,
                                 selected);

#ifdef DESKTOP_DEBUG_OUTLINES
        Rect outline = {
            .top = screenPos.v,
            .left = screenPos.h,
            .bottom = screenPos.v + kIconH,
            .right = screenPos.h + kIconW
        };
        FrameRect(&outline);
#endif
    }
}

/* Public function to draw the desktop */
void DrawDesktop(void);
void DrawVolumeIcon(void);

/*
 * CleanUpDesktop - Arranges all desktop icons in a grid pattern

 */
OSErr CleanUpDesktop(void)
{
    OSErr err = noErr;
    Point currentPos;
    short iconIndex;
    short iconsPerRow;
    short currentRow = 0;
    short currentCol = 0;

    /* Calculate icons per row based on screen width */
    iconsPerRow = (qd.screenBits.bounds.right - (kDesktopMargin * 2)) / kDesktopIconSpacing;

    /* Starting position */
    currentPos.h = kDesktopMargin;
    currentPos.v = kDesktopMargin + 40; /* Leave space for menu bar */

    /* Arrange all desktop icons in grid */
    for (iconIndex = 0; iconIndex < gDesktopIconCount; iconIndex++) {
        /* Calculate grid position */
        currentPos.h = kDesktopMargin + (currentCol * kDesktopIconSpacing);
        currentPos.v = kDesktopMargin + 40 + (currentRow * kDesktopIconSpacing);

        /* Update icon position */
        gDesktopIcons[iconIndex].position = currentPos;

        /* Move to next grid position */
        currentCol++;
        if (currentCol >= iconsPerRow) {
            currentCol = 0;
            currentRow++;
        }
    }

    /* Save updated positions to desktop database */
    err = SaveDesktopDatabase(0); /* System volume */
    if (err != noErr) {
        return err;
    }

    /* Redraw desktop */
    InvalRect(&qd.screenBits.bounds);
    DrawDesktop();  /* Draw the desktop pattern */
    gDesktopNeedsCleanup = false;

    return noErr;
}

/*
 * Finder_DeskHook - Called by Window Manager to let Finder draw desktop
 * This is called during PaintBehind when the desktop needs updating
 */
static void Finder_DeskHook(RgnHandle invalidRgn)
{
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(qd.thePort);  /* Draw to screen port - FIX: qd.thePort is already a GrafPtr */

    /* Clip to the invalid region */
    RgnHandle desktopClip = NewRgn();
    if (desktopClip) {
        Rect desktopRect = qd.screenBits.bounds;
        desktopRect.top = 20;  /* Exclude menu bar */
        RectRgn(desktopClip, &desktopRect);
        if (invalidRgn) {
            SectRgn(invalidRgn, desktopClip, desktopClip);
        }
        SetClip(desktopClip);
    } else if (invalidRgn) {
        SetClip(invalidRgn);
    }

    /* Draw desktop pattern using current background pattern */
    /* Build a paint region that excludes the frontmost window so we don't wipe its content */
    RgnHandle paintRgn = NewRgn();
    if (paintRgn) {
        if (invalidRgn) {
            CopyRgn(invalidRgn, paintRgn);
        } else if (desktopClip) {
            CopyRgn(desktopClip, paintRgn);
        } else {
            Rect screenRect = qd.screenBits.bounds;
            RectRgn(paintRgn, &screenRect);
        }

        if (!EmptyRgn(paintRgn)) {
            Rect excludeBounds;
            WindowPtr front = Finder_GetFrontVisibleWindow();
            if (Finder_GetWindowBounds(front, &excludeBounds)) {
                Finder_EraseRegionExcludingRect(paintRgn, &excludeBounds);
            } else {
                EraseRgn(paintRgn);
            }
        }
        DisposeRgn(paintRgn);
    } else if (invalidRgn) {
        EraseRgn(invalidRgn);
    } else if (desktopClip) {
        EraseRgn(desktopClip);
    }

    QD_DrawCRTBezel();

    /* Draw desktop icons in invalid region */
    RgnHandle paintClip = invalidRgn ? invalidRgn : desktopClip;

    FINDER_LOG_DEBUG("DeskHook: drawing %d desktop icons\n", gDesktopIconCount);
    Desktop_DrawIconsCommon(paintClip);

    SetPort(savePort);
    if (desktopClip) {
        DisposeRgn(desktopClip);
    }
}

/* ArrangeDesktopIcons - Arrange desktop icons in a grid */
void ArrangeDesktopIcons(void)
{
    const int gridSpacing = 80;  /* Spacing between icons */
    const int startX = 700;       /* Start from right side */
    const int startY = 50;        /* Start below menu bar */
    int currentX = startX;
    int currentY = startY;

    FINDER_LOG_DEBUG("ArrangeDesktopIcons: Arranging %d icons\n", gDesktopIconCount);

    /* Arrange icons in a vertical column from top-right */
    for (int i = 0; i < gDesktopIconCount; i++) {
        gDesktopIcons[i].position.h = currentX;
        gDesktopIcons[i].position.v = currentY;

        currentY += gridSpacing;

        /* Wrap to next column if too low */
        if (currentY > 400) {
            currentY = startY;
            currentX -= gridSpacing;
        }
    }

    /* Redraw desktop to show new arrangement */
    DrawDesktop();
}

/*
 * DrawDesktop - Initial desktop drawing (called once at startup)
 * Also used to redraw desktop after menus or dialogs
 */
void DrawDesktop(void)
{
    static Boolean gInDesktopPaint = false;
    RgnHandle desktopRgn;

    /* Erase any active ghost outline before desktop redraw */
    GhostEraseIf();

    /* Re-entrancy guard: prevent recursive painting */
    if (gInDesktopPaint) {
        FINDER_LOG_DEBUG("DrawDesktop: re-entry detected, skipping to avoid freeze\n");
        return;
    }
    gInDesktopPaint = true;

    /* Register our DeskHook with Window Manager */
    SetDeskHook(Finder_DeskHook);

    /* Create a region for the desktop (excluding menu bar) */
    desktopRgn = NewRgn();
    if (!desktopRgn) {
        FINDER_LOG_DEBUG("DrawDesktop: NewRgn failed (out of memory)\n");
        gInDesktopPaint = false;
        return;
    }
    Rect desktopRect = qd.screenBits.bounds;
    desktopRect.top = 20;  /* Start below menu bar */
    RectRgn(desktopRgn, &desktopRect);

    WindowPtr frontWindow = FrontWindow();
    if (frontWindow && frontWindow->visRgn) {
        RgnHandle frontVisible = NewRgn();
        if (frontVisible) {
            CopyRgn(frontWindow->visRgn, frontVisible);
            DiffRgn(desktopRgn, frontVisible, desktopRgn);
            DisposeRgn(frontVisible);
        }
    }

    /* Directly call our DeskHook to paint the desktop with the proper pattern */
    Finder_DeskHook(desktopRgn);  /* Pass the desktop region to paint */

    FINDER_LOG_DEBUG("DrawDesktop: frontWindow=%p next=%p\n",
                     frontWindow,
                     frontWindow ? frontWindow->nextWindow : NULL);
    /* Immediately repaint background windows so the pattern stays behind them. */
    if (frontWindow && frontWindow->nextWindow) {
        PaintBehind(frontWindow->nextWindow, desktopRgn);
    }

    /* NOTE: Do NOT call InvalRect here - that would cause infinite recursion!
     * The caller (PostEvent(updateEvt, 0) sites) already requested the update. */

    DisposeRgn(desktopRgn);
    gInDesktopPaint = false;
}

/*
 * RebuildDesktopFile - Rebuilds the desktop database for a volume

 */
OSErr RebuildDesktopFile(short vRefNum)
{
    OSErr err = noErr;
    short databaseRefNum;
    FSSpec databaseSpec;
    HParamBlockRec pb;

    /* Create FSSpec for desktop database */
    err = FSMakeFSSpec(vRefNum, fsRtDirID, kDesktopDatabaseName, &databaseSpec);
    if (err == fnfErr) {
        /* Database doesn't exist, create it */
        err = FSpCreate(&databaseSpec, 'DMGR', 'DTBS', smSystemScript);
        if (err != noErr && err != dupFNErr) {
            return err;
        }
    } else if (err != noErr) {
        return err;
    }

    /* Open desktop database file */
    err = FSpOpenDF(&databaseSpec, fsWrPerm, &databaseRefNum);
    if (err != noErr) {
        return err;
    }

    /* Clear existing database content */
    err = SetEOF(databaseRefNum, 0);
    if (err != noErr) {
        FSClose(databaseRefNum);
        return err;
    }

    /* Scan volume and rebuild desktop entries */
    pb.ioCompletion = nil;
    pb.ioNamePtr = nil;
    pb.ioVRefNum = vRefNum;
    pb.u.volumeParam.ioVolIndex = 0;

    err = PBHGetVInfoSync(&pb);
    if (err == noErr) {
        /* Recursively scan directories and build database */
        err = ScanDirectoryForDesktopEntries(vRefNum, fsRtDirID, databaseRefNum);
    }

    FSClose(databaseRefNum);
    return err;
}

/*
 * GetDesktopIconPosition - Gets the position of an icon on the desktop

 */
OSErr GetDesktopIconPosition(FSSpec *item, Point *position)
{
    short iconIndex;
    UInt32 itemID;

    if (item == nil || position == nil) {
        return paramErr;
    }

    /* Generate unique ID for item */
    itemID = (UInt32)item->parID ^ (UInt32)item->name[0];

    /* Search for icon in desktop array */
    for (iconIndex = 0; iconIndex < gDesktopIconCount; iconIndex++) {
        if (gDesktopIcons[iconIndex].iconID == itemID) {
            *position = gDesktopIcons[iconIndex].position;
            return noErr;
        }
    }

    /* Icon not found on desktop */
    return fnfErr;
}

/*
 * SetDesktopIconPosition - Sets the position of an icon on the desktop

 */
OSErr SetDesktopIconPosition(FSSpec *item, Point position)
{
    OSErr err;
    short iconIndex;
    UInt32 itemID;
    Boolean found = false;

    if (item == nil) {
        return paramErr;
    }

    /* Ensure desktop icons array is allocated */
    if (gDesktopIcons == nil) {
        err = AllocateDesktopIcons();
        if (err != noErr) return err;
    }

    /* Generate unique ID for item */
    itemID = (UInt32)item->parID ^ (UInt32)item->name[0];

    /* Search for existing icon */
    for (iconIndex = 0; iconIndex < gDesktopIconCount; iconIndex++) {
        if (gDesktopIcons[iconIndex].iconID == itemID) {
            gDesktopIcons[iconIndex].position = position;
            found = true;
            break;
        }
    }

    /* Add new icon if not found */
    if (!found) {
        if (gDesktopIconCount >= kMaxDesktopIcons) {
            return memFullErr;
        }

        gDesktopIcons[gDesktopIconCount].iconID = itemID;
        gDesktopIcons[gDesktopIconCount].position = position;
        gDesktopIconCount++;
    }

    /* Save updated positions */
    err = SaveDesktopDatabase(0);
    return err;
}

/*
 * InitializeDesktopDB - Initialize desktop database system

 */
OSErr InitializeDesktopDB(void)
{
    OSErr err;

    extern void serial_puts(const char* str);
    serial_puts("Desktop: InitializeDesktopDB called\n");

    /* Initialize Pattern Manager first */
    PM_Init();

    /* Try different ppat IDs to see which ones work */
    DesktopPref pref;
    serial_puts("Desktop: Testing different ppat patterns\n");

    /* Try custom PPAT8 patterns that we know have color data */
    int16_t ppat_ids[] = {304}; /* Pattern 304 - BluePixel custom pattern */
    int num_ppats = sizeof(ppat_ids) / sizeof(ppat_ids[0]);

    bool found_working = false;
    for (int i = 0; i < num_ppats; i++) {
        char msg[80];
        sprintf(msg, "Desktop: Trying ppat ID %d\n", ppat_ids[i]);
        serial_puts(msg);

        pref.usePixPat = true;
        pref.patID = 16;      /* Fallback PAT if ppat fails */
        pref.ppatID = ppat_ids[i];
        pref.backColor.red = 0xC000;
        pref.backColor.green = 0xC000;
        pref.backColor.blue = 0xC000;

        if (PM_ApplyDesktopPref(&pref)) {
            sprintf(msg, "Desktop: SUCCESS - ppat ID %d loaded!\n", ppat_ids[i]);
            serial_puts(msg);
            found_working = true;
            break;  /* Use the first one that works */
        } else {
            sprintf(msg, "Desktop: Failed ppat ID %d\n", ppat_ids[i]);
            serial_puts(msg);
        }
    }

    if (!found_working) {
        serial_puts("Desktop: No ppat patterns loaded successfully, using fallback\n");
        pref.usePixPat = false;
        pref.patID = 16;  /* Use dots pattern as fallback */
        PM_ApplyDesktopPref(&pref);
    }

    /* Allocate desktop icons array */
    err = AllocateDesktopIcons();
    if (err != noErr) return err;

    /* Load existing desktop database */
    err = LoadDesktopDatabase(0); /* System volume */
    if (err != noErr) {
        /* If load fails, keep trash as the only item (gDesktopIconCount = 1) */
        /* Don't reset to 0 - trash is already initialized at index 0 */
    }

    /* Register our DeskHook with Window Manager */
    SetDeskHook(Finder_DeskHook);

    return noErr;
}

/* Static helper functions */

/*
 * AllocateDesktopIcons - Allocate memory for desktop icons array
 */
static OSErr AllocateDesktopIcons(void)
{
    if (gDesktopIcons != NULL) {
        serial_puts("Desktop: AllocateDesktopIcons skipped (already allocated)\n");
        return noErr; /* Already allocated */
    }

    serial_puts("Desktop: AllocateDesktopIcons allocating with NewPtr\n");
    gDesktopIcons = (DesktopItem*)NewPtr(sizeof(DesktopItem) * kMaxDesktopIcons);
    if (gDesktopIcons == NULL) {
        serial_puts("Desktop: NewPtr failed, falling back to calloc\n");
        gDesktopIcons = (DesktopItem*)NewPtrClear((kMaxDesktopIcons) * (sizeof(DesktopItem)));
        if (gDesktopIcons == NULL) {
            serial_puts("Desktop: calloc failed, using static storage\n");
            gDesktopIcons = gDesktopIconStatic;
            memset(gDesktopIcons, 0, sizeof(gDesktopIconStatic));
            gDesktopIconStaticInUse = true;
        }
    } else {
        gDesktopIconStaticInUse = false;
    }

    /* Initialize trash as the first desktop item */
    gDesktopIcons[0].type = kDesktopItemTrash;
    gDesktopIcons[0].iconID = 0xFFFFFFFF;
    gDesktopIcons[0].position.h = fb_width - 100;
    gDesktopIcons[0].position.v = fb_height - 80;
    strcpy(gDesktopIcons[0].name, "Trash");
    gDesktopIcons[0].movable = false;  /* Trash stays in place */
    gDesktopIconCount = 1;  /* Start with trash */
    gVolumeIconVisible = true;  /* Ensure trash renders even if volume add fails */

    {
        char msg[96];
        sprintf(msg, "Desktop: AllocateDesktopIcons success, count=%d\n", gDesktopIconCount);
        serial_puts(msg);
    }

    return noErr;
}

#if 0  /* Unused helper functions - reserved for future icon positioning features */
/*
 * CalculateNextIconPosition - Calculate next available icon position
 */
static Point CalculateNextIconPosition(void)
{
    Point pos;
    pos.h = kDesktopMargin;
    pos.v = kDesktopMargin + 40;

    /* Simple increment for now */
    if (gDesktopIconCount > 0) {
        pos.h += (gDesktopIconCount % 10) * kDesktopIconSpacing;
        pos.v += (gDesktopIconCount / 10) * kDesktopIconSpacing;
    }

    return pos;
}

/*
 * IsPositionOccupied - Check if a position is already occupied
 */
static Boolean IsPositionOccupied(Point position)
{
    short i;
    for (i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].position.h == position.h &&
            gDesktopIcons[i].position.v == position.v) {
            return true;
        }
    }
    return false;
}
#endif  /* Unused helper functions */

/*
 * SnapToGrid - Snap position to grid (System 7 style)
 */
static Point SnapToGrid(Point p)
{
    extern QDGlobals qd;

    /* Round to nearest grid */
    p.h = ((p.h + kGridW / 2) / kGridW) * kGridW;
    p.v = ((p.v + kGridH / 2) / kGridH) * kGridH;

    /* Clamp to screen bounds (leave room for 32x32 icon) */
    if (p.h < 0) p.h = 0;
    if (p.v < 20) p.v = 20;  /* Below menu bar */
    if (p.h > qd.screenBits.bounds.right - 32) p.h = qd.screenBits.bounds.right - 32;
    if (p.v > qd.screenBits.bounds.bottom - 32) p.v = qd.screenBits.bounds.bottom - 32;

    return p;
}

/*
 * UpdateIconRect - Update icon rect from position
 */
static void UpdateIconRect(short iconIndex, Rect *outRect)
{
    if (iconIndex < 0 || iconIndex >= gDesktopIconCount) return;

    outRect->left   = gDesktopIcons[iconIndex].position.h;
    outRect->top    = gDesktopIcons[iconIndex].position.v;
    outRect->right  = gDesktopIcons[iconIndex].position.h + kIconW;
    outRect->bottom = gDesktopIcons[iconIndex].position.v + kIconH;
}

/*
 * IconAtPoint - Find icon at point (returns index, -1 if none)
 */
static short IconAtPoint(Point where)
{
    Rect iconRect, labelRect;

    FINDER_LOG_DEBUG("IconAtPoint: checking (%d,%d), gDesktopIconCount=%d\n",
                 where.h, where.v, gDesktopIconCount);

    /* Check all desktop items */
    for (int i = 0; i < gDesktopIconCount; i++) {
        FINDER_LOG_DEBUG("IconAtPoint: Checking item %d (type=%d) at (%d,%d)\n",
                     i, gDesktopIcons[i].type,
                     gDesktopIcons[i].position.h, gDesktopIcons[i].position.v);

        SetRect(&iconRect,
                gDesktopIcons[i].position.h,
                gDesktopIcons[i].position.v,
                gDesktopIcons[i].position.h + kIconW,
                gDesktopIcons[i].position.v + kIconH);

        /* Calculate label rect based on item type */
        int labelOffset = (gDesktopIcons[i].type == kDesktopItemTrash) ? 48 : kIconH;
        SetRect(&labelRect,
                gDesktopIcons[i].position.h - 20,
                gDesktopIcons[i].position.v + labelOffset,
                gDesktopIcons[i].position.h + kIconW + 20,
                gDesktopIcons[i].position.v + labelOffset + 16);

        FINDER_LOG_DEBUG("IconAtPoint: Item %d rects: icon=(%d,%d,%d,%d), label=(%d,%d,%d,%d)\n",
                     i, iconRect.left, iconRect.top, iconRect.right, iconRect.bottom,
                     labelRect.left, labelRect.top, labelRect.right, labelRect.bottom);

        if (PtInRect(where, &iconRect) || PtInRect(where, &labelRect)) {
            FINDER_LOG_DEBUG("IconAtPoint: HIT item %d!\n", i);
            return i;
        }
    }

    FINDER_LOG_DEBUG("IconAtPoint: No icon hit, returning -1\n");
    return -1;
}

/*
 * Ghost outline state for drag tracking
 */
static Boolean sGhostActive = false;
static Rect    sGhostRect   = {0,0,0,0};

/*
 * GhostXOR - Low-level XOR rect drawing using screen port directly
 */
static void GhostXOR(const Rect* r)
{
    /* Direct XOR rectangle drawing to framebuffer for immediate visibility */
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;

    if (!framebuffer || !r) return;

    FINDER_LOG_DEBUG("GhostXOR: received Rect top=%d left=%d bottom=%d right=%d\n",
                 r->top, r->left, r->bottom, r->right);

    uint32_t* fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    /* Clip to screen bounds, clamping fb dimensions to SInt16 range */
    short safe_width = (fb_width <= 32767) ? (short)fb_width : 32767;
    short safe_height = (fb_height <= 32767) ? (short)fb_height : 32767;
    short left = (r->left >= 0) ? r->left : 0;
    short top = (r->top >= 0) ? r->top : 0;
    short right = (r->right <= safe_width) ? r->right : safe_width;
    short bottom = (r->bottom <= safe_height) ? r->bottom : safe_height;

    FINDER_LOG_DEBUG("GhostXOR: drawing at X=%d-%d Y=%d-%d\n", left, right, top, bottom);

    if (left >= right || top >= bottom) return;

    /* XOR color: white (inverts on blue desktop) */
    uint32_t xor_color = 0xFFFFFFFF;

    /* Draw thick (3px) XOR rectangle frame for better visibility */
    /* Top edge (3 pixels thick) */
    for (int y = top; y < top + 3 && y < bottom; y++) {
        for (int x = left; x < right; x++) {
            fb[y * pitch + x] ^= xor_color;
        }
    }

    /* Bottom edge (3 pixels thick) */
    for (int y = bottom - 3; y < bottom && y >= top; y++) {
        for (int x = left; x < right; x++) {
            fb[y * pitch + x] ^= xor_color;
        }
    }

    /* Left edge (3 pixels thick, avoiding corners already drawn) */
    for (int y = top + 3; y < bottom - 3; y++) {
        for (int x = left; x < left + 3 && x < right; x++) {
            fb[y * pitch + x] ^= xor_color;
        }
    }

    /* Right edge (3 pixels thick, avoiding corners already drawn) */
    for (int y = top + 3; y < bottom - 3; y++) {
        for (int x = right - 3; x < right && x >= left; x++) {
            fb[y * pitch + x] ^= xor_color;
        }
    }

    /* Force immediate display update */
    extern void QDPlatform_UpdateScreen(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom);
    QDPlatform_UpdateScreen(left, top, right, bottom);

    FINDER_LOG_DEBUG("GhostXOR: Drew XOR rect (%d,%d,%d,%d)\n", left, top, right, bottom);
}

/*
 * GhostEraseIf - Erase ghost if active
 */
static inline void GhostEraseIf(void)
{
    if (sGhostActive) {
        GhostXOR(&sGhostRect);  /* XOR twice = erase */
        sGhostActive = false;
    }
}

/*
 * Desktop_GhostEraseIf - Public wrapper for folder windows to erase ghost safely
 */
void Desktop_GhostEraseIf(void)
{
    GhostEraseIf();
}

/*
 * Desktop_GhostShowAt - Public wrapper for folder windows to show ghost at rect
 */
void Desktop_GhostShowAt(const Rect* r)
{
    GhostShowAt(r);
}

/*
 * GhostShowAt - Show ghost at new position
 */
static inline void GhostShowAt(const Rect* r)
{
    GhostEraseIf();  /* Erase old first */
    sGhostRect = *r;
    GhostXOR(&sGhostRect);
    sGhostActive = true;
}

/*
 * DesktopYield - Keep system ticking during modal loops
 */
static void DesktopYield(void)
{
    extern void SystemTask(void);
    extern void ProcessModernInput(void);
    static int yieldCount = 0;
    if (++yieldCount % 100 == 0) {
        FINDER_LOG_DEBUG("[DesktopYield] Called %d times\n", yieldCount);
    }

    /* Don't call EventPumpYield() here - it can cause re-entrancy issues during drag.
     * ProcessModernInput() polls PS/2 input AND updates gCurrentButtons */
    ProcessModernInput();  /* Update mouse/keyboard state including gCurrentButtons */
    SystemTask();    /* Handle time manager tasks */
}

/*
 * TrackIconDragSync - System 7 style synchronous modal drag with XOR ghost
 * This runs immediately when mouseDown is received, polls StillDown() directly
 */
static void TrackIconDragSync(short iconIndex, Point startPt)
{
    Rect ghost;
    Boolean didDrag = false;  /* Track if icon actually moved during drag */
    GrafPtr savePort;

    extern Boolean StillDown(void);
    extern void GetMouse(Point *pt);

    if (iconIndex < 0 || iconIndex >= gDesktopIconCount) return;

    FINDER_LOG_DEBUG("TrackIconDragSync ENTRY: starting modal drag for icon %d\n", iconIndex);

    /* Set tracking guard to suppress queued mouse events */
    gInMouseTracking = true;

    /* Mark this icon as being dragged */
    gDraggingIconIndex = iconIndex;

    /* Prepare ghost rect */
    UpdateIconRect(iconIndex, &ghost);
    ghost.left -= 20;
    ghost.right += 20;
    ghost.bottom += 16;

    /* Safety timeout to prevent infinite loops */
    UInt32 loopCount = 0;
    const UInt32 MAX_DRAG_ITERATIONS = 100000;  /* ~1666 seconds at 60Hz */

    /* Threshold using current button state (PS/2/USB safe) */
    Point last = startPt, cur;
    extern volatile UInt8 gCurrentButtons;

    while ((gCurrentButtons & 1) != 0 && loopCount < MAX_DRAG_ITERATIONS) {
        loopCount++;
        GetMouse(&cur);
        if (abs(cur.h - startPt.h) >= kDragThreshold || abs(cur.v - startPt.v) >= kDragThreshold) {
            last = cur;
            FINDER_LOG_DEBUG("TrackIconDragSync: threshold exceeded, starting drag\n");
            break;  /* start drag */
        }
        DesktopYield();
    }

    /* Check if we hit timeout in threshold wait loop */
    if (loopCount >= MAX_DRAG_ITERATIONS) {
        FINDER_LOG_ERROR("TrackIconDragSync: TIMEOUT in threshold wait! Looped %u times, button never released!\n", loopCount);
        FINDER_LOG_ERROR("TrackIconDragSync: This indicates mouse button tracking is broken.\n");
        gDraggingIconIndex = -1;
        gInMouseTracking = false;
        return;
    }

    if ((gCurrentButtons & 1) == 0) {  /* released before threshold */
        FINDER_LOG_DEBUG("TrackIconDragSync: button released before threshold (after %u iterations)\n", loopCount);
        gDraggingIconIndex = -1;
        gInMouseTracking = false;
        return;
    }

    /* Enter drag: draw XOR ghost outline following cursor */
    GetPort(&savePort);
    SetPort(qd.thePort);  /* use screen port */
    ClipRect(&qd.screenBits.bounds);
    GhostShowAt(&ghost);  /* visible immediately */
    FINDER_LOG_DEBUG("TrackIconDragSync: ghost visible, entering drag loop\n");

    /* Reset loop counter for drag loop */
    loopCount = 0;

    while ((gCurrentButtons & 1) != 0 && loopCount < MAX_DRAG_ITERATIONS) {
        loopCount++;
        DesktopYield();  /* keeps input flowing but we don't paint desktop */
        GetMouse(&cur);

        if (cur.h | cur.v) {  /* cheap branch predictor nudge */
            if (cur.h != last.h || cur.v != last.v) {
                /* Move by per-frame delta (cur - last, not cur - start) */
                OffsetRect(&ghost, cur.h - last.h, cur.v - last.v);
                didDrag = true;  /* Icon actually moved */

                /* Constrain to desktop bounds */
                Rect desk = qd.screenBits.bounds;
                desk.top = 20;  /* Below menu bar */

                if (ghost.left < desk.left)     OffsetRect(&ghost, desk.left - ghost.left, 0);
                if (ghost.top < desk.top)       OffsetRect(&ghost, 0, desk.top - ghost.top);
                if (ghost.right > desk.right)   OffsetRect(&ghost, desk.right - ghost.right, 0);
                if (ghost.bottom > desk.bottom) OffsetRect(&ghost, 0, desk.bottom - ghost.bottom);

                GhostShowAt(&ghost);  /* erase old, draw new */
                last = cur;
            }
        }
    }

    /* Check if we hit timeout in drag loop */
    if (loopCount >= MAX_DRAG_ITERATIONS) {
        FINDER_LOG_ERROR("TrackIconDragSync: TIMEOUT in drag loop! Looped %u times, button never released!\n", loopCount);
        FINDER_LOG_ERROR("TrackIconDragSync: This indicates mouse button tracking is broken.\n");
        GhostEraseIf();
        SetPort(savePort);
        gDraggingIconIndex = -1;
        gInMouseTracking = false;
        return;
    }

    /* Button released: erase ghost, restore port */
    GhostEraseIf();
    SetPort(savePort);
    FINDER_LOG_DEBUG("TrackIconDragSync: drag complete after %u iterations, ghost erased\n", loopCount);

    /* Determine drop target and action */
    Point dropPoint = (Point){ .h = ghost.left + 20 + 16, .v = ghost.top + 16 };  /* Icon center */
    DesktopItem* item = &gDesktopIcons[iconIndex];
    Boolean invalidDrop = false;

    /* Get modifier keys to determine action (option = alias, cmd = copy) */
    extern void GetKeys(KeyMap theKeys);
    KeyMap keys;
    GetKeys(keys);
    /* Check for option key (0x3A = option key scancode, byte 7, bit 2) */
    Boolean optionKeyDown = (keys[7] & 0x04) != 0;
    Boolean cmdKeyDown = (keys[7] & 0x80) != 0;  /* Cmd key */

    FINDER_LOG_DEBUG("TrackIconDragSync: Modifiers - option=%d cmd=%d\n", optionKeyDown, cmdKeyDown);

    /* Check drop targets in priority order */
    Boolean droppedOnTrash = Desktop_IsOverTrash(dropPoint);

    if (droppedOnTrash && iconIndex != 0 && item->type != kDesktopItemVolume) {
        /* DROP TARGET: Trash - always move to trash folder with name conflict resolution */
        FINDER_LOG_DEBUG("TrackIconDragSync: Dropped on trash! Moving to trash folder\n");

        extern bool Trash_MoveNode(VRefNum vref, DirID parent, FileID id);
        extern VRefNum VFS_GetBootVRef(void);

        if (item->iconID != 0xFFFFFFFF) {
            VRefNum vref = VFS_GetBootVRef();
            if (Trash_MoveNode(vref, HFS_ROOT_DIR_ID, item->iconID)) {
                FINDER_LOG_DEBUG("TrackIconDragSync: Successfully moved to trash\n");

                /* Remove from desktop array */
                for (short i = iconIndex; i < gDesktopIconCount - 1; i++) {
                    gDesktopIcons[i] = gDesktopIcons[i + 1];
                }
                gDesktopIconCount--;
                gSelectedIcon = -1;
            } else {
                FINDER_LOG_DEBUG("TrackIconDragSync: Trash operation failed\n");
            }
        }
    } else if (droppedOnTrash && (iconIndex == 0 || item->type == kDesktopItemVolume)) {
        /* Cannot trash the trash itself or volume icons */
        FINDER_LOG_DEBUG("TrackIconDragSync: Cannot trash this item\n");
        invalidDrop = true;
    } else {
        /* DROP TARGET: Desktop or folder window */
        extern short FindWindow(Point thePoint, WindowPtr* theWindow);
        extern VRefNum VFS_GetVRefByID(FileID id);
        extern VRefNum VFS_GetBootVRef(void);
        extern bool VFS_GetParentDir(VRefNum vref, FileID id, DirID* parentDir);
        extern bool VFS_Copy(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName, FileID* newID);
        extern OSErr CreateAlias(FSSpec* target, FSSpec* aliasFile);

        WindowPtr hitWindow = NULL;
        short partCode = FindWindow(dropPoint, &hitWindow);

        /* Check if dropped on a folder window */
        Boolean droppedOnFolder = (hitWindow != NULL && partCode == inContent);
        DirID targetDir = HFS_ROOT_DIR_ID;  /* Default to desktop (root) */
        VRefNum vref = VFS_GetBootVRef();

        if (droppedOnFolder) {
            /* TODO: Get actual folder window's DirID from window refCon */
            FINDER_LOG_DEBUG("TrackIconDragSync: Dropped on folder window\n");
            /* For now, treat as desktop drop */
            droppedOnFolder = false;
        }

        /* Get source volume and directory */
        VRefNum sourceVRef = VFS_GetVRefByID(item->iconID);
        DirID sourceDir = HFS_ROOT_DIR_ID;
        VFS_GetParentDir(sourceVRef, item->iconID, &sourceDir);

        /* Determine if cross-volume */
        Boolean crossVolume = (sourceVRef != vref);

        if (optionKeyDown) {
            /* Option key: create alias */
            FINDER_LOG_DEBUG("TrackIconDragSync: Creating alias\n");

            FSSpec target, aliasFile;
            target.vRefNum = sourceVRef;
            target.parID = sourceDir;
            /* Copy name from item */
            int nameLen = strlen(item->name);
            if (nameLen > 31) nameLen = 31;
            memcpy(target.name, item->name, nameLen);
            target.name[nameLen] = 0;

            /* Create alias on desktop with " alias" suffix */
            aliasFile.vRefNum = vref;
            aliasFile.parID = targetDir;
            char aliasName[64];
            snprintf(aliasName, sizeof(aliasName), "%s alias", item->name);
            if (strlen(aliasName) > 31) aliasName[31] = 0;
            memcpy(aliasFile.name, aliasName, strlen(aliasName) + 1);

            if (CreateAlias(&target, &aliasFile) == noErr) {
                FINDER_LOG_DEBUG("TrackIconDragSync: Alias created successfully\n");
            } else {
                FINDER_LOG_DEBUG("TrackIconDragSync: Alias creation failed\n");
                invalidDrop = true;
            }
        } else if (cmdKeyDown || crossVolume) {
            /* Cmd key or cross-volume: force copy */
            FINDER_LOG_DEBUG("TrackIconDragSync: Copying file (cmd=%d, crossVol=%d)\n", cmdKeyDown, crossVolume);

            char copyName[32];
            extern bool VFS_GenerateUniqueName(VRefNum vref, DirID dir, const char* base, char* out);
            VFS_GenerateUniqueName(vref, targetDir, item->name, copyName);

            FileID newID = 0;
            if (VFS_Copy(vref, sourceDir, item->iconID, targetDir, copyName, &newID)) {
                FINDER_LOG_DEBUG("TrackIconDragSync: Copy succeeded, newID=%u\n", newID);
            } else {
                FINDER_LOG_DEBUG("TrackIconDragSync: Copy operation failed\n");
                invalidDrop = true;
            }
        } else {
            /* No modifiers: default behavior (move on same volume or reposition desktop icon) */
            if (droppedOnFolder) {
                /* Move to folder */
                extern bool VFS_Move(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName);
                if (VFS_Move(vref, sourceDir, item->iconID, targetDir, NULL)) {
                    FINDER_LOG_DEBUG("TrackIconDragSync: Moved to folder\n");
                    /* Remove from desktop */
                    for (short i = iconIndex; i < gDesktopIconCount - 1; i++) {
                        gDesktopIcons[i] = gDesktopIcons[i + 1];
                    }
                    gDesktopIconCount--;
                    gSelectedIcon = -1;
                }
            } else if (gDesktopIcons[iconIndex].movable) {
                /* Reposition on desktop */
                Point snapped = (Point){ .h = ghost.left + 20, .v = ghost.top };
                snapped = SnapToGrid(snapped);
                gDesktopIcons[iconIndex].position = snapped;
                FINDER_LOG_DEBUG("TrackIconDragSync: Repositioned to (%d,%d)\n", snapped.h, snapped.v);
            }
        }
    }

    /* Handle operation result */
    if (invalidDrop) {
        /* Invalid drop: beep and restore to original position */
        FINDER_LOG_DEBUG("TrackIconDragSync: Invalid drop - would beep here\n");
        extern void SysBeep(short duration);
        SysBeep(1);  /* System beep */
        /* Icon position already unchanged for invalid drops */
    }

    /* Post updateEvt to defer desktop redraw (no reentrancy) */
    PostEvent(updateEvt, 0);  /* desktop repaint */
    FINDER_LOG_DEBUG("TrackIconDragSync: posted updateEvt\n");

    /* If we actually dragged, clear same-icon tracking to prevent accidental open */
    if (didDrag) {
        FINDER_LOG_DEBUG("TrackIconDragSync: didDrag=true, clearing sLastClickIcon\n");
        sLastClickIcon = -1;
    }

    /* Clear tracking guard and drag flag */
    FINDER_LOG_DEBUG("TrackIconDragSync: clearing tracking flags\n");
    gDraggingIconIndex = -1;
    gInMouseTracking = false;

    /* Drain any queued mouseUp that may have been posted anyway */
    FINDER_LOG_DEBUG("TrackIconDragSync: checking for queued mouseUp\n");
    EventRecord e;
    if (EventAvail(mUpMask, &e)) {
        FINDER_LOG_DEBUG("TrackIconDragSync: draining mouseUp event\n");
        EventRecord dump;
        GetNextEvent(mUpMask, &dump);
    }
    FINDER_LOG_DEBUG("TrackIconDragSync: EXIT\n");
}

/*
 * LoadDesktopDatabase - Load desktop positions from database file

 */
static OSErr LoadDesktopDatabase(short vRefNum)
{
    OSErr err;
    FSSpec databaseSpec;
    short databaseRefNum;
    long dataSize;

    /* Create FSSpec for desktop database */
    err = FSMakeFSSpec(vRefNum, fsRtDirID, kDesktopDatabaseName, &databaseSpec);
    if (err != noErr) {
        return err; /* Database doesn't exist */
    }

    /* Open database file */
    err = FSpOpenDF(&databaseSpec, fsRdPerm, &databaseRefNum);
    if (err != noErr) {
        return err;
    }

    /* Read icon count */
    dataSize = sizeof(short);
    err = FSRead(databaseRefNum, &dataSize, &gDesktopIconCount);
    if (err != noErr) {
        FSClose(databaseRefNum);
        return err;
    }

    /* Validate icon count */
    if (gDesktopIconCount > kMaxDesktopIcons) {
        gDesktopIconCount = kMaxDesktopIcons;
    }

    /* Read desktop items */
    if (gDesktopIconCount > 0) {
        dataSize = sizeof(DesktopItem) * gDesktopIconCount;
        err = FSRead(databaseRefNum, &dataSize, gDesktopIcons);
    }

    /* Ensure at least the trash icon exists so the desktop is never empty */
    if (gDesktopIconCount <= 0 || gDesktopIcons[0].type != kDesktopItemTrash) {
        gDesktopIconCount = 1;
        gDesktopIcons[0].type = kDesktopItemTrash;
        gDesktopIcons[0].iconID = 0xFFFFFFFF;
        gDesktopIcons[0].position.h = fb_width - 100;
        gDesktopIcons[0].position.v = fb_height - 80;
        strcpy(gDesktopIcons[0].name, "Trash");
        gDesktopIcons[0].movable = false;
    }

    gVolumeIconVisible = true;  /* Allow DrawVolumeIcon to render icons */

    FSClose(databaseRefNum);
    return err;
}

/*
 * SaveDesktopDatabase - Save desktop positions to database file

 */
static OSErr SaveDesktopDatabase(short vRefNum)
{
    OSErr err;
    FSSpec databaseSpec;
    short databaseRefNum;
    long dataSize;

    /* Create FSSpec for desktop database */
    err = FSMakeFSSpec(vRefNum, fsRtDirID, kDesktopDatabaseName, &databaseSpec);
    if (err == fnfErr) {
        /* Create database file */
        err = FSpCreate(&databaseSpec, 'DMGR', 'DTBS', smSystemScript);
        if (err != noErr && err != dupFNErr) {
            return err;
        }
    } else if (err != noErr) {
        return err;
    }

    /* Open database file */
    err = FSpOpenDF(&databaseSpec, fsWrPerm, &databaseRefNum);
    if (err != noErr) {
        return err;
    }

    /* Write icon count */
    dataSize = sizeof(short);
    err = FSWrite(databaseRefNum, &dataSize, &gDesktopIconCount);
    if (err != noErr) {
        FSClose(databaseRefNum);
        return err;
    }

    /* Write desktop items */
    if (gDesktopIconCount > 0) {
        dataSize = sizeof(DesktopItem) * gDesktopIconCount;
        err = FSWrite(databaseRefNum, &dataSize, gDesktopIcons);
    }

    FSClose(databaseRefNum);
    return err;
}

/*
 * ScanDirectoryForDesktopEntries - Recursively scan directory for desktop database

 */
static OSErr ScanDirectoryForDesktopEntries(short vRefNum, long dirID, short databaseRefNum)
{
    OSErr err = noErr;
    CInfoPBRec pb;
    Str255 itemName;
    short itemIndex = 1;
    DesktopRecord record;
    long dataSize;

    /* Scan all items in directory */
    do {
        pb.ioCompletion = nil;
        pb.ioNamePtr = itemName;
        pb.ioVRefNum = vRefNum;
        pb.u.hFileInfo.ioDirID = dirID;
        pb.u.hFileInfo.ioFDirIndex = itemIndex;

        err = PBGetCatInfoSync(&pb);
        if (err == noErr) {
            /* Create desktop record for this item */
            record.recordType = (pb.u.hFileInfo.ioFlAttrib & ioDirMask) ? 1 : 0; /* Folder or file */
            record.fileType = pb.u.hFileInfo.ioFlFndrInfo.fdType;
            record.creator = pb.u.hFileInfo.ioFlFndrInfo.fdCreator;
            record.iconLocalID = 0; /* Will be assigned later */
            record.iconType = 0;

            /* Write record to database */
            dataSize = sizeof(DesktopRecord);
            FSWrite(databaseRefNum, &dataSize, &record);

            /* If it's a folder, recurse into it */
            if (pb.u.hFileInfo.ioFlAttrib & ioDirMask) {
                ScanDirectoryForDesktopEntries(vRefNum, pb.u.dirInfo.ioDrDirID, databaseRefNum);
            }

            itemIndex++;
        }
    } while (err == noErr);

    /* fnfErr is expected when we've processed all items */
    return (err == fnfErr) ? noErr : err;
}

/*
 * InitializeVolumeIcon - Initialize boot volume icon on desktop
 * Integrates with VFS to show Macintosh HD
 */
OSErr InitializeVolumeIcon(void)
{
    OSErr err = noErr;
    VolumeControlBlock vcb;

    /* Get boot volume reference */
    gBootVolumeRef = VFS_GetBootVRef();

    /* Get volume info */
    if (!VFS_GetVolumeInfo(gBootVolumeRef, &vcb)) {
        return ioErr;
    }

    /* Ensure desktop icons array is allocated */
    if (gDesktopIcons == nil) {
        err = AllocateDesktopIcons();
        if (err != noErr) return err;
    }

    /* Add volume icon to desktop (note: trash is already at index 0) */
    if (gDesktopIconCount < kMaxDesktopIcons) {
        /* Position at top-right of desktop */
        gDesktopIcons[gDesktopIconCount].type = kDesktopItemVolume;
        gDesktopIcons[gDesktopIconCount].iconID = 0xFFFFFFFF; /* Special ID for volume */
        gDesktopIcons[gDesktopIconCount].position.h = fb_width - 100;
        gDesktopIcons[gDesktopIconCount].position.v = 60;
        strcpy(gDesktopIcons[gDesktopIconCount].name, "Macintosh HD");
        gDesktopIcons[gDesktopIconCount].movable = true;  /* Volumes can be moved */
        gDesktopIcons[gDesktopIconCount].data.volume.vRefNum = gBootVolumeRef;

        FINDER_LOG_DEBUG("InitializeVolumeIcon: Added volume icon at index %d, pos=(%d,%d)\n",
                      gDesktopIconCount, fb_width - 100, 60);

        gDesktopIconCount++;
        gVolumeIconVisible = true;

        FINDER_LOG_DEBUG("InitializeVolumeIcon: gDesktopIconCount now = %d\n", gDesktopIconCount);
    }

    return noErr;
}

/*
 * Desktop_AddVolumeIcon - Add a volume icon to the desktop
 * Called when a new volume is mounted
 *
 * Parameters:
 *   name - Volume name to display
 *   vref - Volume reference number
 *
 * Returns: OSErr (noErr on success)
 */
OSErr Desktop_AddVolumeIcon(const char* name, VRefNum vref) {
    extern uint32_t fb_width, fb_height;

    if (!name || gDesktopIconCount >= kMaxDesktopIcons) {
        return paramErr;
    }

    /* Ensure desktop icons array is allocated */
    if (gDesktopIcons == nil) {
        OSErr err = AllocateDesktopIcons();
        if (err != noErr) return err;
    }

    /* Calculate position (stack volumes on right side) */
    /* Count existing volume icons to calculate Y position */
    int volumeCount = 0;
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].type == kDesktopItemVolume) {
            volumeCount++;
        }
    }

    /* Add volume icon */
    DesktopItem* item = &gDesktopIcons[gDesktopIconCount];
    item->type = kDesktopItemVolume;
    item->iconID = 0xFFFFFFFF; /* Special ID for volume */
    item->position.h = fb_width - 100;
    item->position.v = 60 + (volumeCount * 80); /* Stack vertically */
    strncpy(item->name, name, sizeof(item->name) - 1);
    item->name[sizeof(item->name) - 1] = '\0';
    item->movable = true;
    item->data.volume.vRefNum = vref;

    FINDER_LOG_DEBUG("Desktop_AddVolumeIcon: Added '%s' (vRef %d) at index %d, pos=(%d,%d)\n",
                  name, vref, gDesktopIconCount, item->position.h, item->position.v);

    gDesktopIconCount++;

    /* Desktop will be redrawn on next event loop iteration */

    return noErr;
}

/*
 * Desktop_RemoveVolumeIcon - Remove a volume icon from the desktop
 * Called when a volume is unmounted
 *
 * Parameters:
 *   vref - Volume reference number to remove
 *
 * Returns: OSErr (noErr on success)
 */
OSErr Desktop_RemoveVolumeIcon(VRefNum vref) {
    if (!gDesktopIcons) {
        return paramErr;
    }

    /* Find the volume icon */
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].type == kDesktopItemVolume &&
            gDesktopIcons[i].data.volume.vRefNum == vref) {

            FINDER_LOG_DEBUG("Desktop_RemoveVolumeIcon: Removing volume icon for vRef %d at index %d\n", vref, i);

            /* Shift remaining icons down */
            for (int j = i; j < gDesktopIconCount - 1; j++) {
                gDesktopIcons[j] = gDesktopIcons[j + 1];
            }

            gDesktopIconCount--;

            /* Desktop will be redrawn on next event loop iteration */

            return noErr;
        }
    }

    FINDER_LOG_DEBUG("Desktop_RemoveVolumeIcon: Volume icon for vRef %d not found\n", vref);
    return fnfErr; /* Not found */
}

/*
 * Desktop_AddAliasIcon - Add an alias icon to the desktop (PUBLIC API)
 * Creates a desktop alias/shortcut pointing to a file or folder
 *
 * Parameters:
 *   name      - Display name for the alias
 *   position  - Position on desktop (global coordinates)
 *   targetID  - FileID of the target file/folder
 *   vref      - Volume reference containing the target
 *   isFolder  - true if target is a folder, false for file
 *
 * Returns: OSErr (noErr on success)
 */
OSErr Desktop_AddAliasIcon(const char* name, Point position, FileID targetID,
                           VRefNum vref, Boolean isFolder) {
    if (!name || gDesktopIconCount >= kMaxDesktopIcons) {
        return paramErr;
    }

    FINDER_LOG_DEBUG("Desktop_AddAliasIcon: Creating alias '%s' at (%d,%d), targetID=%d, vref=%d\n",
                 name, position.h, position.v, targetID, vref);

    /* Add to desktop icons array */
    DesktopItem* item = &gDesktopIcons[gDesktopIconCount];

    item->type = isFolder ? kDesktopItemFolder : kDesktopItemAlias;
    item->iconID = targetID;  /* Use FileID as iconID */
    item->position = position;
    item->movable = true;  /* Aliases are movable */

    /* Copy name (ensure null termination) */
    size_t nameLen = strlen(name);
    if (nameLen >= 64) nameLen = 63;
    memcpy(item->name, name, nameLen);
    item->name[nameLen] = '\0';

    /* Store alias-specific data */
    if (isFolder) {
        item->data.folder.dirID = (long)targetID;
    } else {
        item->data.alias.targetID = (long)targetID;
    }

    gDesktopIconCount++;

    FINDER_LOG_DEBUG("Desktop_AddAliasIcon: Added alias at index %d, total icons now = %d\n",
                 gDesktopIconCount - 1, gDesktopIconCount);

    /* Request desktop redraw */
    PostEvent(updateEvt, 0);  /* 0 = desktop update */

    return noErr;
}

/*
 * Desktop_IsOverTrash - Check if a point is over the trash icon (PUBLIC API)
 * Used for drag-and-drop trash detection
 *
 * Parameters:
 *   where - Point to test (in global coordinates)
 *
 * Returns: true if point is over trash icon or label, false otherwise
 */
Boolean Desktop_IsOverTrash(Point where) {
    /* Trash is always at index 0 */
    if (gDesktopIconCount == 0 || gDesktopIcons[0].type != kDesktopItemTrash) {
        return false;
    }

    Rect iconRect, labelRect;

    /* Calculate trash icon rect (32x32) */
    SetRect(&iconRect,
            gDesktopIcons[0].position.h,
            gDesktopIcons[0].position.v,
            gDesktopIcons[0].position.h + kIconW,
            gDesktopIcons[0].position.v + kIconH);

    /* Calculate trash label rect (trash has labelOffset=48) */
    SetRect(&labelRect,
            gDesktopIcons[0].position.h - 20,
            gDesktopIcons[0].position.v + 48,
            gDesktopIcons[0].position.h + kIconW + 20,
            gDesktopIcons[0].position.v + 48 + 16);

    return (PtInRect(where, &iconRect) || PtInRect(where, &labelRect));
}

/*
 * DrawVolumeIcon - Draw the boot volume icon on desktop using universal icon system
 */
void DrawVolumeIcon(void)
{
    static Boolean gInVolumeIconPaint = false;
    GrafPtr savePort;
    RgnHandle savedClip = NULL;
    Boolean clipSaved = false;

    FINDER_LOG_DEBUG("DrawVolumeIcon: ENTRY\n");

    /* Erase any active ghost outline before icon redraw */
    GhostEraseIf();

    /* Re-entrancy guard: prevent recursive painting */
    if (gInVolumeIconPaint) {
        FINDER_LOG_DEBUG("DrawVolumeIcon: re-entry detected, skipping to avoid freeze\n");
        return;
    }
    gInVolumeIconPaint = true;

    GetPort(&savePort);
    SetPort(qd.thePort);

    if (qd.thePort->clipRgn && *qd.thePort->clipRgn) {
        savedClip = NewRgn();
        if (savedClip) {
            CopyRgn(qd.thePort->clipRgn, savedClip);
            clipSaved = true;
        }
    }

    Rect desktopBounds = qd.screenBits.bounds;
    desktopBounds.top = 20; /* Keep menu bar clear */
    ClipRect(&desktopBounds);

    if (!gVolumeIconVisible) {
        FINDER_LOG_DEBUG("DrawVolumeIcon: not visible, returning\n");
        gInVolumeIconPaint = false;
        if (clipSaved) {
            SetClip(savedClip);
            DisposeRgn(savedClip);
        }
        SetPort(savePort);
        return;
    }

    FINDER_LOG_DEBUG("DrawVolumeIcon: Drawing desktop icon set\n");
    Desktop_DrawIconsCommon(NULL);
    FINDER_LOG_DEBUG("DrawVolumeIcon: about to return\n");
    gInVolumeIconPaint = false;

    if (clipSaved) {
        SetClip(savedClip);
        DisposeRgn(savedClip);
    } else {
        ClipRect(&qd.screenBits.bounds);
    }
    SetPort(savePort);
    return;  /* Explicit return for debugging */
}
/* DrawVolumeIcon function ends here */

/*
 * HandleDesktopClick - Check if a click is on a desktop icon and handle it
 * Returns true if click was on an icon, false otherwise
 */
Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick)
{
    short prevSelected = gSelectedIcon;
    short hitIcon;
    WindowPtr whichWindow;

    extern short FindWindow(Point thePoint, WindowPtr* theWindow);

    FINDER_LOG_DEBUG("HandleDesktopClick: click at (%d,%d), doubleClick=%d\n",
                  clickPoint.h, clickPoint.v, doubleClick);

    /* System 7 faithful: only handle if click is on desktop (not a window) */
    short part = FindWindow(clickPoint, &whichWindow);
    if (part != inDesk) {
        /* Click was on a window, not desktop - let Window Manager handle it */
        FINDER_LOG_DEBUG("HandleDesktopClick: part=%d (not inDesk), ignoring\n", part);
        return false;
    }

    /* Ensure we're in screen port for global coordinate hit testing */
    extern QDGlobals qd;
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(qd.thePort);

    /* Find which icon was hit */
    hitIcon = IconAtPoint(clickPoint);

    if (hitIcon == -1) {
        /* No icon hit - deselect and clear same-icon tracking */
        if (prevSelected != -1) {
            gSelectedIcon = -1;
            /* Don't redraw here - let Window Manager handle it via DeskHook */
        }
        sLastClickIcon = -1;
        sLastClickTicks = 0;
        SetPort(savePort);
        return false;
    }

    /* Check for double-click ourselves using time threshold (ignore broken event flag) */
    extern UInt32 TickCount(void);
    extern UInt32 GetDblTime(void);
    UInt32 currentTicks = TickCount();
    UInt32 timeSinceLastClick = currentTicks - sLastClickTicks;
    Boolean isDoubleClick = (hitIcon == sLastClickIcon && timeSinceLastClick <= GetDblTime());

    FINDER_LOG_DEBUG("Hit icon index %d, same=%d, dt=%d, threshold=%d, dblClick=%d\n",
                 hitIcon, (hitIcon == sLastClickIcon), (int)timeSinceLastClick,
                 (int)GetDblTime(), (int)isDoubleClick);

    /* Double-click on SAME icon: open immediately, never drag */
    if (isDoubleClick && hitIcon >= 0 && hitIcon < gDesktopIconCount) {
        FINDER_LOG_DEBUG("[DBLCLK SAME ICON] Opening icon %d\n", hitIcon);
        extern WindowPtr Finder_OpenDesktopItem(Boolean isTrash, ConstStr255Param title);
        DesktopItem *it = &gDesktopIcons[hitIcon];

        /* Ensure any ghost from prior drag is erased */
        GhostEraseIf();

        if (it->type == kDesktopItemVolume) {
            /* Build Pascal string for HD title */
            static unsigned char hdTitle[256];
            strcpy((char*)&hdTitle[1], "Macintosh HD");
            hdTitle[0] = (unsigned char)strlen("Macintosh HD");
            Finder_OpenDesktopItem(false, hdTitle);
        } else if (it->type == kDesktopItemTrash) {
            /* Build Pascal string for Trash title */
            static unsigned char trashTitle[256];
            strcpy((char*)&trashTitle[1], "Trash");
            trashTitle[0] = (unsigned char)strlen("Trash");
            Finder_OpenDesktopItem(true, trashTitle);
        }

        /* DO NOT post desktop updateEvt - the window will handle its own updates */
        /* Posting updateEvt(0) would trigger DrawDesktop which paints over windows! */
        sLastClickIcon = -1;  /* Consume double-click */
        SetPort(savePort);
        return true;
    }

    /* Single click or double-click on different icon: select and arm for drag */
    if (hitIcon >= 0 && hitIcon < gDesktopIconCount) {
        gSelectedIcon = hitIcon;

        /* Update same-icon tracking for next potential double-click */
        extern UInt32 TickCount(void);
        sLastClickIcon = hitIcon;
        sLastClickTicks = TickCount();

        PostEvent(updateEvt, 0);  /* selection redraw deferred */

        FINDER_LOG_DEBUG("Single-click: icon %d selected, sLastClickIcon=%d\n", hitIcon, sLastClickIcon);

        extern volatile UInt8 gCurrentButtons;
        if ((gCurrentButtons & 1) != 0) {  /* mouse still down? arm drag */
            FINDER_LOG_DEBUG("Single-click: button still down, starting drag tracking\n");
            SetPort(savePort);
            TrackIconDragSync(hitIcon, clickPoint);
        } else {
            FINDER_LOG_DEBUG("Single-click: button released, no drag\n");
            SetPort(savePort);
        }
        return true;
    }

    SetPort(savePort);

    return true;
}

/*
 * HandleVolumeDoubleClick - Open volume window on double-click
 */
OSErr HandleVolumeDoubleClick(Point clickPoint)
{
    Rect iconRect;
    VolumeControlBlock vcb;

    /* Check if click is on volume icon */
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].type == kDesktopItemVolume) {
            SetRect(&iconRect,
                    gDesktopIcons[i].position.h,
                    gDesktopIcons[i].position.v,
                    gDesktopIcons[i].position.h + 32,
                    gDesktopIcons[i].position.v + 32);

            if (PtInRect(clickPoint, &iconRect)) {
                /* Get volume info */
                if (!VFS_GetVolumeInfo(gBootVolumeRef, &vcb)) {
                    return ioErr;
                }

                /* Open root directory window */
                /* TODO: Create folder window for root directory */
                /* FINDER_LOG_DEBUG("Opening volume: %s (root ID=%d)\n", vcb.name, vcb.rootID); */

                return noErr;
            }
        }
    }

    return fnfErr;
}

/*
 * StartDragIcon - Begin dragging an icon
 */
void StartDragIcon(Point mousePt)
{

    if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
        if (gDesktopIcons[gSelectedIcon].movable) {
            gDraggingIcon = true;
            FINDER_LOG_DEBUG("Started dragging icon %d\n", gSelectedIcon);
        } else {
            FINDER_LOG_DEBUG("Cannot drag non-movable icon %d\n", gSelectedIcon);
        }
    }
}

/*
 * DragIcon - Update icon position during drag
 */
void DragIcon(Point mousePt)
{
    if (!gDraggingIcon || gSelectedIcon < 0 || gSelectedIcon >= gDesktopIconCount) {
        return;
    }

    /* Calculate new position based on mouse and offset */
    Point newPos;
    newPos.h = mousePt.h - gDragOffset.h;
    newPos.v = mousePt.v - gDragOffset.v;

    /* Constrain to screen bounds, clamping to SInt16 range */
    short max_h = (fb_width > 42 && fb_width - 42 <= 32767) ? (short)(fb_width - 42) : 32767;
    short max_v = (fb_height > 60 && fb_height - 60 <= 32767) ? (short)(fb_height - 60) : 32767;
    if (newPos.h < 10) newPos.h = 10;
    if (newPos.v < 30) newPos.v = 30;  /* Below menu bar */
    if (newPos.h > max_h) newPos.h = max_h;
    if (newPos.v > max_v) newPos.v = max_v;

    /* Update icon position */
    gDesktopIcons[gSelectedIcon].position = newPos;

    /* Redraw desktop to show new position */
    DrawDesktop();
    DrawVolumeIcon();
}

/*
 * EndDragIcon - Finish dragging an icon
 */
void EndDragIcon(Point mousePt)
{

    if (!gDraggingIcon) {
        return;
    }

    gDraggingIcon = false;

    if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
        FINDER_LOG_DEBUG("Finished dragging icon %d to (%d,%d)\n",
                     gSelectedIcon,
                     gDesktopIcons[gSelectedIcon].position.h,
                     gDesktopIcons[gSelectedIcon].position.v);

        /* Save the new position */
        SaveDesktopDatabase(0);
    }
}

/*
 * HandleDesktopDrag - Handle mouse drag on desktop
 * Returns true if dragging an icon
 */
Boolean HandleDesktopDrag(Point mousePt, Boolean buttonDown)
{
    if (gDraggingIcon && buttonDown) {
        /* Continue dragging */
        DragIcon(mousePt);
        return true;
    } else if (gDraggingIcon && !buttonDown) {
        /* End drag */
        EndDragIcon(mousePt);
        return true;
    }

    return false;
}

/*
 * SelectNextDesktopIcon - Cycle through desktop icons with Tab key
 */
void SelectNextDesktopIcon(void)
{

    FINDER_LOG_DEBUG("SelectNextDesktopIcon: current=%d, count=%d\n",
                  gSelectedIcon, gDesktopIconCount);

    if (gDesktopIconCount == 0) {
        return;
    }

    /* Advance to next icon, wrapping around */
    short prevSelected = gSelectedIcon;
    gSelectedIcon = (gSelectedIcon + 1) % gDesktopIconCount;

    FINDER_LOG_DEBUG("SelectNextDesktopIcon: selected %d → %d, posting updateEvt\n",
                 prevSelected, gSelectedIcon);

    /* Post updateEvt to defer redraw to event loop (avoids re-entrancy freeze) */
    PostEvent(updateEvt, 0);  /* NULL window = desktop/background */
}

/*
 * OpenSelectedDesktopIcon - Open window for currently selected desktop icon
 */
void OpenSelectedDesktopIcon(void)
{

    FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: selected=%d, count=%d\n",
                  gSelectedIcon, gDesktopIconCount);

    if (gSelectedIcon < 0 || gSelectedIcon >= gDesktopIconCount) {
        FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: No icon selected\n");
        return;
    }

    /* Check if it's the volume icon */
    if (gDesktopIcons[gSelectedIcon].type == kDesktopItemVolume) {
        FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Opening volume window\n");

        /* Create a folder window for the volume */
        Rect windowBounds;
        SetRect(&windowBounds, 100, 60, 500, 360);

        WindowPtr volumeWindow = NewWindow(NULL, &windowBounds,
                                          "\014Macintosh HD",
                                          true,  /* visible */
                                          0,     /* documentProc */
                                          (WindowPtr)-1L,  /* frontmost */
                                          true,  /* goAway box */
                                          'DISK');  /* refCon to identify as disk window */

        if (volumeWindow) {
            ShowWindow(volumeWindow);
            SelectWindow(volumeWindow);

            /* Force update event */
            InvalRect(&volumeWindow->port.portRect);

            FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Volume window created successfully\n");
        } else {
            FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Failed to create window\n");
        }
    } else if (gDesktopIcons[gSelectedIcon].type == kDesktopItemTrash) {
        FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Opening trash window\n");

        /* Create a trash window */
        Rect windowBounds;
        SetRect(&windowBounds, 200, 120, 600, 420);

        WindowPtr trashWindow = (WindowPtr)NewWindow(NULL, &windowBounds,
                                         "\005Trash",
                                         true,  /* visible */
                                         0,     /* documentProc */
                                         (WindowPtr)-1L,  /* frontmost */
                                         true,  /* goAway box */
                                         'TRSH');  /* refCon to identify as trash window */

        if (trashWindow) {
            ShowWindow(trashWindow);
            SelectWindow(trashWindow);

            /* Force update event */
            InvalRect(&trashWindow->port.portRect);

            FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Trash window created successfully\n");
        } else {
            FINDER_LOG_DEBUG("OpenSelectedDesktopIcon: Failed to create trash window\n");
        }
    }
}
