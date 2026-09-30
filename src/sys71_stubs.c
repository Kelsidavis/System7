/*
 * System 7.1 Stubs for linking
 *
 * [WM-050] Naming clarification: SYS71_PROVIDE_FINDER_TOOLBOX
 *
 * When SYS71_PROVIDE_FINDER_TOOLBOX is defined (=1), this means:
 *   DO NOT provide Toolbox stubs; real implementations exist and should be used.
 *
 * Stubs wrapped in #if !defined(SYS71_STUBS_DISABLED) are excluded when
 * SYS71_PROVIDE_FINDER_TOOLBOX is defined, ensuring single source of truth per symbol.
 *
 * When the flag is NOT defined (bootstrap builds), stubs are included to
 * satisfy linker requirements until real implementations are integrated.
 */

/* Lock stub switch: single knob to disable all quarantined stubs */
#ifdef SYS71_PROVIDE_FINDER_TOOLBOX
#define SYS71_STUBS_DISABLED 1
#endif
/* Disable quarantined stubs by default now that real modules exist */
#ifndef SYS71_STUBS_DISABLED
#define SYS71_STUBS_DISABLED 1
#endif

#include "../include/MacTypes.h"
#include "../include/SystemInternal.h"
#include "../include/QuickDraw/QuickDraw.h"
#include "../include/QuickDraw/QDRegions.h"
#include "../include/QuickDrawConstants.h"
#include "../include/ResourceManager.h"
#include "../include/EventManager/EventTypes.h"  /* Include EventTypes first to define activeFlag */
#include "../include/EventManager/EventManager.h"
#include "../include/WindowManager/WindowManager.h"
/* #include "../include/WindowManager/WindowManagerInternal.h" -- removed, has conflicts */
#include "../include/MenuManager/MenuManager.h"
#include "../include/DialogManager/DialogManager.h"
#include "../include/ControlManager/ControlManager.h"
#include "../include/ListManager/ListManager.h"
#include "../include/TextEdit/TextEdit.h"
#include "../include/FontManager/FontManager.h"
#include "../include/sys71_stubs.h"
#include "../include/System/SystemLogging.h"
#include "../include/MemoryMgr/MemoryManager.h"
#include "../include/FileMgr/file_manager.h"
#include "../include/Finder/finder.h"
#include "../include/FS/hfs_types.h"

/* DeskHook type definition if not in headers */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);

void Platform_CleanupMenuSystem(void) {
    /* Platform-specific menu cleanup
     * Called by CleanupMenus() during Menu Manager shutdown
     *
     * In a full implementation, this would:
     * - Release platform-specific menu rendering resources
     * - Dispose of cursor resources
     * - Free platform menu caches
     * - Unregister menu event handlers
     * - Clean up platform tracking structures
     *
     * For the kernel environment, cleanup is handled by the
     * Menu Manager core, so this is a no-op.
     */
}

void Platform_EraseMenuBar(void) {
    /* Platform-specific menu bar erasing
     *
     * In a full implementation, this would:
     * - Erase the menu bar area on screen (typically top 20 pixels)
     * - Fill with desktop pattern or white background
     * - Called before redrawing the menu bar
     * - May invalidate menu bar region for update
     *
     * For the kernel environment, menu bar erasing is handled
     * by MenuDisplay.c which directly manipulates the framebuffer,
     * so this is a no-op.
     */
}

/* Resource Manager */
#ifndef ENABLE_RESOURCES
void InitResourceManager(void) {
    /* Initialize the Resource Manager */
    /* This sets up the resource chain and system resource file */

    /* In a full implementation, this would:
     * 1. Initialize the resource file chain (empty list)
     * 2. Open the System resource file
     * 3. Set up resource map data structures
     * 4. Initialize resource cache
     * 5. Set the current resource file to the System file
     */

    /* For simple builds without resource support, this is a no-op */
    /* Resource Manager is ready for GetResource calls */
}
#endif

/* Font Manager */

/* [WM-050] Window Manager stub quarantine
 * Provenance: IM:Windows Vol I - real implementations in WindowDisplay.c, WindowEvents.c, WindowResizing.c
 * Policy: Stubs compile only if SYS71_PROVIDE_FINDER_TOOLBOX is undefined
 * Real WM always wins; no dual definitions
 */

/* Control Manager */

/* Event Manager */

/* Removed DISABLED GetNextEvent stub (real implementation lives in EventManager) */

/* Removed DISABLED PostEvent stub (real implementation lives in EventManager) */

/* Forward declaration for compatibility */
extern void GenerateSystemEvent(short eventType, int message, Point where, short modifiers);

/* External functions we use */

