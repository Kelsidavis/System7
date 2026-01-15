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

/* DeskHook type definition if not in headers */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);

/* Platform Menu System stubs */
void Platform_InitMenuSystem(void) {
    /* Platform-specific menu initialization
     * Called by InitMenus() during Menu Manager initialization
     *
     * In a full implementation, this would:
     * - Initialize platform-specific menu rendering state
     * - Set up menu bar cursor resources
     * - Allocate platform-specific menu caches
     * - Register menu-related event handlers
     * - Initialize platform menu tracking structures
     *
     * For the kernel environment, menu rendering is handled by
     * MenuDisplay.c and platform_stubs.c, so this is a no-op.
     */
}

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

/* QuickDraw - InitGraf is now provided by QuickDrawCore.c */

/* Font Manager */
/* Moved to FontManagerCore.c
void InitFonts(void) {
    // Stub implementation
}
*/

/* [WM-050] Window Manager stub quarantine
 * Provenance: IM:Windows Vol I - real implementations in WindowDisplay.c, WindowEvents.c, WindowResizing.c
 * Policy: Stubs compile only if SYS71_PROVIDE_FINDER_TOOLBOX is undefined
 * Real WM always wins; no dual definitions
 */
/* DrawWindow removed - implemented in WindowManager/WindowDisplay.c:919
 * Window chrome is drawn by real implementation, not this disabled stub */

/* Menu Manager - Most functions now provided by MenuManagerCore.c */

/* AppendMenu now implemented in MenuItems.c */

/* Dialog Manager - provided by DialogManagerCore.c */

/* Control Manager */
/* InitControlManager_Sys7 now implemented in ControlManager/control_manager_sys7.c */

/* List Manager */
void InitListManager(void) {
    /* Initialize List Manager for displaying scrollable lists
     *
     * The List Manager provides utilities for creating and managing
     * scrollable lists in dialogs and windows. It handles:
     * - List creation and disposal (LNew, LDispose)
     * - Cell selection and highlighting
     * - Scrolling and updating
     * - Drawing list contents
     * - Mouse tracking in lists
     *
     * Used extensively in:
     * - Standard File dialogs (file/folder lists)
     * - Font selection dialogs
     * - Color picker lists
     * - Custom application lists
     *
     * This initialization:
     * 1. Sets up internal dialog-list registry via InitRegistryIfNeeded()
     * 2. Prepares data structures for LNew() calls
     * 3. Initializes list manager globals
     *
     * After this call, applications can use LNew() to create lists.
     */
    extern void InitRegistryIfNeeded(void);

    /* Set up internal registry for dialog-attached lists */
    InitRegistryIfNeeded();

    /* List Manager is now ready for LNew() calls */
}

/* Event Manager */
/* InitEvents now implemented in EventManager/EventManagerCore.c */

/* Removed DISABLED GetNextEvent stub (real implementation lives in EventManager) */

/* Removed DISABLED PostEvent stub (real implementation lives in EventManager) */

/* GenerateSystemEvent now provided by EventManager/event_manager.c */
/* Forward declaration for compatibility */
extern void GenerateSystemEvent(short eventType, int message, Point where, short modifiers);

/* SystemTask provided by DeskManagerCore.c */

/* External functions we use */

/* ExpandMem stubs for SystemInit
 * These functions manage the Expanded Memory structure introduced in System 7.
 * ExpandMem contains global state and handles that don't fit in the original
 * 256-byte low memory area. It's allocated in the system heap during boot. */

#ifndef EXPANDMEM_FULL_IMPL
/* Stub implementations when ExpandMem.c is not included */
void ExpandMemInit(void) {
    SYSTEM_LOG_DEBUG("ExpandMemInit: Initializing expanded memory\n");
    /* In a full implementation, this would:
     * 1. Allocate ExpandMem handle in system heap (typically ~512 bytes)
     * 2. Initialize ExpandMem structure with magic number 'expm'
     * 3. Set up pointers to low-memory globals
     * 4. Initialize expansion slots and device structures
     * 5. Store handle in low-memory global at 0x02B6 (ExpandMemRec)
     *
     * For kernel environment, expanded memory is managed elsewhere */
}

void ExpandMemInitKeyboard(void) {
    SYSTEM_LOG_DEBUG("ExpandMemInitKeyboard: Initializing keyboard expanded memory\n");
    /* In a full implementation, this would:
     * 1. Set up keyboard state in ExpandMem
     * 2. Initialize keyboard repeat rate (ExpandMem+0x1A)
     * 3. Set keyboard type (ExpandMem+0x1C)
     * 4. Configure dead key state for international keyboards
     *
     * For kernel environment, keyboard state is in EventManager globals */
}

