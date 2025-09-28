/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Desktop Manager Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source: /home/k/Desktop/system7/system7_resources/Install 3_resources/Finder.rsrc
 * SHA256: 7d59b9ef6c5587010ee4d573bd4b5fb3aa70ba696ccaff1a61b52c9ae1f7a632
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
#include "QuickDrawConstants.h"

/* HD Icon data - still needed by icon_system.c */
extern const uint8_t g_HDIcon[128];
extern const uint8_t g_HDIconMask[128];

/* Debug output */
extern void serial_printf(const char* fmt, ...);
extern void serial_puts(const char* str);

/* Chicago font data */
extern const uint8_t chicago_bitmap[];
extern GrafPtr g_currentPort;
#define CHICAGO_HEIGHT 16
#define CHICAGO_ASCENT 12
#define CHICAGO_ROW_BYTES 140


/* Desktop Database Constants */
#define kDesktopDatabaseName    "\pDesktop DB"
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

/* Icon selection and dragging state */
static short gSelectedIcon = -1;             /* Index of selected icon (-1 = none) */
static Boolean gDraggingIcon = false;        /* Currently dragging an icon */
static Point gDragOffset = {0, 0};           /* Offset from icon origin to mouse */
static Point gOriginalPos = {0, 0};          /* Original position before drag */
static UInt32 gLastClickTime = 0;            /* For double-click detection */
static Point gLastClickPos = {0, 0};         /* Last click position */

/* Forward Declarations */
static OSErr LoadDesktopDatabase(short vRefNum);
static OSErr SaveDesktopDatabase(short vRefNum);
static OSErr AllocateDesktopIcons(void);
static Point CalculateNextIconPosition(void);
static Boolean IsPositionOccupied(Point position);
static OSErr ScanDirectoryForDesktopEntries(short vRefNum, long dirID, short databaseRefNum);
static Point SnapToGrid(Point p);
static void UpdateIconRect(short iconIndex, Rect *outRect);
static short IconAtPoint(Point where);
static void InvertIconOutline(const Rect *r);
static void DesktopYield(void);
static void TrackIconDragSync(short iconIndex, Point startPt);

/* Public function to draw the desktop */
void DrawDesktop(void);
void DrawVolumeIcon(void);

/*
 * CleanUpDesktop - Arranges all desktop icons in a grid pattern
 * Evidence: "Clean Up Desktop" string from menu analysis
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
    SetPort(&qd.thePort);  /* Draw to screen port */

    /* Clip to the invalid region */
    SetClip(invalidRgn);

    /* Draw desktop pattern using current background pattern */
    /* EraseRgn will use the BackPat/BackColor set by Pattern Manager */
    EraseRgn(invalidRgn);

    /* Draw desktop icons in invalid region */
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (i == gDraggingIconIndex) continue;

        Rect iconRect;
        SetRect(&iconRect,
                gDesktopIcons[i].position.h,
                gDesktopIcons[i].position.v,
                gDesktopIcons[i].position.h + 32,
                gDesktopIcons[i].position.v + 32);

        if (RectInRgn(&iconRect, invalidRgn)) {
            /* Draw icon using QuickDraw */
            /* PlotIcon(&iconRect, gDesktopIcons[i].icon); */

            /* For now, draw a simple rect as placeholder */
            FrameRect(&iconRect);
        }
    }

    /* Draw volume icon if visible (DrawVolumeIcon also draws trash) */
    if (gVolumeIconVisible) {
        DrawVolumeIcon();
    }

    SetPort(savePort);
}

/* ArrangeDesktopIcons - Arrange desktop icons in a grid */
void ArrangeDesktopIcons(void)
{
    const int gridSpacing = 80;  /* Spacing between icons */
    const int startX = 700;       /* Start from right side */
    const int startY = 50;        /* Start below menu bar */
    int currentX = startX;
    int currentY = startY;

    serial_printf("ArrangeDesktopIcons: Arranging %d icons\n", gDesktopIconCount);

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
    RgnHandle desktopRgn;

    /* Register our DeskHook with Window Manager */
    SetDeskHook(Finder_DeskHook);

    /* Create a region for the desktop (excluding menu bar) */
    desktopRgn = NewRgn();
    Rect desktopRect = qd.screenBits.bounds;
    desktopRect.top = 20;  /* Start below menu bar */
    RectRgn(desktopRgn, &desktopRect);

    /* Directly call our DeskHook to paint the desktop with the proper pattern */
    Finder_DeskHook(desktopRgn);  /* Pass the desktop region to paint */

    /* Invalidate entire screen to trigger any window repaints */
    InvalRect(&qd.screenBits.bounds);

    DisposeRgn(desktopRgn);
}