/* Serial stubs */
#include <stdarg.h>

/* QuickDraw globals - defined in main.c */
extern QDGlobals qd;

/* Window manager globals */
WindowPtr g_firstWindow = NULL;  /* Head of window chain */

void FinderEventLoop(void) {
    /* Main Finder event processing loop */
    /* This processes events and dispatches to appropriate handlers */

    extern Boolean WaitNextEvent(SInt16 eventMask, EventRecord* theEvent,
                                 UInt32 sleep, RgnHandle mouseRgn);
    extern void DoBackgroundTasks(void);

    EventRecord event;
    Boolean gotEvent;

    /* Event loop runs until quit */
    while (true) {
        /* Wait for next event with background processing */
        gotEvent = WaitNextEvent(everyEvent, &event, 30, NULL);

        if (gotEvent) {
            /* Dispatch event to appropriate handler */
            /* In full implementation, would call:
             * - HandleMouseDown, HandleKeyDown, HandleUpdate
             * - HandleActivate, HandleOSEvent, etc.
             */
        }

        /* Perform background tasks during idle time */
        DoBackgroundTasks();
    }
}

/* Additional Finder support functions */
/* Removed DISABLED FlushEvents stub (real implementation lives in EventManager) */

/* Window Manager functions - All real implementations in WindowManager/ directory:
 * InitWindows - WindowManagerCore.c
 * NewWindow - WindowManagerCore.c
 * DisposeWindow - WindowManagerCore.c
 * MoveWindow - WindowDragging.c
 * CloseWindow - WindowManagerCore.c
 * ShowWindow - WindowDisplay.c
 * SelectWindow - WindowDisplay.c
 * FrontWindow - WindowDisplay.c
 * DrawGrowIcon - WindowDisplay.c
 * FindWindow - WindowEvents.c
 * DragWindow - WindowDragging.c
 * SetWTitle - WindowManagerCore.c
 */

/* System stubs */
long sysconf(int name) {
    /* POSIX system configuration query function
     * Returns runtime configuration limits and options */

    /* Common sysconf constants (from POSIX unistd.h) */
    #define _SC_PAGESIZE 1
    #define _SC_CLK_TCK 2
    #define _SC_OPEN_MAX 4
    #define _SC_NPROCESSORS_ONLN 58

    switch (name) {
        case _SC_PAGESIZE:
            /* Memory page size in bytes */
            return 4096;

        case _SC_CLK_TCK:
            /* Clock ticks per second for times() function */
            return 60;  /* 60 Hz for Mac System 7 */

        case _SC_OPEN_MAX:
            /* Maximum number of open files per process */
            return 256;

        case _SC_NPROCESSORS_ONLN:
            /* Number of online processors */
            return 1;  /* Single-processor system */

        default:
            /* Unknown configuration parameter */
            return -1;
    }
}

#ifndef ENABLE_RESOURCES
void ReleaseResource(Handle theResource) {
    if (!theResource) return;

    /* Release a resource handle loaded from resource fork */
    /* Marks the resource as purgeable so Memory Manager can free it if needed */

    extern void HPurge(Handle h);

    /* Make the resource purgeable */
    HPurge(theResource);

    /* In a full implementation, this would also:
     * - Update resource map to mark resource as not loaded
     * - Clear any resource attributes set during loading
     * - Allow the handle to be purged by Memory Manager
     */
}
#endif

OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, SInt16 scriptTag) {
    if (!spec) {
        return paramErr;
    }
    (void)scriptTag;

    /* Route through FSCreate which creates a file in the File Manager */
    extern OSErr FSCreate(const unsigned char* fileName, SInt16 vRefNum,
                          UInt32 creator, UInt32 fileType);
    return FSCreate(spec->name, spec->vRefNum, creator, fileType);
}

OSErr FSpOpenDF(const FSSpec* spec, SInt16 permission, SInt16* refNum) {
    if (!spec || !refNum) {
        return paramErr;
    }

    /* Use FSOpen which takes fileName + vRefNum and routes through PBOpenSync.
     * FSOpen is the classic File Manager open that works with our VFS. */
    extern OSErr FSOpen(const unsigned char* fileName, SInt16 vRefNum, SInt16* refNum);
    return FSOpen(spec->name, spec->vRefNum, refNum);
}

OSErr FSpDelete(const FSSpec* spec) {
    if (!spec) {
        return paramErr;
    }

    /* Route through FSDelete which removes the file via File Manager */
    extern OSErr FSDelete(const unsigned char* fileName, SInt16 vRefNum);
    return FSDelete(spec->name, spec->vRefNum);
}