void ExpandMemSetAppleTalkInactive(void) {
    SYSTEM_LOG_DEBUG("ExpandMemSetAppleTalkInactive: Disabling AppleTalk\n");
    /* In a full implementation, this would:
     * 1. Clear AppleTalk active flag in ExpandMem+0x4E
     * 2. Mark AppleTalk driver as inactive
     * 3. Prevent AppleTalk from loading at system startup
     *
     * For kernel environment without networking, this is a no-op */
}
#endif /* EXPANDMEM_FULL_IMPL */
void SetAutoDecompression(Boolean enable) {
    SYSTEM_LOG_DEBUG("SetAutoDecompression: %s\n", enable ? "Enabled" : "Disabled");
    /* In a full implementation, this would:
     * 1. Set resource decompression flag in ExpandMem
     * 2. Control automatic decompression of compressed resources
     * 3. Used by Resource Manager when loading 'dcmp' resources
     *
     * For kernel environment, resources are not compressed */
    (void)enable;
}

void ResourceManager_SetDecompressionCacheSize(Size size) {
    SYSTEM_LOG_DEBUG("ResourceManager_SetDecompressionCacheSize: Setting cache to %lu bytes\n", (unsigned long)size);
    /* In a full implementation, this would:
     * 1. Allocate decompression cache in system heap
     * 2. Configure cache size for decompressed resource data
     * 3. Improve performance for frequently-accessed compressed resources
     *
     * For kernel environment, no compression cache needed */
    (void)size;
}

void InstallDecompressHook(DecompressHookProc proc) {
    SYSTEM_LOG_DEBUG("InstallDecompressHook: Installing hook at %p\n", proc);
    /* In a full implementation, this would:
     * 1. Store decompression hook pointer in ExpandMem
     * 2. Allow custom decompression algorithms
     * 3. Called by Resource Manager for 'dcmp' resources
     *
     * For kernel environment, no custom decompression */
    (void)proc;
}

#ifndef EXPANDMEM_FULL_IMPL
void ExpandMemInstallDecompressor(void) {
    SYSTEM_LOG_DEBUG("ExpandMemInstallDecompressor: Installing default decompressor\n");
    /* In a full implementation, this would:
     * 1. Install default resource decompressor hook
     * 2. Support standard System 7 compression algorithms
     * 3. Register with Resource Manager for automatic decompression
     *
     * For kernel environment, resources are not compressed */
}

void ExpandMemCleanup(void) {
    SYSTEM_LOG_DEBUG("ExpandMemCleanup: Cleaning up expanded memory\n");
    /* In a full implementation, this would:
     * 1. Dispose ExpandMem handle from system heap
     * 2. Clear low-memory global at 0x02B6
     * 3. Release any expansion slot structures
     * 4. Called during system shutdown
     *
     * For kernel environment, cleanup handled by memory manager */
}

void ExpandMemDump(void) {
    /* Debug function to dump ExpandMem contents to serial output
     * This is a wrapper around ExpandMemDump(ExpandMemRec*, output_func)
     *
     * Displays:
     * - Version and size information
     * - Keyboard: type, script, key cache pointer
     * - Processes: count, current process, process list
     * - Memory: physical/logical RAM sizes
     * - Network: AppleTalk status, config
     * - VM: enabled status, page size
     * - Resources: cache, decompressor, flags
     * - Display: screen count, color table
     * - Sound: channel count, globals
     * - Errors: last error code, error handler
     * - FileSystem: default and boot volumes
     *
     * Useful for debugging system initialization and state issues
     */

    extern ExpandMemRec* ExpandMemGet(void);
    /* extern void ExpandMemDump(const ExpandMemRec* em, void (*output_func)(const char* text)); */
    /* Removed: causes recursive call issue - function calls itself */
    extern void serial_puts(const char* s);

    ExpandMemRec* em = ExpandMemGet();
    if (!em) {
        serial_puts("ExpandMemDump: ExpandMem not initialized\n");
        return;
    }

    /* Delegate to real implementation with serial output */
    /* ExpandMemDump(em, serial_puts); */
    /* Commented out: recursive wrapper issue - cannot call function with same name */
    serial_puts("ExpandMemDump: Wrapper stub - real implementation needed\n");
}
Boolean ExpandMemValidate(void) {
    /* Validate expanded memory structure integrity
     * This is a wrapper around the full ExpandMemValidate(ExpandMemRec*) function
     *
     * Validation checks performed by real implementation:
     * 1. Check ExpandMem pointer is non-NULL
     * 2. Verify version number matches EM_CURRENT_VERSION
     * 3. Validate size is within reasonable bounds (sizeof(ExpandMemRec) to 2*EM_EXTENDED_SIZE)
     * 4. Check for memory corruption indicators
     *
     * Returns: true if ExpandMem is valid, false otherwise
     */

    extern ExpandMemRec* ExpandMemGet(void);
    /* extern Boolean ExpandMemValidate(const ExpandMemRec* em); */
    /* Removed: causes recursive call issue - function calls itself */

    ExpandMemRec* em = ExpandMemGet();
    if (!em) {
        /* No ExpandMem allocated yet - this is valid during early boot */
        return true;
    }

    /* Delegate to real implementation */
    /* return ExpandMemValidate(em); */
    /* Commented out: recursive wrapper issue - cannot call function with same name */
    return true; /* Stub: assume valid */
}
#endif /* EXPANDMEM_FULL_IMPL */

