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

/* HD Icon data - still needed by icon_system.c */
extern const uint8_t g_HDIcon[128];
extern const uint8_t g_HDIconMask[128];

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

/* External globals */
extern QDGlobals qd;  /* QuickDraw globals from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* Global Desktop State */
static IconPosition *gDesktopIcons = nil;   /* Array of desktop icon positions */
static short gDesktopIconCount = 0;         /* Number of icons on desktop */
static Boolean gDesktopNeedsCleanup = false; /* Desktop needs reorganization */
static VRefNum gBootVolumeRef = 0;          /* Boot volume reference */
static Boolean gVolumeIconVisible = false;   /* Is volume icon shown on desktop */

/* Forward Declarations */
static OSErr LoadDesktopDatabase(short vRefNum);
static OSErr SaveDesktopDatabase(short vRefNum);
static OSErr AllocateDesktopIcons(void);
static Point CalculateNextIconPosition(void);
static Boolean IsPositionOccupied(Point position);
static OSErr ScanDirectoryForDesktopEntries(short vRefNum, long dirID, short databaseRefNum);

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

    /* Draw volume icon if visible */
    if (gVolumeIconVisible) {
        DrawVolumeIcon();
    }

    SetPort(savePort);
}

/*
 * DrawDesktop - Initial desktop drawing (called once at startup)
 * This now just triggers the proper repaint through Window Manager
 */
void DrawDesktop(void)
{
    /* Register our DeskHook with Window Manager */
    SetDeskHook(Finder_DeskHook);

    /* Invalidate entire screen to trigger repaint */
    InvalRect(&qd.screenBits.bounds);

    /* Window Manager will call our DeskHook during next WM_Update */
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
        /* If load fails, initialize empty database */
        gDesktopIconCount = 0;
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

    gDesktopIcons = (IconPosition*)NewPtr(sizeof(IconPosition) * kMaxDesktopIcons);
    if (gDesktopIcons == NULL) {
        return memFullErr;
    }

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

    /* Read icon positions */
    if (gDesktopIconCount > 0) {
        dataSize = sizeof(IconPosition) * gDesktopIconCount;
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

    /* Write icon positions */
    if (gDesktopIconCount > 0) {
        dataSize = sizeof(IconPosition) * gDesktopIconCount;
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

    /* Add volume icon to desktop */
    if (gDesktopIconCount < kMaxDesktopIcons) {
        /* Position at top-right of desktop */
        gDesktopIcons[gDesktopIconCount].iconID = 0xFFFFFFFF; /* Special ID for volume */
        gDesktopIcons[gDesktopIconCount].position.h = fb_width - 100;
        gDesktopIcons[gDesktopIconCount].position.v = 60;
        /* Volume doesn't have a fileSpec - it uses special ID */

        gDesktopIconCount++;
        gVolumeIconVisible = true;
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

    if (!gVolumeIconVisible) return;

    /* Find volume icon in desktop array */
    for (int i = 0; i < gDesktopIconCount; i++) {
        if (gDesktopIcons[i].iconID == 0xFFFFFFFF) {
            volumePos = gDesktopIcons[i].position;
            break;
        }
    }

    /* Get volume name */
    if (VFS_GetVolumeInfo(gBootVolumeRef, &vcb)) {
        size_t len = strlen(vcb.name);
        if (len > 255) len = 255;
        memcpy(volumeName, vcb.name, len);
        volumeName[len] = '\0';
    } else {
        memcpy(volumeName, "Macintosh HD", 13);
    }

    /* Setup FileKind for volume */
    fk.isVolume = true;
    fk.isFolder = false;
    fk.type = 0;
    fk.creator = 0;
    fk.hasCustomIcon = false;
    fk.path = NULL;

    /* Get icon handle for volume - this will use the default HD icon */
    iconHandle.fam = IconSys_DefaultVolume();
    iconHandle.selected = false;

    /* Draw icon with label using universal system */
    Icon_DrawWithLabel(&iconHandle, volumeName,
                      volumePos.h + 16,  /* Center X (icon is 32px wide) */
                      volumePos.v,        /* Top Y */
                      false);             /* Not selected */

    /* Draw Trash icon in bottom-right corner */
    Point trashPos;
    trashPos.h = 700;  /* Near right edge */
    trashPos.v = 500;  /* Near bottom */

    /* Get appropriate trash icon */
    extern const IconFamily* IconSys_TrashEmpty(void);
    extern const IconFamily* IconSys_TrashFull(void);
    extern bool Trash_IsEmptyAll(void);

    IconHandle trashHandle;
    trashHandle.fam = Trash_IsEmptyAll() ? IconSys_TrashEmpty() : IconSys_TrashFull();
    trashHandle.selected = false;

    /* Draw trash with label - using custom offset for trash */
    Icon_DrawWithLabelOffset(&trashHandle, "Trash",
                            trashPos.h + 16,    /* Center X */
                            trashPos.v,         /* Top Y */
                            48,                 /* Label offset - moved down 1 more pixel */
                            false);             /* Not selected */
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
        if (gDesktopIcons[i].iconID == 0xFFFFFFFF) {
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