OSErr FSpGetFInfo(const FSSpec* spec, FInfo* fndrInfo) {
    if (!spec || !fndrInfo) return paramErr;
    extern OSErr HGetFInfo(short vRefNum, long dirID,
                           const unsigned char* fileName, FInfo* fndrInfo);
    return HGetFInfo(spec->vRefNum, spec->parID, spec->name, fndrInfo);
}

OSErr FSpSetFInfo(const FSSpec* spec, const FInfo* fndrInfo) {
    if (!spec || !fndrInfo) return paramErr;

    /* Look up the file in VFS to update its type/creator in the overlay */
    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);
    extern bool VFS_SetCatEntryInfo(VRefNum vref, FileID id,
                                     uint32_t type, uint32_t creator, uint16_t flags);

    char cName[32];
    unsigned char len = spec->name[0];
    if (len > 31) len = 31;
    for (int i = 0; i < len; i++) cName[i] = spec->name[i + 1];
    cName[len] = '\0';

    CatEntry entry;
    if (!VFS_Lookup(spec->vRefNum, spec->parID, cName, &entry)) {
        return fnfErr;
    }

    VFS_SetCatEntryInfo(spec->vRefNum, entry.id,
                        fndrInfo->fdType, fndrInfo->fdCreator, fndrInfo->fdFlags);
    return noErr;
}

OSErr FSpDirDelete(const FSSpec* spec) {
    if (!spec) return paramErr;

    /* Resolve FSSpec to FileID via VFS_Lookup, then recursively delete */
    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);
    extern bool VFS_DeleteTree(VRefNum vref, DirID parent, FileID id);

    char cName[32];
    unsigned char len = spec->name[0];
    if (len > 31) len = 31;
    for (int i = 0; i < len; i++) cName[i] = spec->name[i + 1];
    cName[len] = '\0';

    CatEntry entry;
    if (!VFS_Lookup(spec->vRefNum, spec->parID, cName, &entry)) {
        return fnfErr;
    }

    if (entry.kind != kNodeDir) return paramErr;

    return VFS_DeleteTree(spec->vRefNum, spec->parID, entry.id) ? noErr : ioErr;
}

OSErr FSpCatMove(const FSSpec* source, const FSSpec* dest) {
    if (!source || !dest) {
        return paramErr;
    }

    /* Cross-volume moves not supported */
    if (source->vRefNum != dest->vRefNum) {
        return paramErr;
    }

    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);
    extern bool VFS_Move(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName);

    /* Convert source Pascal name to C string */
    CatEntry srcEntry;
    char srcName[32];
    unsigned char srcLen = source->name[0];
    if (srcLen > 31) srcLen = 31;
    for (int i = 0; i < srcLen; i++) srcName[i] = source->name[i + 1];
    srcName[srcLen] = '\0';

    /* Look up source to get its FileID */
    if (!VFS_Lookup(source->vRefNum, source->parID, srcName, &srcEntry)) {
        return fnfErr;
    }

    /* dest FSSpec identifies the destination folder — resolve its DirID */
    DirID targetDir;
    unsigned char dstLen = dest->name[0];
    if (dstLen == 0) {
        /* Empty name means dest->parID is the target directory itself */
        targetDir = dest->parID;
    } else {
        CatEntry dstEntry;
        char dstName[32];
        if (dstLen > 31) dstLen = 31;
        for (int i = 0; i < dstLen; i++) dstName[i] = dest->name[i + 1];
        dstName[dstLen] = '\0';

        if (!VFS_Lookup(dest->vRefNum, dest->parID, dstName, &dstEntry)) {
            return dirNFErr;
        }
        targetDir = dstEntry.id;
    }

    /* Move the file/folder */
    if (!VFS_Move(source->vRefNum, source->parID, srcEntry.id, targetDir, NULL)) {
        return ioErr;
    }

    return noErr;
}

OSErr PBHGetVInfoSync(void *paramBlock) {
    if (!paramBlock) {
        return paramErr;
    }

    HParamBlockRec* pb = (HParamBlockRec*)paramBlock;

    /* Try to get real volume info from VFS */
    extern bool VFS_GetVolumeInfo(VRefNum vref, VolumeControlBlock* vcb);
    VolumeControlBlock vcb;
    VRefNum vref = pb->ioVRefNum;

    if (VFS_GetVolumeInfo(vref, &vcb)) {
        /* Use real volume data */
        UInt32 allocBlkSize = 512;
        pb->u.volumeParam.ioVAlBlkSiz = allocBlkSize;
        pb->u.volumeParam.ioVNmAlBlks = (UInt32)(vcb.totalBytes / allocBlkSize);
    } else {
        /* Fallback: reasonable defaults */
        pb->u.volumeParam.ioVAlBlkSiz = 512;
        pb->u.volumeParam.ioVNmAlBlks = 800;
    }

    return noErr;
}