/* Serial stubs */
#include <stdarg.h>

/* serial_printf moved to System71StdLib.c */

/* Finder InitializeFinder provided by finder_main.c */

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

/* TEInit now implemented in TextEditCore.c */

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


/* Functions provided by other components:
 * ShowErrorDialog - finder_main.c
 * CleanUpDesktop - desktop_manager.c
 * DrawDesktop - desktop_manager.c
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

/* Resource Manager functions provided by Memory Manager */

/* HandleKeyDown moved to Finder/finder_main.c */

/* ResolveAliasFile moved to Finder/alias_manager.c */

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

/* NewAlias moved to Finder/alias_manager.c */

/* Memory Manager functions provided by MemoryManager.c */

OSErr FSpCreateResFile(const FSSpec* spec, OSType creator, OSType fileType, SInt16 scriptTag) {
    if (!spec) {
        return paramErr;
    }

    /* Create a new resource file with empty resource fork */

    /* In a full implementation, this would:
     * 1. Create the file using FSpCreate with specified creator/type
     * 2. Create an empty resource fork
     * 3. Write an empty resource map to the resource fork
     * 4. Close the file
     */

    /* For now, just call FSpCreate to create the data fork */
    OSErr err = FSpCreate(spec, creator, fileType, scriptTag);
    if (err != noErr) {
        return err;
    }

    /* Resource fork initialization would happen here in full implementation */

    return noErr;
}

/* File Manager stubs - Core functions now implemented in FileManager.c */

OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, SInt16 scriptTag) {
    if (!spec) {
        return paramErr;
    }

    /* Create a new file with specified creator and type */

    /* In a full implementation, this would:
     * 1. Validate the FSSpec (volume and parent directory exist)
     * 2. Check if file already exists (return dupFNErr if so)
     * 3. Create directory entry with name, creator, type
     * 4. Set file dates (creation, modification)
     * 5. Initialize empty data fork
     */

    /* For now, return success */
    /* Real implementation would call File Manager PBHCreate */
    (void)creator;
    (void)fileType;
    (void)scriptTag;

    return noErr;
}

OSErr FSpOpenDF(const FSSpec* spec, SInt16 permission, SInt16* refNum) {
    if (!spec || !refNum) {
        return paramErr;
    }

    /* Open the data fork of a file */

    /* In a full implementation, this would:
     * 1. Validate the FSSpec points to an existing file
     * 2. Check permission flags (read, write, read/write)
     * 3. Allocate a file control block (FCB)
     * 4. Open the data fork via File Manager
     * 5. Return the file reference number
     */

    /* Return a fake file reference number for now */
    *refNum = 1;

    (void)permission;

    return noErr;
}

OSErr FSpOpenResFile(const FSSpec* spec, SInt16 permission) {
    if (!spec) {
        return -1; /* Invalid resource file reference */
    }

    /* Open the resource fork of a file */

    /* In a full implementation, this would:
     * 1. Validate the FSSpec points to an existing file
     * 2. Open the resource fork
     * 3. Read and parse the resource map
     * 4. Add the file to the resource file chain
     * 5. Return the resource file reference number
     */

    /* Permission values:
     * fsCurPerm (0) - read/write based on file permissions
     * fsRdPerm (1) - read-only
     * fsWrPerm (2) - write-only
     * fsRdWrPerm (3) - read/write
     */

    /* Return fake resource file reference number */
    (void)permission;
    return 1;
}

OSErr FSpDelete(const FSSpec* spec) {
    if (!spec) {
        return paramErr;
    }

    /* Delete a file or empty directory */

    /* In a full implementation, this would:
     * 1. Validate the FSSpec points to an existing file or directory
     * 2. Check if item is a directory - if so, verify it's empty
     * 3. Remove the item from its parent directory
     * 4. Release disk space allocated to the item
     * 5. Update the volume's free space count
     */

    /* For now, return success */
    return noErr;
}

OSErr FSpDirDelete(const FSSpec* spec) {
    if (!spec) {
        return paramErr;
    }

    /* Delete a directory and all its contents recursively */

    /* In a full implementation, this would:
     * 1. Validate the FSSpec points to a directory
     * 2. Recursively delete all files and subdirectories
     * 3. Delete the directory itself after emptying
     * 4. Return appropriate errors for locked or busy files
     */

    /* This is more dangerous than FSpDelete which requires empty directory */
    /* For now, return success */
    return noErr;
}

OSErr FSpCatMove(const FSSpec* source, const FSSpec* dest) {
    if (!source || !dest) {
        return paramErr;
    }

    /* Move or rename a file or directory */

    /* In a full implementation, this would:
     * 1. Validate source exists and dest doesn't exist
     * 2. Check if moving to different volume (requires copy+delete)
     * 3. If same volume, update directory entry
     * 4. If different volume, copy data then delete source
     * 5. Preserve file metadata (creator, type, dates)
     */

    /* For now, return success */
    return noErr;
}