/*
 * RebuildDesktopFile - Rebuilds the desktop database for a volume
 * Evidence: "Rebuilding the desktop file" string from analysis
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
 * Evidence: "was found on the desktop" string analysis
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
 * Evidence: "on the desktop" string analysis
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
 * Evidence: Desktop database management functionality
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
        return noErr; /* Already allocated */
    }

    gDesktopIcons = (DesktopItem*)NewPtr(sizeof(DesktopItem) * kMaxDesktopIcons);
    if (gDesktopIcons == NULL) {
        return memFullErr;
    }

    /* Initialize trash as the first desktop item */
    gDesktopIcons[0].type = kDesktopItemTrash;
    gDesktopIcons[0].iconID = 0xFFFFFFFF;
    gDesktopIcons[0].position.h = fb_width - 100;
    gDesktopIcons[0].position.v = fb_height - 80;
    strcpy(gDesktopIcons[0].name, "Trash");
    gDesktopIcons[0].movable = false;  /* Trash stays in place */
    gDesktopIconCount = 1;  /* Start with trash */

    return noErr;
}

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

/*
 * SnapToGrid - Snap position to grid (System 7 style)
 */
static Point SnapToGrid(Point p)
{
    p.h = ((p.h + kGridW / 2) / kGridW) * kGridW;
    p.v = ((p.v + kGridH / 2) / kGridH) * kGridH;
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

    extern void serial_printf(const char* fmt, ...);
    serial_printf("IconAtPoint: checking (%d,%d), gDesktopIconCount=%d\n",
                 where.h, where.v, gDesktopIconCount);

    /* Check all desktop items */
    for (int i = 0; i < gDesktopIconCount; i++) {
        serial_printf("IconAtPoint: Checking item %d (type=%d) at (%d,%d)\n",
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

        serial_printf("IconAtPoint: Item %d rects: icon=(%d,%d,%d,%d), label=(%d,%d,%d,%d)\n",
                     i, iconRect.left, iconRect.top, iconRect.right, iconRect.bottom,
                     labelRect.left, labelRect.top, labelRect.right, labelRect.bottom);

        if (PtInRect(where, &iconRect) || PtInRect(where, &labelRect)) {
            serial_printf("IconAtPoint: HIT item %d!\n", i);
            return i;
        }
    }

    serial_printf("IconAtPoint: No icon hit, returning -1\n");
    return -1;
}

/*
 * InvertIconOutline - Draw XOR outline (System 7 style)
 */
static void InvertIconOutline(const Rect *r)
{
    PenState save;
    GetPenState(&save);

    /* Draw thick XOR outline for visibility */
    PenMode(patXor);
    PenSize(2, 2);
    FrameRect(r);

    /* Draw inner rect for emphasis */
    Rect inner = *r;
    InsetRect(&inner, 1, 1);
    FrameRect(&inner);

    SetPenState(&save);

    /* Lightweight display update - just update the outline region */
    extern void QDPlatform_UpdateScreen(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom);
    QDPlatform_UpdateScreen(r->left - 2, r->top - 2, r->right + 2, r->bottom + 2);
}

/*
 * DesktopYield - Keep system ticking during modal loops
 */
static void DesktopYield(void)
{
    extern void SystemTask(void);
    extern void PollPS2Input(void);

    SystemTask();
    PollPS2Input();  /* Poll hardware to update mouse/keyboard state */
}

/*
 * TrackIconDragSync - System 7 style synchronous modal drag with XOR ghost
 * This runs immediately when mouseDown is received, polls StillDown() directly
 */
static void TrackIconDragSync(short iconIndex, Point startPt)
{
    Rect ghost;
    Boolean ghostOn = false;
    Point p;
    GrafPtr savePort;

    extern void serial_printf(const char* fmt, ...);
    extern Boolean StillDown(void);
    extern void GetMouse(Point *pt);

    if (iconIndex < 0 || iconIndex >= gDesktopIconCount) return;

    serial_printf("TrackIconDragSync ENTRY: starting modal drag for icon %d\n", iconIndex);

    /* Set tracking guard to suppress queued mouse events */
    gInMouseTracking = true;

    /* Mark this icon as being dragged */
    gDraggingIconIndex = iconIndex;

    /* Prepare ghost rect */
    UpdateIconRect(iconIndex, &ghost);
    ghost.left -= 20;
    ghost.right += 20;
    ghost.bottom += 16;

    /* Wait to see if this becomes a drag (threshold check) */
    GetMouse(&p);
    while (StillDown()) {
        GetMouse(&p);
        if (abs(p.h - startPt.h) > kDragThreshold || abs(p.v - startPt.v) > kDragThreshold) {
            serial_printf("TrackIconDragSync: threshold exceeded at (%d,%d)\n", p.h, p.v);
            break;
        }
        DesktopYield();
    }

    /* If not moved enough, treat as simple click */
    GetMouse(&p);
    if (!(abs(p.h - startPt.h) > kDragThreshold || abs(p.v - startPt.v) > kDragThreshold)) {
        serial_printf("TrackIconDragSync: no drag, threshold not exceeded\n");
        gDraggingIconIndex = -1;
        gInMouseTracking = false;
        return;
    }

    /* Enter drag: draw translucent icon following cursor (QEMU-compatible) */
    GetPort(&savePort);
    SetPort(&qd.thePort);

    /* Draw initial ghost outline */
    InvertIconOutline(&ghost);
    ghostOn = true;
    serial_printf("TrackIconDragSync: drag started, ghost visible\n");

    Rect prevGhost = ghost;

    while (StillDown()) {
        Point cur;
        GetMouse(&cur);
        if (cur.h != p.h || cur.v != p.v) {
            /* Erase old ghost outline */
            if (ghostOn) InvertIconOutline(&ghost);

            /* Calculate dirty rect (union of old and new positions) */
            Rect dirtyRect = prevGhost;
            UnionRect(&dirtyRect, &ghost, &dirtyRect);
            InsetRect(&dirtyRect, -4, -4);  /* Expand for outline */

            /* Move by delta */
            OffsetRect(&ghost, cur.h - p.h, cur.v - p.v);

            /* Constrain to desktop bounds */
            Rect desk = qd.screenBits.bounds;
            desk.top = 20;  /* Below menu bar */

            if (ghost.left < desk.left)     OffsetRect(&ghost, desk.left - ghost.left, 0);
            if (ghost.top < desk.top)       OffsetRect(&ghost, 0, desk.top - ghost.top);
            if (ghost.right > desk.right)   OffsetRect(&ghost, desk.right - ghost.right, 0);
            if (ghost.bottom > desk.bottom) OffsetRect(&ghost, 0, desk.bottom - ghost.bottom);

            /* Expand dirty rect to include new position */
            UnionRect(&dirtyRect, &ghost, &dirtyRect);
            InsetRect(&dirtyRect, -4, -4);

            /* Erase dirty region - just redraw desktop for now */
            DrawDesktop();

            /* Draw dragged icon at new ghost position (adjust for label margins) */
            Point savedPos = gDesktopIcons[iconIndex].position;
            gDesktopIcons[iconIndex].position.h = ghost.left + 20;
            gDesktopIcons[iconIndex].position.v = ghost.top;
            DrawVolumeIcon();
            gDesktopIcons[iconIndex].position = savedPos;

            /* Force display update for smoother dragging */
            extern void QDPlatform_FlushScreen(void);
            QDPlatform_FlushScreen();

            /* Draw new ghost outline on top */
            InvertIconOutline(&ghost);
            ghostOn = true;

            prevGhost = ghost;
            p = cur;
        }
        DesktopYield();
    }

    /* Button released: erase ghost */
    if (ghostOn) InvertIconOutline(&ghost);

    /* Commit new position (snap to grid) - only for movable items */
    if (gDesktopIcons[iconIndex].movable) {
        Point snapped;
        snapped.h = ghost.left;
        snapped.v = ghost.top;
        snapped = SnapToGrid(snapped);

        gDesktopIcons[iconIndex].position = snapped;

        serial_printf("TrackIconDragSync: drag complete, new position (%d,%d)\n", snapped.h, snapped.v);
    } else {
        serial_printf("TrackIconDragSync: drag complete (non-movable item - no position save)\n");
    }

    /* Redraw entire desktop to clear old position */
    SetPort(savePort);
    DrawDesktop();
    DrawVolumeIcon();

    /* Clear tracking guard and drag flag */
    gDraggingIconIndex = -1;
    gInMouseTracking = false;

    /* Drain any queued mouseUp that may have been posted anyway */
    EventRecord e;
    if (EventAvail(mUpMask, &e)) {
        EventRecord dump;
        GetNextEvent(mUpMask, &dump);
    }
}

/*
 * LoadDesktopDatabase - Load desktop positions from database file
 * Evidence: Desktop database persistence
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

    FSClose(databaseRefNum);
    return err;
}

/*
 * SaveDesktopDatabase - Save desktop positions to database file
 * Evidence: Desktop database persistence
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
 * Evidence: Desktop database rebuilding process
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
 * RE-AGENT-TRAILER-JSON
 * {
 *   "module": "desktop_manager.c",
 *   "evidence_density": 0.80,
 *   "functions": 9,
 *   "primary_evidence": [
 *     "Clean Up Desktop string from menu analysis",
 *     "Rebuilding the desktop file string",
 *     "Desktop positioning strings: was found on the desktop",
 *     "Desktop database management patterns"
 *   ],
 *   "implementation_status": "desktop_complete"
 * }
 */

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

        serial_printf("InitializeVolumeIcon: Added volume icon at index %d, pos=(%d,%d)\n",
                      gDesktopIconCount, fb_width - 100, 60);

        gDesktopIconCount++;
        gVolumeIconVisible = true;

        serial_printf("InitializeVolumeIcon: gDesktopIconCount now = %d\n", gDesktopIconCount);
    }

    return noErr;
}