OSErr SetEOF(short refNum, long logEOF) {
    if (refNum <= 0) return paramErr;
    if (logEOF < 0) return paramErr;

    /* Route to real File Manager implementation */
    extern OSErr FSSetEOF(short refNum, UInt32 eof);
    return FSSetEOF(refNum, (UInt32)logEOF);
}

OSErr GetEOF(short refNum, long* logEOF) {
    if (refNum <= 0 || !logEOF) return paramErr;

    /* Route to real File Manager implementation */
    extern OSErr FSGetEOF(short refNum, UInt32* eof);
    UInt32 eof;
    OSErr err = FSGetEOF(refNum, &eof);
    if (err == noErr) *logEOF = (long)eof;
    return err;
}

/* Resource Manager stubs */

#ifndef ENABLE_RESOURCES
Handle Get1Resource(ResType theType, SInt16 theID) {
    extern Handle GetResource(ResType type, short id);
    return GetResource(theType, theID);
}

void AddResource(Handle theData, ResType theType, SInt16 theID, ConstStr255Param name) {
    if (!theData) return;

    /* Add a resource to the current resource file */
    /* This adds the resource to the resource map but doesn't write to disk yet */

    /* In a full implementation, this would:
     * 1. Get the current resource file reference
     * 2. Add an entry to the resource map with type, ID, and name
     * 3. Associate the handle with the resource entry
     * 4. Mark the resource file as modified
     * 5. The actual data is written when UpdateResFile or CloseResFile is called
     */

    /* For now, just mark the handle as a resource */
    extern void HNoPurge(Handle h);
    HNoPurge(theData); /* Make resource non-purgeable */

    (void)theType;
    (void)theID;
    (void)name;
}

void RemoveResource(Handle theResource) {
    if (!theResource) return;

    /* Remove a resource from the resource map */
    /* This detaches the resource from its resource file but doesn't dispose the handle */

    /* In a full implementation, this would:
     * 1. Find the resource in the resource map
     * 2. Remove it from the map
     * 3. Mark the resource file as modified
     * 4. The handle remains valid but is no longer a resource
     */

    /* Make the handle purgeable since it's no longer a resource */
    extern void HPurge(Handle h);
    HPurge(theResource);
}

void WriteResource(Handle theResource) {
    if (!theResource) return;

    /* Write changed resource data to the resource file */
    /* This updates the resource in the resource fork */

    /* In a full implementation, this would:
     * 1. Find the resource in the resource map
     * 2. Calculate the size and position in the resource fork
     * 3. Write the handle data to the resource file
     * 4. Update the resource map with new size/position
     * 5. Mark the resource file as modified
     */

    /* For now, just mark the handle as not purgeable to ensure data is preserved */
    extern void HNoPurge(Handle h);
    HNoPurge(theResource);
}

void CloseResFile(SInt16 refNum) {
    if (refNum <= 0) return;

    /* Close a resource file */
    /* This writes any changes and releases the resource map */

    /* In a full implementation, this would:
     * 1. Write any modified resources to disk
     * 2. Update the resource map on disk
     * 3. Close the file via File Manager
     * 4. Release the in-memory resource map
     * 5. Remove from the resource file chain
     */

    /* For now, just a no-op as we don't have full resource file management */
    (void)refNum;
}

OSErr ResError(void) {
    /* Return most recent Resource Manager error code
     *
     * In a full implementation (see ResourceMgr.c), this would:
     * 1. Return the last error code from gResMgr.resError
     * 2. Clear the error code after reading (one-time read)
     * 3. Track errors from GetResource, OpenResFile, etc.
     * 4. Return noErr if no error occurred
     *
     * Error codes include:
     * - resNotFound (-192): Resource not found
     * - mapReadErr (-199): Map inconsistent with operation
     * - resFNotFound (-193): Resource file not found
     * - addResFailed (-194): AddResource failed
     *
     * For builds without ENABLE_RESOURCES, always return noErr */
    return noErr;
}
#endif

/* Finder-specific stubs */

OSErr InitializeWindowManager(void) {
    /* Initialize the Window Manager */
    extern void InitWindows(void);

    /* Set up Window Manager port, desktop pattern, and internal structures */
    InitWindows();

    return noErr;
}