OSErr PBHGetVInfoSync(void *paramBlock) {
    if (!paramBlock) {
        return paramErr;
    }

    HParamBlockRec* pb = (HParamBlockRec*)paramBlock;

    /* Get volume information synchronously */

    /* In a full implementation, this would:
     * 1. Validate the volume reference number
     * 2. Retrieve actual volume parameters from the volume control block
     * 3. Fill in allocation block size, total blocks, free blocks
     * 4. Set volume name, creation date, modification date
     * 5. Return volume attributes (locked, ejectable, etc.)
     */

    /* Fill in simulated volume info for 400K floppy disk */
    pb->u.volumeParam.ioVAlBlkSiz = 512;     /* Allocation block size in bytes */
    pb->u.volumeParam.ioVNmAlBlks = 800;     /* Total allocation blocks (400K) */
    /* pb->u.volumeParam.ioVFrBlk = 400; */  /* Free blocks (50% free) */
    /* Commented out: ioVFrBlk member doesn't exist in volumeParam struct */

    return noErr;
}


OSErr SetEOF(short refNum, long logEOF) {
    if (refNum <= 0) {
        return paramErr;
    }

    if (logEOF < 0) {
        return paramErr;
    }

    /* Set the logical end-of-file for an open file */

    /* In a full implementation, this would:
     * 1. Validate the file reference number
     * 2. If growing file, allocate additional disk blocks
     * 3. If shrinking file, release unused disk blocks
     * 4. Update the file control block with new EOF
     * 5. Update directory entry if needed
     */

    /* For now, return success */
    return noErr;
}

/* Resource Manager stubs */
/* NewHandle and DisposeHandle provided by Memory Manager */
/* GetResource provided by simple_resource_manager.c */

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

/* AddResMenu and InsertResMenu now implemented in MenuManager/menu_stubs.c
 * - AddResMenu: Enumerates resources and appends names to menu
 * - InsertResMenu: Enumerates resources and inserts names at position */

/* InsertFontResMenu moved to MenuManager/MenuItems.c */

/* Memory Manager functions provided by Memory Manager */
/* MemError moved to System71StdLib.c */

/* BlockMoveData moved to System71StdLib.c */

/* Finder-specific stubs */
/* FindFolder moved to Finder/finder_main.c */

/* GenerateUniqueTrashName moved to Finder/trash_folder.c */

OSErr InitializeWindowManager(void) {
    /* Initialize the Window Manager */
    extern void InitWindows(void);

    /* Set up Window Manager port, desktop pattern, and internal structures */
    InitWindows();

    return noErr;
}

/* InitializeTrashFolder provided by trash_folder.c */

OSErr HandleGetInfo(void) {
    /* Show Get Info dialog for selected items in front window */
    extern WindowPtr FrontWindow(void);
    extern OSErr ShowGetInfo(FSSpec *items, short count);

    WindowPtr window = FrontWindow();
    if (!window) {
        return paramErr;
    }

    /* In a full implementation, this would:
     * 1. Get selected items from the front window
     * 2. Build an array of FSSpec for selected items
     * 3. Call ShowGetInfo with the array
     */

    /* For now, show Get Info for a dummy item */
    /* Real implementation would get selection from window */
    FSSpec dummyItem;
    dummyItem.vRefNum = 0;
    dummyItem.parID = 0;
    dummyItem.name[0] = 0;

    return ShowGetInfo(&dummyItem, 1);
}

/* ShowFind and FindAgain implemented in src/Finder/Find.c */

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

/* HandleContentClick moved to Finder/finder_main.c */

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

/* CloseFinderWindow moved to Finder/finder_main.c */
/* TrackGoAway, TrackBox, ZoomWindow - real implementations in WindowManager */
/* DoUpdate moved to Finder/finder_main.c */

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

/* WaitNextEvent now implemented in EventManager/event_manager.c */

/* Removed DISABLED EventAvail stub (real implementation lives in EventManager) */

/* Menu and Window functions provided by their respective managers:
 * MenuSelect - MenuSelection.c
 * SystemClick - DeskManagerCore.c
 * HiliteMenu - MenuManagerCore.c
 * FrontWindow - WindowDisplay.c
 */

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

/* CleanUpWindow moved to Finder/finder_main.c */

/* ParamText provided by DialogManagerCore.c */

/* Alert, StopAlert, NoteAlert, CautionAlert now provided by AlertDialogs.c */

/* TickCount() now implemented in TimeManager/TimeBase.c */

/* GetMenuItemText now implemented in MenuItems.c */

/* OpenDeskAcc provided by DeskManagerCore.c */

/* HiWord and LoWord moved to System71StdLib.c */

/* InvalRect removed - implemented in WindowManager/WindowEvents.c:348 */

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

/* Standard library functions moved to System71StdLib.c:
 * sprintf, __assert_fail, strlen, abs
 * HiWord, LoWord, BlockMoveData - moved to System71StdLib.c
 */

/* Minimal math functions for -nostdlib build */

/* Forward declarations */
double fabs(double x);