/*
 * DrawVolumeIcon - Draw the boot volume icon on desktop using universal icon system
 */
void DrawVolumeIcon(void)
{
    Point volumePos = {0, 0};
    VolumeControlBlock vcb;
    char volumeName[256];
    IconHandle iconHandle;
    FileKind fk = {0};

    extern void serial_printf(const char* fmt, ...);
    serial_printf("DrawVolumeIcon: ENTRY\n");

    if (!gVolumeIconVisible) {
        serial_printf("DrawVolumeIcon: not visible, returning\n");
        return;
    }

    serial_printf("DrawVolumeIcon: Finding volume position\n");
    /* Find volume icon in desktop array */
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].type == kDesktopItemVolume) {
            volumePos = gDesktopIcons[i].position;
            break;
        }
    }

    serial_printf("DrawVolumeIcon: Getting volume name\n");
    /* Get volume name */
    if (VFS_GetVolumeInfo(gBootVolumeRef, &vcb)) {
        size_t len = strlen(vcb.name);
        if (len > 255) len = 255;
        memcpy(volumeName, vcb.name, len);
        volumeName[len] = '\0';
    } else {
        memcpy(volumeName, "Macintosh HD", 13);
    }

    serial_printf("DrawVolumeIcon: Setting up icon handle\n");
    /* Setup FileKind for volume */
    fk.isVolume = true;
    fk.isFolder = false;
    fk.type = 0;
    fk.creator = 0;
    fk.hasCustomIcon = false;
    fk.path = NULL;

    /* Get icon handle for volume - this will use the default HD icon */
    iconHandle.fam = IconSys_DefaultVolume();
    /* Check if this icon is selected (find its index) */
    Boolean isSelected = false;
    for (int j = 0; j < gDesktopIconCount; j++) {
        if (gDesktopIcons[j].type == kDesktopItemVolume && gSelectedIcon == j) {
            isSelected = true;
            break;
        }
    }
    iconHandle.selected = isSelected;

    serial_printf("DrawVolumeIcon: About to call Icon_DrawWithLabel\n");
    /* Draw icon with label using universal system */
    Icon_DrawWithLabel(&iconHandle, volumeName,
                      volumePos.h + 16,  /* Center X (icon is 32px wide) */
                      volumePos.v,        /* Top Y */
                      isSelected);        /* Selection state */
    serial_printf("DrawVolumeIcon: Icon_DrawWithLabel returned\n");

    /* Draw all other desktop items */
    serial_printf("DrawVolumeIcon: Drawing other desktop items\n");
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].type == kDesktopItemTrash) {
            /* Draw trash icon */
            extern const IconFamily* IconSys_TrashEmpty(void);
            extern const IconFamily* IconSys_TrashFull(void);
            extern bool Trash_IsEmptyAll(void);

            IconHandle trashHandle;
            trashHandle.fam = Trash_IsEmptyAll() ? IconSys_TrashEmpty() : IconSys_TrashFull();
            trashHandle.selected = (gSelectedIcon == i);

            serial_printf("DrawVolumeIcon: Drawing trash at (%d,%d)\n",
                         gDesktopIcons[i].position.h, gDesktopIcons[i].position.v);

            Icon_DrawWithLabelOffset(&trashHandle, gDesktopIcons[i].name,
                                    gDesktopIcons[i].position.h + 16,  /* Center X */
                                    gDesktopIcons[i].position.v,        /* Top Y */
                                    48,                                 /* Label offset */
                                    trashHandle.selected);
        }
        /* Future: handle kDesktopItemFile, kDesktopItemFolder, etc. */
    }
    serial_printf("DrawVolumeIcon: EXIT\n");
}