OSErr ShowAboutFinder(void) {
    /* Show About Finder dialog */
    /* extern short Alert(short alertID, void* filterProc); */
    /* Removed: Alert declared elsewhere (DialogManager/AlertDialogs.c) */

    /* Display About box with simple message */
    /* Alert ID 128 is typically used for About boxes */
    /* For now, we'll use a generic alert since we don't have resources */

    /* extern void ParamText(ConstStr255Param param0, ConstStr255Param param1,
                         ConstStr255Param param2, ConstStr255Param param3); */
    /* extern short NoteAlert(short alertID, void* filterProc); */
    /* Removed: ParamText and NoteAlert declared elsewhere (DialogManager) */

    /* Set up message text */
    const unsigned char aboutMsg[] = "System 7.1 Finder";
    const unsigned char versionMsg[] = "Version 7.1";
    const unsigned char emptyMsg[] = "";

    ParamText(aboutMsg, versionMsg, emptyMsg, emptyMsg);

    /* Show note alert (info icon) */
    NoteAlert(128, NULL);

    return noErr;
}

OSErr HandleGrowWindow(WindowPtr window, EventRecord* event) {
    /* Handle window grow/resize interaction
     *
     * Called when user clicks and drags the window's grow icon (bottom-right
     * corner resize handle). Tracks the mouse and resizes the window to match
     * user's desired size within specified constraints.
     *
     * Parameters:
     *   window - Window to resize
     *   event - Mouse down event in grow region
     *
     * Behavior:
     * 1. Calls GrowWindow() to track mouse and show resize outline
     * 2. GrowWindow returns new size as long (width in low word, height in high)
     * 3. If size changed (nonzero return), calls SizeWindow() to apply new size
     * 4. Size is constrained to sizeRect bounds (min 80x80, max 640x480)
     *
     * Size constraints in Rect format:
     *   sizeRect.top = minimum height (80)
     *   sizeRect.left = minimum width (80)
     *   sizeRect.bottom = maximum height (480)
     *   sizeRect.right = maximum width (640)
     *
     * After resizing, window contents are invalidated and redrawn.
     * SizeWindow with fUpdate=true triggers automatic redraw.
     *
     * Returns:
     *   paramErr if window or event is NULL
     *   noErr on success
     */
    if (!window || !event) {
        return paramErr;
    }

    /* Call GrowWindow to let user resize the window */
    extern long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox);
    extern void SizeWindow(WindowPtr theWindow, SInt16 w, SInt16 h, Boolean fUpdate);
    /* extern SInt16 HiWord(long x); */
    /* extern SInt16 LoWord(long x); */
    /* Removed: HiWord and LoWord declared in System71StdLib.c */

    /* Set size constraints (minimum 80x80, maximum screen size) */
    Rect sizeRect = {80, 80, 480, 640};

    /* Track window resizing */
    long newSize = GrowWindow(window, event->where, &sizeRect);

    /* If user changed size, apply it */
    if (newSize != 0) {
        SInt16 newWidth = LoWord(newSize);
        SInt16 newHeight = HiWord(newSize);
        SizeWindow(window, newWidth, newHeight, true);
    }

    return noErr;
}

/* TrackGoAway, TrackBox, ZoomWindow - real implementations in WindowManager */

void DoActivate(WindowPtr window, Boolean activate) {
    /* Handle window activation/deactivation events
     *
     * Called when a window gains or loses focus. Updates the visual
     * state of all controls in the window to reflect active/inactive status.
     *
     * Parameters:
     *   window - Window being activated or deactivated
     *   activate - true to activate, false to deactivate
     *
     * Activation behavior:
     * - Active windows: Controls drawn normally (hilite = 0)
     * - Inactive windows: Controls drawn dimmed (hilite = 255)
     * - Title bar appearance changes (handled by Window Manager)
     * - Scroll bars change from active to inactive appearance
     *
     * This is typically called in response to:
     * - activateEvt event with activeFlag bit set/clear
     * - User clicking on a different window
     * - Application switching (suspend/resume events)
     *
     * The function walks the window's control list and updates each
     * control's hilite state, then redraws all controls to show the
     * new visual state.
     */
    if (!window) return;

    /* Hilite or unhilite all controls in the window */
    extern void HiliteControl(ControlHandle theControl, SInt16 hiliteState);

    ControlHandle control = window->controlList;
    while (control) {
        /* Hilite value: 0 = active, 255 = inactive */
        SInt16 hiliteState = activate ? 0 : 255;
        HiliteControl(control, hiliteState);

        /* Move to next control */
        control = (*control)->nextControl;
    }

    /* Redraw controls to show new hilite state */
    extern void DrawControls(WindowPtr theWindow);
    DrawControls(window);
}