/* Helper function for atan2 */
static double _atan_approx(double z) {
    /* Minimax polynomial approximation for atan(z) where |z| <= 1
     * Accurate to ~6 decimal places */
    double z2 = z * z;
    return z * (0.99866 + z2 * (-0.33015 + z2 * (0.18014 + z2 * (-0.08467 + z2 * 0.02487))));
}

static double __attribute__((unused)) _sqrt_approx(double x) {
    if (x <= 0.0) return 0.0;
    /* Newton-Raphson approximation */
    double guess = x;
    for (int i = 0; i < 10; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

double atan2(double y, double x) {
    /* Two-argument arctangent with proper quadrant handling
     * Returns angle in radians from -π to π
     * Used for converting Cartesian to polar coordinates */

    const double PI = 3.14159265358979323846;
    const double PI_2 = 1.57079632679489661923;

    /* Handle special cases */
    if (x == 0.0 && y == 0.0) return 0.0;
    if (x == 0.0) return (y > 0.0) ? PI_2 : -PI_2;

    /* Compute atan(y/x) with range reduction */
    double ax = fabs(x);
    double ay = fabs(y);
    double z, angle;

    if (ay <= ax) {
        z = ay / ax;
        angle = _atan_approx(z);
    } else {
        z = ax / ay;
        angle = PI_2 - _atan_approx(z);
    }

    /* Adjust for quadrant */
    if (x < 0.0) {
        angle = (y >= 0.0) ? (PI - angle) : (-PI + angle);
    } else {
        angle = (y >= 0.0) ? angle : -angle;
    }

    return angle;
}

double cos(double x) {
    /* Cosine using Taylor series: 1 - x²/2! + x⁴/4! - x⁶/6! + x⁸/8!
     * Range reduction to [-π, π] for better accuracy */

    const double PI = 3.14159265358979323846;
    const double TWO_PI = 6.28318530717958647693;

    /* Range reduction: bring x into [-π, π] */
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;

    /* Taylor series computation */
    double x2 = x * x;
    double term = 1.0;
    double sum = 1.0;

    /* Terms: -x²/2!, +x⁴/4!, -x⁶/6!, +x⁸/8! */
    term = -term * x2 / (1.0 * 2.0);
    sum += term;

    term = -term * x2 / (3.0 * 4.0);
    sum += term;

    term = -term * x2 / (5.0 * 6.0);
    sum += term;

    term = -term * x2 / (7.0 * 8.0);
    sum += term;

    return sum;
}

double sin(double x) {
    /* Sine using Taylor series: x - x³/3! + x⁵/5! - x⁷/7! + x⁹/9!
     * Range reduction to [-π, π] for better accuracy */

    const double PI = 3.14159265358979323846;
    const double TWO_PI = 6.28318530717958647693;

    /* Range reduction: bring x into [-π, π] */
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;

    /* Taylor series computation */
    double x2 = x * x;
    double term = x;
    double sum = x;

    /* Terms: -x³/3!, +x⁵/5!, -x⁷/7!, +x⁹/9! */
    term = -term * x2 / (2.0 * 3.0);
    sum += term;

    term = -term * x2 / (4.0 * 5.0);
    sum += term;

    term = -term * x2 / (6.0 * 7.0);
    sum += term;

    term = -term * x2 / (8.0 * 9.0);
    sum += term;

    return sum;
}

double tan(double x) {
    /* Tangent using sin/cos: tan(x) = sin(x) / cos(x)
     * Simple implementation using existing sin and cos functions */
    return sin(x) / cos(x);
}

double atan(double x) {
    /* Arctangent using approximation for general case
     * Uses series expansion in [-1, 1] with range reduction */
    double absX = (x < 0.0) ? -x : x;
    double result = 0.0;

    if (absX <= 1.0) {
        /* Use Taylor series: atan(x) ≈ x - x³/3 + x⁵/5 - x⁷/7 ... */
        double x2 = x * x;
        double numerator = x;
        double denominator = 1.0;

        for (int i = 0; i < 8; i++) {
            result += numerator / denominator;
            numerator *= -x2;
            denominator += 2.0;
        }
    } else {
        /* For |x| > 1: atan(x) = π/2 - atan(1/x) */
        const double PI_2 = 1.57079632679489661923;
        double reciprocal = 1.0 / x;
        double x2 = reciprocal * reciprocal;
        double numerator = reciprocal;
        double denominator = 1.0;

        for (int i = 0; i < 8; i++) {
            result += numerator / denominator;
            numerator *= -x2;
            denominator += 2.0;
        }

        result = (x > 0.0) ? (PI_2 - result) : (-PI_2 - result);
    }

    return result;
}

double asin(double x) {
    /* Arcsine: asin(x) = atan(x / sqrt(1 - x²))
     * Valid for x in [-1, 1] */
    if (x < -1.0 || x > 1.0) return 0.0;  /* Out of domain */
    if (x == 0.0) return 0.0;

    double x2 = x * x;
    double sqrt_term = x2;
    double sqrt_result = 1.0;

    /* Compute sqrt(1 - x²) using Newton's method approximation */
    for (int i = 0; i < 4; i++) {
        sqrt_result = (sqrt_result + (1.0 - sqrt_term) / sqrt_result) / 2.0;
    }

    return atan(x / sqrt_result);
}

/* Suppress missing prototype warnings for stub implementations */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

double acos(double x) {
    /* Arccosine: acos(x) = π/2 - asin(x)
     * Valid for x in [-1, 1] */
    const double PI_2 = 1.57079632679489661923;
    if (x < -1.0 || x > 1.0) return 0.0;  /* Out of domain */
    return PI_2 - asin(x);
}

double exp(double x) {
    /* Exponential function using Taylor series
     * e^x = 1 + x + x²/2! + x³/3! + x⁴/4! + ... */
    double result = 1.0;
    double term = 1.0;

    for (int i = 1; i <= 20; i++) {
        term = term * x / (double)i;
        result += term;

        /* Early termination if term becomes negligible */
        if (term > -1e-15 && term < 1e-15) break;
    }

    return result;
}

double log(double x) {
    /* Natural logarithm using range reduction and series
     * For x in (0, 2]: ln(x) = 2 * Σ((x-1)/(x+1))^(2n+1) / (2n+1) */
    if (x <= 0.0) return 0.0;  /* Undefined for x <= 0 */
    if (x == 1.0) return 0.0;

    /* Range reduction: express x as 2^n * y where y in [1, 2) */
    int exponent = 0;
    double y = x;
    while (y >= 2.0) {
        y /= 2.0;
        exponent++;
    }
    while (y < 1.0) {
        y *= 2.0;
        exponent--;
    }

    /* Series computation for ln(y) */
    double z = (y - 1.0) / (y + 1.0);
    double z2 = z * z;
    double result = 0.0;
    double term = z;

    for (int i = 0; i < 20; i++) {
        result += term / (2.0 * (double)i + 1.0);
        term *= z2;
    }

    result *= 2.0;

    /* Add log(2) * exponent contribution */
    const double LN2 = 0.693147180559945309417;
    result += (double)exponent * LN2;

    return result;
}

double log10(double x) {
    /* Base-10 logarithm: log10(x) = ln(x) / ln(10) */
    const double LN10 = 2.302585092994045684017;
    if (x <= 0.0) return 0.0;
    return log(x) / LN10;
}

double fabs(double x) {
    /* Absolute value for floating point numbers
     * Returns the magnitude of x without sign */
    return (x < 0.0) ? -x : x;
}

double floor(double x) {
    /* Round down to nearest integer (towards negative infinity)
     * Examples: floor(2.7) = 2.0, floor(-2.7) = -3.0 */

    if (x >= 0.0) {
        /* Positive: truncate fractional part */
        return (double)(long long)x;
    } else {
        /* Negative: truncate and subtract 1 if there was a fraction */
        long long truncated = (long long)x;
        if (x == (double)truncated) {
            return x;  /* Already integer */
        }
        return (double)(truncated - 1);
    }
}

/* Bare-metal stubs for missing system functions */

/* File I/O stubs */
void* fopen(const char *filename, const char *mode) {
    /* Stub: File operations not supported in bare-metal */
    return NULL;
}

int fclose(void *stream) {
    /* Stub: File operations not supported */
    return -1;
}

int fseek(void *stream, long offset, int whence) {
    /* Stub: File operations not supported */
    return -1;
}

long ftell(void *stream) {
    /* Stub: File operations not supported */
    return -1;
}

size_t fread(void *ptr, size_t size, size_t nmemb, void *stream) {
    /* Stub: File operations not supported */
    return 0;
}

int fprintf(void *stream, const char *format, ...) {
    /* Stub: File operations not supported */
    return -1;
}

char* fgets(char *s, int size, void *stream) {
    /* Stub: File operations not supported */
    return NULL;
}

/* Suppress missing prototype warnings for stub implementations */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

/* Memory handle stubs */
UInt8 HGetState(Handle h) {
    /* Stub: Return default state (unlocked, unpurgeable) */
    return 0;
}

void HSetState(Handle h, UInt8 state) {
    /* Stub: Ignore handle state changes */
    return;
}

/* HFS B-tree stubs */
OSErr HFS_BT_FindRecord(void *btree, const void *key, void *record, UInt32 *recLen) {
    /* Stub: HFS B-tree operations not available */
    return -1;
}

/* Trap dispatcher stub */
int TrapDispatcher_SetTrapAddress(UInt16 trap_number, UInt16 trap_word, void *handler) {
    /* Stub: Trap patching not available in bare-metal */
    return -1;
}

/* Dialog and resource loading stubs */
int InitDialogResources(void) {
    return 0;
}

void* LoadDialogTemplate(int dialogID) {
    return NULL;
}

void* LoadDialogItemList(void *template) {
    return NULL;
}

void DisposeDialogTemplate(void *template) {
    return;
}

void DisposeDialogItemList(void *items) {
    return;
}

void* LoadAlertTemplate(int alertID) {
    return NULL;
}

void DisposeAlertTemplate(void *template) {
    return;
}

/* isnan stub */
int isnan(double x) {
    /* Check for NaN by comparing x with itself */
    return (x != x) ? 1 : 0;
}

double ceil(double x) {
    /* Round up to nearest integer (towards positive infinity)
     * Examples: ceil(2.3) = 3.0, ceil(-2.3) = -2.0 */

    if (x < 0.0) {
        /* Negative: truncate fractional part */
        return (double)(long long)x;
    } else {
        /* Positive: truncate and add 1 if there was a fraction */
        long long truncated = (long long)x;
        if (x == (double)truncated) {
            return x;  /* Already integer */
        }
        return (double)(truncated + 1);
    }
}

double fmod(double x, double y) {
    /* Floating-point remainder of x/y
     * Returns x - n*y where n = floor(x/y)
     * Sign of result matches sign of x */

    if (y == 0.0) return 0.0;  /* Avoid division by zero */

    double quotient = x / y;
    long long n = (long long)quotient;

    /* For negative quotients, adjust n to match floor behavior */
    if (quotient < 0.0 && quotient != (double)n) {
        n--;
    }

    return x - (double)n * y;
}

double pow(double base, double exponent) {
    /* Power function: base^exponent
     * Simplified implementation for integer exponents */

    if (exponent == 0.0) return 1.0;
    if (base == 0.0) return 0.0;
    if (exponent == 1.0) return base;

    /* Handle negative exponents */
    int isNegExp = (exponent < 0.0);
    if (isNegExp) {
        exponent = -exponent;
    }

    /* Check if exponent is integer */
    long long intExp = (long long)exponent;
    if (exponent != (double)intExp) {
        /* Non-integer exponents: use exp(exponent * log(base))
         * For simplicity, only handle integer exponents */
        return 1.0;  /* Placeholder for non-integer powers */
    }

    /* Integer exponent: use repeated multiplication */
    double result = 1.0;
    double currentPower = base;

    while (intExp > 0) {
        if (intExp & 1) {
            result *= currentPower;
        }
        currentPower *= currentPower;
        intExp >>= 1;
    }

    if (isNegExp) {
        return 1.0 / result;
    }
    return result;
}

double round(double x) {
    /* Round to nearest integer, halfway cases away from zero
     * Examples: round(2.3) = 2.0, round(2.5) = 3.0, round(-2.5) = -3.0 */
    if (x >= 0.0) {
        return floor(x + 0.5);
    } else {
        return ceil(x - 0.5);
    }
}

double trunc(double x) {
    /* Round towards zero (remove fractional part)
     * Examples: trunc(2.7) = 2.0, trunc(-2.7) = -2.0 */
    return (x >= 0.0) ? floor(x) : ceil(x);
}

int signum(int x) {
    /* Return sign of integer: -1, 0, or 1 */
    return (x > 0) - (x < 0);
}

double fsignum(double x) {
    /* Return sign of double: -1.0, 0.0, or 1.0 */
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return 0.0;
}

/* Restore diagnostic state */
#pragma GCC diagnostic pop

/* 64-bit division for -nostdlib build */
long long __divdi3(long long a, long long b) {
    /* Binary long division algorithm for 64-bit signed integers
     * Much faster than repeated subtraction: O(log n) vs O(n)
     * Handles negative operands using two's complement arithmetic
     *
     * Algorithm:
     * 1. Handle signs: track if result should be negative
     * 2. Work with absolute values
     * 3. Binary long division from MSB to LSB
     * 4. Apply sign to result
     */

    if (b == 0) return 0;  /* Division by zero */

    /* Handle signs */
    int result_negative = 0;
    unsigned long long ua, ub;

    if (a < 0) {
        ua = (unsigned long long)(-a);
        result_negative = !result_negative;
    } else {
        ua = (unsigned long long)a;
    }

    if (b < 0) {
        ub = (unsigned long long)(-b);
        result_negative = !result_negative;
    } else {
        ub = (unsigned long long)b;
    }

    /* Binary long division */
    unsigned long long quotient = 0;
    unsigned long long remainder = 0;

    /* Process each bit from MSB to LSB */
    for (int i = 63; i >= 0; i--) {
        /* Shift remainder left by 1 and bring down next bit of dividend */
        remainder <<= 1;
        if (ua & (1ULL << i)) {
            remainder |= 1;
        }

        /* If remainder >= divisor, subtract and set quotient bit */
        if (remainder >= ub) {
            remainder -= ub;
            quotient |= (1ULL << i);
        }
    }

    /* Apply sign */
    if (result_negative) {
        return -(long long)quotient;
    } else {
        return (long long)quotient;
    }
}

/* Standard library minimal implementations */
#include <stddef.h>

/* Memory and string functions moved to System71StdLib.c:
 * memcpy, memset, memcmp, memmove, strncpy, snprintf
 *
 * Memory allocation handled by Memory Manager:
 * malloc, calloc, realloc, free
 */

/* External globals from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* External QuickDraw globals */
extern QDGlobals qd;
extern void* framebuffer;

/* WM_Update, WM_InvalidateDisplay, SetDeskHook, and g_deskHook moved to WindowManager/WindowDisplay.c */

/* [WM-050] Stub quarantine: real BeginUpdate/EndUpdate in WindowEvents.c */
/* BeginUpdate and EndUpdate removed - implemented in WindowManager/WindowEvents.c
 * - BeginUpdate at line 499
 * - EndUpdate at line 650
 */

/* SetDeskHook moved to WindowManager/WindowDisplay.c */

/* [WM-053] QuickDraw region drawing functions
 * Removed stubs (now disabled by default via SYS71_STUBS_DISABLED=1):
 * - FillRgn - implemented in QuickDraw/Regions.c:621
 * - RectInRgn - implemented in QuickDraw/Regions.c:557
 */

/* sqrt() moved to System71StdLib.c */
/* QDPlatform_DrawRegion() moved to QuickDraw/QuickDrawPlatform.c */
/* QuickDraw text stubs for About box */
/* Moved to FontManagerCore.c
void TextSize(short size) {
    // Stub - would set text size in current GrafPort
}
void TextFont(short font) {
    // Stub - would set text font in current GrafPort
}

void TextFace(short face) {
    // Stub - would set text face (bold, italic, etc.) in current GrafPort
}
*/


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

    UInt32 startTicks = TickCount();
    UInt32 targetTicks = startTicks + numTicks;
    UInt32 iterations = 0;
    const UInt32 MAX_ITERATIONS = numTicks * 1000;  /* Safety timeout: ~1000 iterations per tick */

    /* Wait until target tick count reached */
    while (TickCount() < targetTicks && iterations < MAX_ITERATIONS) {
        /* Call SystemTask to allow DAs to run during delay */
        SystemTask();
        iterations++;

        /* If TickCount isn't advancing after many iterations, break out */
        if (iterations > 100 && TickCount() == startTicks) {
            /* Timer not working - just exit to prevent hang */
            extern void serial_printf(const char* fmt, ...);
            serial_printf("[Delay] WARNING: TickCount not advancing, exiting early (iterations=%u)\n", iterations);
            break;
        }
    }

    if (finalTicks) {
        *finalTicks = TickCount();
    }
}

/*
 * InitRegistryIfNeeded - Initialize internal dialog-list registry
 *
 * Called by InitListManager() during List Manager initialization.
 * Sets up internal structures for dialog-attached lists.
 *
 * This is a minimal stub for bare-metal environment.
 */
void InitRegistryIfNeeded(void) {
    /* Minimal implementation - just returns as system is bootstrapped */
}

/*
 * ShowGetInfo - Display Get Info dialog for file items
 *
 * Called from HandleGetInfo() to show the Get Info dialog.
 * Takes an array of FSSpec items and displays their information.
 *
 * Parameters:
 * - items: Array of FSSpec for selected items
 * - count: Number of items in array
 *
 * Returns:
 * - noErr on success
 * - Error code on failure
 *
 * This is a minimal stub for bare-metal environment.
 */
OSErr ShowGetInfo(FSSpec *items, short count) {
    /* Minimal implementation - just returns success */
    if (!items || count <= 0) {
        return paramErr;
    }
    return noErr;
}

/*
 * InitEvents - Initialize the Event Manager
 *
 * Sets up the event queue and event handling infrastructure.
 * Called during system startup to prepare event processing.
 *
 * Parameters:
 * - numEvents: Requested size of event queue
 *
 * Returns:
 * - 0 on success
 * - Negative value on failure
 *
 * This is a minimal stub for bare-metal environment.
 */
SInt16 InitEvents(SInt16 numEvents) {
    /* Minimal implementation - event system is initialized elsewhere */
    return 0;
}

/*
 * ARM64 Boot Functions - Stubs for bare-metal environment
 */

/* Timer initialization for ARM64 */
void timer_init(void) {
    /* Minimal stub - timer setup deferred to actual platform implementation */
}

/* Exception handler initialization for ARM64 */
void exceptions_init(void) {
    /* Minimal stub - exception handlers setup deferred to actual platform implementation */
}

/* Device Tree Blob initialization - ARM64 has its own implementation */
#ifndef QEMU_BUILD
bool dtb_init(void *dtb) {
    (void)dtb;
    /* Minimal stub - DTB parsing deferred to actual platform implementation */
    return false;
}
#endif

/* MMU (Memory Management Unit) initialization - ARM64 has its own implementation */
#ifndef QEMU_BUILD
bool mmu_init(void) {
    /* Minimal stub - MMU setup deferred to actual platform implementation */
    return false;
}
#endif

/* MMU enable/activation - ARM64 has its own implementation */
#ifndef QEMU_BUILD
void mmu_enable(void) {
    /* Minimal stub - MMU activation deferred to actual platform implementation */
}
#endif

/*
 * InitControlManager_Sys7 - Initialize Control Manager for System 7.1
 *
 * Sets up the control manager subsystem for UI controls (buttons, scroll bars, etc.).
 * Called during system initialization.
 *
 * This is a minimal stub for bare-metal environment.
 */
void InitControlManager_Sys7(void) {
    /* Minimal implementation - control manager is initialized elsewhere */
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