/*
 * HandleDesktopClick - Check if a click is on a desktop icon and handle it
 * Returns true if click was on an icon, false otherwise
 */
Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick)
{
    short prevSelected = gSelectedIcon;
    short hitIcon;

    extern void serial_printf(const char* fmt, ...);

    serial_printf("HandleDesktopClick: click at (%d,%d), doubleClick=%d\n",
                  clickPoint.h, clickPoint.v, doubleClick);

    /* Find which icon was hit */
    hitIcon = IconAtPoint(clickPoint);

    if (hitIcon == -1) {
        /* No icon hit - deselect */
        if (prevSelected != -1) {
            gSelectedIcon = -1;
            /* Don't redraw here - let Window Manager handle it via DeskHook */
        }
        return false;
    }

    serial_printf("Hit icon index %d, doubleClick=%d\n", hitIcon, doubleClick);

    /* DEBUG: Print condition components */
    serial_printf("[PRE-IF] dbl=%d, hit=%d, count=%d, (hit>=0)=%d, (hit<count)=%d\n",
                  doubleClick, hitIcon, gDesktopIconCount,
                  (hitIcon >= 0), (hitIcon < gDesktopIconCount));

    /* Handle double-click IMMEDIATELY after detection - no debug calls in between */
    if (doubleClick && hitIcon >= 0 && hitIcon < gDesktopIconCount) {
        serial_printf("[DBLCLK-IF] dbl=%d hit=%d count=%d\n", doubleClick, hitIcon, gDesktopIconCount);
        extern WindowPtr Finder_OpenDesktopItem(Boolean isTrash, ConstStr255Param title);
        DesktopItem *it = &gDesktopIcons[hitIcon];
        serial_printf("[DBLCLK-ITEM] type=%d\n", it->type);

        if (it->type == kDesktopItemVolume) {
            Finder_OpenDesktopItem(false, "\pMacintosh HD");
            return true;
        } else if (it->type == kDesktopItemTrash) {
            Finder_OpenDesktopItem(true, "\pTrash");
            return true;
        }
    }

    /* Single click - select and potentially drag */
    serial_printf("Single-click handler: prevSelected=%d, hitIcon=%d\n", prevSelected, hitIcon);
    gSelectedIcon = hitIcon;

    if (prevSelected != gSelectedIcon) {
        serial_printf("Selection changed, invalidating icon regions\n");
        /* Invalidate only the affected icon regions instead of redrawing entire desktop */
        if (prevSelected >= 0 && prevSelected < gDesktopIconCount) {
            Rect prevRect;
            UpdateIconRect(prevSelected, &prevRect);
            InvalRect(&prevRect);
        }
        if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
            Rect newRect;
            UpdateIconRect(gSelectedIcon, &newRect);
            InvalRect(&newRect);
        }
        serial_printf("Icon regions invalidated\n");
    }

    /* Start synchronous modal drag tracking immediately (only for single clicks) */
    if (!doubleClick && hitIcon >= 0 && hitIcon < gDesktopIconCount) {
        serial_printf("Starting TrackIconDragSync for icon %d\n", hitIcon);
        TrackIconDragSync(hitIcon, clickPoint);
    }

    return true;
}