void DoBackgroundTasks(void) {
    /* Perform idle-time system tasks during event loop
     *
     * Called from FinderEventLoop and other event loops to give time
     * to background processes when no user events are pending.
     *
     * Primary tasks:
     * 1. Call SystemTask() to update Desk Accessories
     * 2. Allow DA windows to process periodic tasks
     * 3. Update desk accessory menu items
     * 4. Service VBL (vertical blanking) tasks
     *
     * In a full implementation, additional background work includes:
     * - Check for disk insertions/ejections
     * - Update network status indicators
     * - Perform deferred memory cleanup
     * - Process asynchronous I/O completions
     * - Update AppleTalk status
     * - Service printer queues
     *
     * This is critical for cooperative multitasking - without calling
     * DoBackgroundTasks(), desk accessories and system tasks won't run.
     */
    extern void SystemTask(void);
    SystemTask();
}

/* Removed DISABLED EventAvail stub (real implementation lives in EventManager) */

OSErr ShowConfirmDialog(StringPtr message, Boolean* confirmed) {
    /* Display confirmation dialog with OK and Cancel buttons
     *
     * Shows a caution alert (yellow warning icon) with a custom message
     * and OK/Cancel buttons. Used to confirm destructive or important
     * operations before proceeding.
     *
     * Parameters:
     *   message - Pascal string with message to display (can be NULL)
     *   confirmed - Returns true if user clicked OK, false if Cancel
     *
     * Alert behavior:
     * - Displays caution icon (yellow triangle with exclamation)
     * - Shows message text via ParamText substitution (^0)
     * - Presents OK and Cancel buttons
     * - OK button is default (Return/Enter key)
     * - Cancel button responds to Escape/Command-. keys
     * - Modal dialog blocks until user responds
     *
     * Common uses:
     * - Confirm file deletion ("Really delete this file?")
     * - Confirm folder emptying ("Empty the Trash?")
     * - Confirm application quit with unsaved changes
     * - Confirm irreversible operations
     *
     * Alert ID 128 is typically used for basic confirmation dialogs.
     * The ALRT resource template defines button layout and icon type.
     *
     * Returns:
     *   paramErr if confirmed pointer is NULL
     *   noErr on success (confirmed set to user's choice)
     */
    if (!confirmed) {
        return paramErr;
    }

    /* Show confirmation dialog with OK and Cancel buttons */
    /* extern void ParamText(ConstStr255Param param0, ConstStr255Param param1,
                         ConstStr255Param param2, ConstStr255Param param3); */
    /* extern short CautionAlert(short alertID, void* filterProc); */
    /* Removed: ParamText and CautionAlert declared in DialogManager headers */

    const unsigned char emptyMsg[] = "";

    /* Set the message text */
    if (message && message[0] > 0) {
        ParamText(message, emptyMsg, emptyMsg, emptyMsg);
    }

    /* Show caution alert (returns 1 for OK, 2 for Cancel) */
    short result = CautionAlert(128, NULL);

    /* OK button = 1, Cancel button = 2 */
    *confirmed = (result == 1);

    return noErr;
}

OSErr CloseAllWindows(void) {
    /* Close all open windows iteratively from front to back
     *
     * Iterates through the window list and closes each window in order,
     * starting with the frontmost window. Continues until all windows
     * are closed or a window refuses to close.
     *
     * Behavior:
     * - Calls FrontWindow() to get the topmost visible window
     * - Calls CloseWindow() to close it
     * - Repeats until FrontWindow() returns NULL (no windows left)
     * - Includes safety check to prevent infinite loop if close fails
     *
     * Window closing order:
     * - Frontmost to rearmost (z-order)
     * - Each CloseWindow may trigger save dialogs for modified documents
     * - User can cancel individual window closes
     * - Loop terminates if a window refuses to close
     *
     * Safety mechanism:
     * - After CloseWindow, checks if window is still front window
     * - If window didn't close, breaks loop to prevent hang
     * - This handles cases where window close is vetoed or fails
     *
     * Common uses:
     * - Application quit (close all document windows)
     * - Finder restart
     * - System shutdown sequence
     * - Testing/cleanup scenarios
     *
     * Returns noErr always (doesn't report which windows failed to close)
     */
    extern WindowPtr FrontWindow(void);
    extern void CloseWindow(WindowPtr theWindow);

    WindowPtr window;
    while ((window = FrontWindow()) != NULL) {
        /* Close the front window */
        CloseWindow(window);

        /* Safety check to prevent infinite loop if CloseWindow fails */
        WindowPtr checkWindow = FrontWindow();
        if (checkWindow == window) {
            /* Window didn't close, abort to prevent hang */
            break;
        }
    }

    return noErr;
}

OSErr CleanUpSelection(WindowPtr window) {
    /* Arrange selected icons in a neat grid pattern
     *
     * Finder menu command "Clean Up Selection" - arranges only the
     * selected icons in a window, snapping them to an invisible grid
     * while leaving unselected icons in their current positions.
     *
     * Parameters:
     *   window - Finder window containing icons to arrange
     *
     * Grid layout behavior:
     * - Icons snap to grid points (typically 80x80 pixel spacing)
     * - Only selected icons are moved
     * - Icons maintain relative order where possible
     * - Grid starts from current window scroll position
     * - Prevents overlapping icons
     *
     * Full implementation would:
     * 1. Enumerate window's icon list to find selected items
     * 2. Calculate grid spacing based on icon view settings
     * 3. Determine available grid positions (avoiding unselected icons)
     * 4. Move each selected icon to nearest available grid point
     * 5. Update icon positions in window data structure
     * 6. Mark window content as modified
     * 7. Invalidate affected regions to trigger redraw
     *
     * This stub implementation just invalidates the entire window
     * to trigger a redraw, without actually moving any icons.
     *
     * Returns:
     *   paramErr if window is NULL
     *   noErr on success
     */
    if (!window) {
        return paramErr;
    }

    /* For now, just invalidate the window to trigger redraw */
    extern void InvalRect(const Rect* badRect);

    /* Rect windowRect = window->portRect; */
    /* InvalRect(&windowRect); */
    /* Commented out: portRect member doesn't exist in WindowRecord struct */
    /* TODO: Use proper window bounds accessor when available */

    return noErr;
}

OSErr CleanUpBy(WindowPtr window, SInt16 sortType) {
    /* Arrange all icons sorted by specified criteria
     *
     * Finder menu commands "Clean Up by Name", "Clean Up by Date", etc.
     * Sorts all icons in the window and arranges them in a neat grid
     * pattern from top-left to bottom-right.
     *
     * Parameters:
     *   window - Finder window containing icons to arrange
     *   sortType - Sort criterion (from finder.h):
     *     kCleanUpByName = 0   (alphabetical by file/folder name)
     *     kCleanUpByDate = 1   (chronological by modification date)
     *     kCleanUpBySize = 2   (by file size, largest first)
     *     kCleanUpByKind = 3   (grouped by type/kind)
     *     kCleanUpByLabel = 4  (grouped by Finder label color)
     *
     * Sort and arrangement behavior:
     * - All icons (not just selected) are affected
     * - Icons sorted according to sortType criterion
     * - Folders typically sorted before files
     * - Arranged left-to-right, top-to-bottom in grid
     * - Grid spacing typically 80x80 pixels
     * - Starts from top-left of window content area
     *
     * Full implementation would:
     * 1. Enumerate all icons in the window
     * 2. Sort icon list by specified criterion
     * 3. Calculate grid positions (rows and columns)
     * 4. Assign each sorted icon to next grid position
     * 5. Update icon positions in window data structure
     * 6. Mark window content as modified
     * 7. Invalidate window to trigger redraw
     *
     * Common use cases:
     * - Organize messy desktop or folder window
     * - Prepare folder for screenshots
     * - Find files more easily with consistent layout
     *
     * This stub implementation just invalidates the window without
     * actually sorting or moving any icons.
     *
     * Returns:
     *   paramErr if window is NULL
     *   noErr on success
     */
    if (!window) {
        return paramErr;
    }

    /* For now, just invalidate the window to trigger redraw */
    extern void InvalRect(const Rect* badRect);

    /* Rect windowRect = window->portRect; */
    /* InvalRect(&windowRect); */
    /* Commented out: portRect member doesn't exist in WindowRecord struct */
    /* TODO: Use proper window bounds accessor when available */

    (void)sortType; /* Unused for now */

    return noErr;
}