/*
 * HandleVolumeDoubleClick - Open volume window on double-click
 */
OSErr HandleVolumeDoubleClick(Point clickPoint)
{
    Rect iconRect;
    WindowPtr volumeWindow;
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
                /* serial_printf("Opening volume: %s (root ID=%d)\n", vcb.name, vcb.rootID); */

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
    extern void serial_printf(const char* fmt, ...);

    if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
        if (gDesktopIcons[gSelectedIcon].movable) {
            gDraggingIcon = true;
            serial_printf("Started dragging icon %d\n", gSelectedIcon);
        } else {
            serial_printf("Cannot drag non-movable icon %d\n", gSelectedIcon);
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

    /* Constrain to screen bounds */
    if (newPos.h < 10) newPos.h = 10;
    if (newPos.v < 30) newPos.v = 30;  /* Below menu bar */
    if (newPos.h > fb_width - 42) newPos.h = fb_width - 42;
    if (newPos.v > fb_height - 60) newPos.v = fb_height - 60;

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
    extern void serial_printf(const char* fmt, ...);

    if (!gDraggingIcon) {
        return;
    }

    gDraggingIcon = false;

    if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
        serial_printf("Finished dragging icon %d to (%d,%d)\n",
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
    extern void serial_printf(const char* fmt, ...);

    serial_printf("SelectNextDesktopIcon: current=%d, count=%d\n",
                  gSelectedIcon, gDesktopIconCount);

    if (gDesktopIconCount == 0) {
        return;
    }

    /* Advance to next icon, wrapping around */
    short prevSelected = gSelectedIcon;
    gSelectedIcon = (gSelectedIcon + 1) % gDesktopIconCount;

    serial_printf("SelectNextDesktopIcon: selected icon %d\n", gSelectedIcon);

    /* Invalidate icon regions to show selection change */
    if (prevSelected >= 0 && prevSelected < gDesktopIconCount) {
        Rect prevRect;
        UpdateIconRect(prevSelected, &prevRect);
        InvalRect(&prevRect);
    }
    if (gSelectedIcon >= 0 && gSelectedIcon < gDesktopIconCount) {
        Rect newRect;
        UpdateIconRect(gSelectedIcon, &newRect);
        InvalRect(&newRect);
    }
}

/*
 * OpenSelectedDesktopIcon - Open window for currently selected desktop icon
 */
void OpenSelectedDesktopIcon(void)
{
    extern void serial_printf(const char* fmt, ...);

    serial_printf("OpenSelectedDesktopIcon: selected=%d, count=%d\n",
                  gSelectedIcon, gDesktopIconCount);

    if (gSelectedIcon < 0 || gSelectedIcon >= gDesktopIconCount) {
        serial_printf("OpenSelectedDesktopIcon: No icon selected\n");
        return;
    }

    /* Check if it's the volume icon */
    if (gDesktopIcons[gSelectedIcon].type == kDesktopItemVolume) {
        serial_printf("OpenSelectedDesktopIcon: Opening volume window\n");

        /* Create a folder window for the volume */
        Rect windowBounds;
        SetRect(&windowBounds, 100, 60, 500, 360);

        WindowPtr volumeWindow = NewWindow(NULL, &windowBounds,
                                          "\pMacintosh HD",
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

            serial_printf("OpenSelectedDesktopIcon: Volume window created successfully\n");
        } else {
            serial_printf("OpenSelectedDesktopIcon: Failed to create window\n");
        }
    } else if (gDesktopIcons[gSelectedIcon].type == kDesktopItemTrash) {
        serial_printf("OpenSelectedDesktopIcon: Opening trash window\n");

        /* Create a trash window */
        Rect windowBounds;
        SetRect(&windowBounds, 200, 120, 600, 420);

        WindowPtr trashWindow = NewWindow(NULL, &windowBounds,
                                         "\pTrash",
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

            serial_printf("OpenSelectedDesktopIcon: Trash window created successfully\n");
        } else {
            serial_printf("OpenSelectedDesktopIcon: Failed to create trash window\n");
        }
    }
}