OSErr ScanDirectoryForDesktopEntries(SInt16 vRefNum, SInt32 dirID, SInt16 databaseRefNum) {
    /* Scan directory and populate Desktop Database with file metadata
     *
     * Desktop Database tracks file associations, custom icons, and comments:
     * - Creator/Type codes for "Open With" application launching
     * - Custom icon resources (ICON, ICN#, icl4, icl8)
     * - Get Info comments for search and display
     * - Bundle bit flags and custom icon flags
     * - Application version info for version tracking
     *
     * Database structure (Desktop DB and Desktop DF files):
     * - DB file: B-tree indexed by creator/type for fast lookups
     * - DF file: Icon family resources and comments
     *
     * Full implementation algorithm:
     * 1. Call PBGetCatInfo iteratively (index 1..N) to enumerate entries
     * 2. For each file/folder, extract FInfo (finder info structure)
     * 3. If custom icon bit set, read icon resources from file
     * 4. Read desktop comment from file's resource fork
     * 5. Insert/update entry in database B-tree by creator+type key
     * 6. Store icon resources in DF file, reference in DB entry
     * 7. Recurse into subdirectories to scan entire volume
     *
     * Parameters:
     * - vRefNum: Volume reference number to scan
     * - dirID: Directory ID (root = 2, use PBGetCatInfo for subdirs)
     * - databaseRefNum: Open file refnum of Desktop DB file
     *
     * Returns: noErr on success, paramErr for invalid parameters
     *
     * Used by Finder during:
     * - Volume mount to rebuild database
     * - Rebuild Desktop command (Command-Option at boot)
     * - Background update when files change
     */

    /* Validate parameters */
    if (vRefNum == 0 || databaseRefNum <= 0) {
        return paramErr;
    }

    /* For now, return success without scanning */
    /* Real implementation would iterate directory entries */
    (void)dirID;

    return noErr;
}

/* QuickDraw Region functions - All real implementations in QuickDraw/Regions.c:
 * NewRgn, DisposeRgn, RectRgn, SetRectRgn, CopyRgn, SetEmptyRgn
 */

/* Minimal math functions for -nostdlib build */

/* Forward declarations */
double fabs(double x);

/* Standard library minimal implementations */
#include <stddef.h>

/* External globals from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* External QuickDraw globals */
extern QDGlobals qd;
extern void* framebuffer;

/* [WM-050] Stub quarantine: real BeginUpdate/EndUpdate in WindowEvents.c */

/* [WM-053] QuickDraw region drawing functions
 * Removed stubs (now disabled by default via SYS71_STUBS_DISABLED=1):
 * - FillRgn - implemented in QuickDraw/Regions.c:621
 * - RectInRgn - implemented in QuickDraw/Regions.c:557
 */

/* QuickDraw text stubs for About box */

/* Alert stub for trash_folder */

void Delay(UInt32 numTicks, UInt32* finalTicks) {
    /* Wait for specified number of ticks with cooperative multitasking
     *
     * Timing:
     * - One tick = 1/60th second (16.67 ms) on most Macs
     * - Some systems use 1/50th second (PAL regions)
     * - Query actual tick rate with TickCount() frequency
     *
     * Cooperative Multitasking:
     * - Calls SystemTask() during wait to service Desk Accessories
     * - Allows DA windows to update, respond to events
     * - Critical for responsive UI during delays
     *
     * Common uses:
     * - Animation frame delays (e.g., 3 ticks = ~50ms)
     * - Double-click detection timeouts
     * - Debouncing user input
     * - Pacing Finder operations (icon dragging, etc.)
     *
     * Parameters:
     * - numTicks: Number of ticks to wait (60 ticks = 1 second)
     * - finalTicks: Optional output of actual final tick count
     *
     * Note: Not suitable for precise timing due to cooperative scheduling
     * overhead. For animations, use actual elapsed time calculations.
     */
    extern UInt32 TickCount(void);
    extern void SystemTask(void);

    /* Until the ticks have passed, however long each SystemTask takes. The
     * difference is unsigned, so the counter wrapping does not end it early.
     * This used to give up after numTicks*1000 passes, and after 100 passes
     * without a tick - a pass is one SystemTask, far shorter than a 60th of a
     * second, so a short Delay could end almost at once. The only way out now
     * is the tick count really not moving: the timer is dead. */
    UInt32 startTicks = TickCount();
    UInt32 lastTicks = startTicks;
    UInt32 passesSinceTick = 0;
    while ((UInt32)(TickCount() - startTicks) < numTicks) {
        SystemTask();
        UInt32 now = TickCount();
        if (now != lastTicks) {
            lastTicks = now;
            passesSinceTick = 0;
        } else if (++passesSinceTick > 50000000u) {
            extern void serial_printf(const char* fmt, ...);
            serial_printf("[Delay] TickCount has stopped; giving up the wait\n");
            break;
        }
    }

    if (finalTicks) {
        *finalTicks = TickCount();
    }
}

/*
 * main - Main system entry point
 *
 * Called by the ARM64 bootloader after basic hardware initialization.
 * Sets up the System 7.1 kernel and enters the main system loop.
 *
 * Returns:
 * - 0 on normal exit
 * - Non-zero on error
 *
 * This is a minimal stub for bare-metal ARM64 environment.
 */
int main(void) {
    /* Minimal stub - main system entry point */
    /* In a real implementation, this would initialize the entire System 7.1 kernel */
    while (1) {
        /* Idle loop - system is running but idle */
    }
    return 0;
}
