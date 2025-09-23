/* System 7.1 Stubs for linking */
#include "../include/MacTypes.h"
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

/* DeskHook type definition if not in headers */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);

/* Platform Menu System stubs */
void Platform_InitMenuSystem(void) {
    /* Platform-specific menu initialization */
}

void Platform_CleanupMenuSystem(void) {
    /* Platform-specific menu cleanup */
}

/* Resource Manager */
void InitResourceManager(void) {
    /* Stub implementation */
}

/* QuickDraw - InitGraf is now provided by QuickDrawCore.c */

void InitCursor(void) {
    /* Stub implementation */
}

/* Font Manager */
void InitFonts(void) {
    /* Stub implementation */
}

/* Window Manager functions provided by WindowManagerCore.c and WindowDisplay.c */

void DrawControls(WindowPtr window) {
    /* Stub implementation */
}

/* Menu Manager - Most functions now provided by MenuManagerCore.c */

void AppendMenu(MenuHandle menu, ConstStr255Param data) {
    /* Stub implementation */
}

/* TextEdit */
void InitTE(void) {
    /* Stub implementation */
}

/* Dialog Manager - provided by DialogManagerCore.c */

/* Control Manager */
void InitControlManager_Sys7(void) {
    /* Stub implementation */
}

/* List Manager */
void InitListManager(void) {
    /* Stub implementation */
}

/* Event Manager */
SInt16 InitEvents(SInt16 numEvents) {
    return 0;  /* Success */
}

/* Simple event queue implementation */
#define MAX_EVENTS 32
static struct {
    EventRecord events[MAX_EVENTS];
    int head;
    int tail;
    int count;
} g_eventQueue = {0};

Boolean GetNextEvent(short eventMask, EventRecord* theEvent) {
    extern void serial_printf(const char* fmt, ...);

    /* Debug: log every call */
    serial_printf("GetNextEvent: Called with mask=0x%04x, queue count=%d\n",
                  eventMask, g_eventQueue.count);

    /* Check if queue has events */
    if (g_eventQueue.count == 0) {
        serial_printf("GetNextEvent: Queue empty, returning false\n");
        return false;
    }

    /* Find the next event matching the mask */
    int index = g_eventQueue.head;
    int checked = 0;

    while (checked < g_eventQueue.count) {
        EventRecord* evt = &g_eventQueue.events[index];

        /* Check if event matches mask */
        if ((1 << evt->what) & eventMask) {
            /* Debug: Show what we're retrieving */
            serial_printf("GetNextEvent: Found matching event type=%d at index=%d\n", evt->what, index);
            serial_printf("GetNextEvent: Event where={v=%d,h=%d}, msg=0x%08x\n",
                         evt->where.v, evt->where.h, evt->message);

            /* Copy event to caller */
            if (theEvent) {
                *theEvent = *evt;
                serial_printf("GetNextEvent: Copied to caller, where={v=%d,h=%d}\n",
                             theEvent->where.v, theEvent->where.h);
            }

            /* Remove event from queue */
            if (index == g_eventQueue.head) {
                /* Easy case - removing from head */
                g_eventQueue.head = (g_eventQueue.head + 1) % MAX_EVENTS;
                g_eventQueue.count--;
            } else {
                /* Need to shift events to fill the gap */
                int next = (index + 1) % MAX_EVENTS;
                while (next != (g_eventQueue.tail)) {
                    g_eventQueue.events[index] = g_eventQueue.events[next];
                    index = next;
                    next = (next + 1) % MAX_EVENTS;
                }
                g_eventQueue.tail = (g_eventQueue.tail - 1 + MAX_EVENTS) % MAX_EVENTS;
                g_eventQueue.count--;
            }

            extern void serial_printf(const char* fmt, ...);
            if (evt->what == mouseDown) {
                serial_printf("GetNextEvent: Returning mouseDown at (%d,%d)\n",
                             theEvent->where.h, theEvent->where.v);
            }

            return true;
        }

        index = (index + 1) % MAX_EVENTS;
        checked++;
    }

    return false;
}

SInt16 PostEvent(SInt16 eventNum, SInt32 eventMsg) {
    extern void serial_printf(const char* fmt, ...);
    extern void GetMouse(Point* mouseLoc);

    serial_printf("PostEvent: eventNum=%d, eventMsg=0x%08x\n", eventNum, eventMsg);

    /* Check if queue is full */
    if (g_eventQueue.count >= MAX_EVENTS) {
        serial_printf("PostEvent: Event queue full!\n");
        return queueFull;
    }

    /* Add event to queue */
    EventRecord* evt = &g_eventQueue.events[g_eventQueue.tail];
    evt->what = eventNum;
    evt->message = eventMsg;
    evt->when = TickCount();

    /* Get current mouse position for all events */
    GetMouse(&evt->where);

    /* For mouse events, message contains additional data like click count */
    if (eventNum == mouseDown || eventNum == mouseUp) {
        /* Message high word contains click count for mouse down */
        /* Message low word may contain button/modifier info */
        serial_printf("PostEvent: Mouse event with message=0x%08x at (%d,%d)\n",
                     eventMsg, evt->where.h, evt->where.v);
    }

    evt->modifiers = 0;  /* TODO: Get keyboard modifiers */

    /* Debug: Print actual coordinates we're storing */
    serial_printf("PostEvent: Stored event with where=(%d,%d)\n",
                 evt->where.h, evt->where.v);

    g_eventQueue.tail = (g_eventQueue.tail + 1) % MAX_EVENTS;
    g_eventQueue.count++;

    if (eventNum == mouseDown) {
        serial_printf("PostEvent: Added mouseDown at (%d,%d) to queue (count=%d)\n",
                     evt->where.h, evt->where.v, g_eventQueue.count);
    }

    return noErr;
}

/* GenerateSystemEvent - forward declaration to avoid header conflicts */
void GenerateSystemEvent(short eventType, int message, Point where, short modifiers);
void GenerateSystemEvent(short eventType, int message, Point where, short modifiers) {
    /* Generate and post a system event */
    PostEvent(eventType, message);
}

/* SystemTask provided by DeskManagerCore.c */

/* ExpandMem stubs for SystemInit */
void ExpandMemInit(void) {
    serial_printf("ExpandMemInit: Initializing expanded memory\n");
    /* Set up expanded memory globals if needed */
}
void ExpandMemInitKeyboard(void) {
    serial_printf("ExpandMemInitKeyboard: Initializing keyboard expanded memory\n");
    /* Initialize keyboard-specific memory areas */
}
void ExpandMemSetAppleTalkInactive(void) {
    serial_printf("ExpandMemSetAppleTalkInactive: Disabling AppleTalk\n");
    /* Mark AppleTalk as inactive in expanded memory */
}
void SetAutoDecompression(Boolean enable) {
    serial_printf("SetAutoDecompression: %s\n", enable ? "Enabled" : "Disabled");
    /* Set resource decompression flag */
}
void ResourceManager_SetDecompressionCacheSize(Size size) {
    serial_printf("ResourceManager_SetDecompressionCacheSize: Setting cache to %lu bytes\n", (unsigned long)size);
    /* Configure decompression cache */
}
void InstallDecompressHook(DecompressHookProc proc) {
    serial_printf("InstallDecompressHook: Installing hook at %p\n", proc);
    /* Install custom decompression routine */
}
void ExpandMemInstallDecompressor(void) {
    serial_printf("ExpandMemInstallDecompressor: Installing default decompressor\n");
    /* Install default resource decompressor */
}
void ExpandMemCleanup(void) {
    serial_printf("ExpandMemCleanup: Cleaning up expanded memory\n");
    /* Release expanded memory resources */
}
void ExpandMemDump(void) {
    serial_printf("ExpandMemDump: Dumping expanded memory state\n");
    /* Debug dump of expanded memory contents */
}
Boolean ExpandMemValidate(void) { return true; }

/* Serial stubs */
#include <stdarg.h>

/* serial_printf moved to System71StdLib.c */

/* Finder InitializeFinder provided by finder_main.c */

/* QuickDraw globals - defined in main.c */
extern QDGlobals qd;

void FinderEventLoop(void) {
    /* Stub implementation */
}

/* Additional Finder support functions */
/* FlushEvents - clear events from queue */
void FlushEvents(short eventMask, short stopMask) {
    /* Clear matching events from our queue */
    if (eventMask == everyEvent && stopMask == 0) {
        /* Clear all events */
        g_eventQueue.head = 0;
        g_eventQueue.tail = 0;
        g_eventQueue.count = 0;
    }
}

void TEInit(void) {
    /* TextEdit initialization stub */
}

/* Window stubs for functions not yet implemented elsewhere */
void InitWindows(void) {
    serial_printf("InitWindows: Initializing window manager\n");

    /* Set up window list chain */
    extern WindowPtr g_firstWindow;
    g_firstWindow = NULL;

    /* Initialize desktop port */
    extern QDGlobals qd;
    if (qd.thePort == NULL) {
        /* Create default desktop port */
        serial_printf("InitWindows: Creating default desktop port\n");
    }
}
WindowPtr NewWindow(void* storage, const Rect* boundsRect, ConstStr255Param title,
                    Boolean visible, short procID, WindowPtr behind, Boolean goAwayFlag,
                    long refCon) {
    return NULL;
}
void DisposeWindow(WindowPtr window) {
    if (!window) {
        serial_printf("DisposeWindow: NULL window\n");
        return;
    }

    serial_printf("DisposeWindow: Disposing window at %p\n", window);

    /* Remove from window chain */
    extern WindowPtr g_firstWindow;
    if (g_firstWindow == window) {
        g_firstWindow = window->nextWindow;
    } else {
        WindowPtr prev = g_firstWindow;
        while (prev && prev->nextWindow != window) {
            prev = prev->nextWindow;
        }
        if (prev) {
            prev->nextWindow = window->nextWindow;
        }
    }

    /* Free window storage - would normally free memory */
    /* For now just mark it invalid */
    window->windowKind = 0;
}
void DragWindow(WindowPtr window, Point startPt, const Rect* boundsRect) {
    if (!window) {
        serial_printf("DragWindow: NULL window\n");
        return;
    }

    serial_printf("DragWindow: Starting drag at (%d,%d)\n", startPt.h, startPt.v);

    /* Track mouse while button held */
    extern Boolean Button(void);
    extern void GetMouse(Point* mouseLoc);
    extern void PollPS2Input(void);

    Point lastPt = startPt;
    Point currentPt;

    while (Button()) {
        PollPS2Input();
        GetMouse(&currentPt);

        if (currentPt.h != lastPt.h || currentPt.v != lastPt.v) {
            /* Calculate offset */
            short dh = currentPt.h - lastPt.h;
            short dv = currentPt.v - lastPt.v;

            /* Move window */
            window->portRect.left += dh;
            window->portRect.right += dh;
            window->portRect.top += dv;
            window->portRect.bottom += dv;

            /* Constrain to bounds if provided */
            if (boundsRect) {
                if (window->portRect.left < boundsRect->left) {
                    short adjust = boundsRect->left - window->portRect.left;
                    window->portRect.left += adjust;
                    window->portRect.right += adjust;
                }
                if (window->portRect.top < boundsRect->top) {
                    short adjust = boundsRect->top - window->portRect.top;
                    window->portRect.top += adjust;
                    window->portRect.bottom += adjust;
                }
            }

            lastPt = currentPt;

            /* Redraw desktop and window */
            extern void DrawDesktop(void);
            DrawDesktop();
        }

        /* Small delay */
        for (volatile int i = 0; i < 1000; i++) {}
    }

    serial_printf("DragWindow: Ended at (%d,%d)\n", currentPt.h, currentPt.v);
}
void CloseWindow(WindowPtr window) {
    if (!window) {
        serial_printf("CloseWindow: NULL window\n");
        return;
    }

    serial_printf("CloseWindow: Closing window at %p\n", window);

    /* Hide the window */
    window->visible = false;

    /* Generate close event if needed */
    /* DisposeWindow handles actual disposal */
}
void ShowWindow(WindowPtr window) {
    if (!window) {
        serial_printf("ShowWindow: NULL window\n");
        return;
    }

    serial_printf("ShowWindow: Showing window at %p\n", window);

    window->visible = true;

    /* Invalidate window area to force redraw */
    extern void InvalRect(const Rect* rect);
    InvalRect(&window->portRect);
}
void HideWindow(WindowPtr window) {
    if (!window) {
        serial_printf("HideWindow: NULL window\n");
        return;
    }

    serial_printf("HideWindow: Hiding window at %p\n", window);

    window->visible = false;

    /* Invalidate area behind window */
    extern void InvalRect(const Rect* rect);
    InvalRect(&window->portRect);
}
void SelectWindow(WindowPtr window) {
    if (!window) {
        serial_printf("SelectWindow: NULL window\n");
        return;
    }

    serial_printf("SelectWindow: Selecting window at %p\n", window);

    /* Move window to front of chain */
    extern WindowPtr g_firstWindow;
    if (g_firstWindow != window) {
        /* Remove from current position */
        WindowPtr prev = g_firstWindow;
        while (prev && prev->nextWindow != window) {
            prev = prev->nextWindow;
        }
        if (prev) {
            prev->nextWindow = window->nextWindow;
        }

        /* Insert at front */
        window->nextWindow = g_firstWindow;
        g_firstWindow = window;
    }

    /* Set as active window */
    window->hilited = true;
}
WindowPtr FrontWindow(void) { return NULL; }
void SetWTitle(WindowPtr window, ConstStr255Param title) {
    if (!window) {
        serial_printf("SetWTitle: NULL window\n");
        return;
    }

    if (!title) {
        serial_printf("SetWTitle: NULL title\n");
        return;
    }

    /* Copy title string - Pascal string with length byte */
    unsigned char len = title[0];
    if (len > 255) len = 255;

    serial_printf("SetWTitle: Setting window title length %d\n", len);

    /* Store in window title field - would copy to window's title storage */
    /* For now just log it */
    char titleBuf[256];
    for (int i = 0; i < len; i++) {
        titleBuf[i] = title[i+1];
    }
    titleBuf[len] = 0;
    serial_printf("SetWTitle: Title = '%s'\n", titleBuf);

    /* Invalidate title bar area */
    Rect titleBar = window->portRect;
    titleBar.bottom = titleBar.top + 20;  /* Standard title bar height */
    extern void InvalRect(const Rect* rect);
    InvalRect(&titleBar);
}
void DrawGrowIcon(WindowPtr window) {
    if (!window) {
        serial_printf("DrawGrowIcon: NULL window\n");
        return;
    }

    if (!window->visible) {
        return;
    }

    serial_printf("DrawGrowIcon: Drawing grow icon for window at %p\n", window);

    /* Draw grow icon in bottom-right corner */
    Rect growRect;
    growRect.right = window->portRect.right;
    growRect.bottom = window->portRect.bottom;
    growRect.left = growRect.right - 15;
    growRect.top = growRect.bottom - 15;

    /* Would draw grow box pattern here */
    extern void FrameRect(const Rect* r);
    FrameRect(&growRect);
}
void WM_UpdateWindowVisibility(WindowPtr window) {
    if (!window) {
        serial_printf("WM_UpdateWindowVisibility: NULL window\n");
        return;
    }

    serial_printf("WM_UpdateWindowVisibility: Updating visibility for window at %p\n", window);

    /* Update window visibility state */
    if (window->visible) {
        /* Ensure window is drawn */
        extern void InvalRect(const Rect* rect);
        InvalRect(&window->portRect);
    }
}

short FindWindow(Point thePt, WindowPtr *window) {
    extern void serial_printf(const char* fmt, ...);

    if (window) {
        *window = NULL;
    }

    /* Check if click is in menu bar (top 20 pixels) */
    if (thePt.v >= 0 && thePt.v < 20) {
        serial_printf("FindWindow: Click in menu bar at v=%d\n", thePt.v);
        return inMenuBar;
    }

    /* TODO: Check for actual window hits when window manager is fully implemented */

    /* Default to desktop */
    return inDesk;
}

/* DragWindow is implemented in WindowManager/WindowDragging.c
void DragWindow(WindowPtr window, Point startPt, const Rect* limitRect) {
    // Stub - would handle window dragging
}
*/

/* Functions provided by other components:
 * ShowErrorDialog - finder_main.c
 * CleanUpDesktop - desktop_manager.c
 * DrawDesktop - desktop_manager.c
 */

/* System stubs */
long sysconf(int name) { return -1; }

/* Resource Manager functions provided by Memory Manager */

/* Missing function implementations */
void HandleKeyDown(EventRecord* event) {
    /* Stub */
}

OSErr ResolveAliasFile(const FSSpec* spec, FSSpec* target, Boolean* wasAliased, Boolean* wasFolder) {
    if (target) *target = *spec;
    if (wasAliased) *wasAliased = false;
    if (wasFolder) *wasFolder = false;
    return noErr;
}

void ReleaseResource(Handle theResource) {
    /* Stub */
}

OSErr NewAlias(const FSSpec* fromFile, const FSSpec* target, AliasHandle* alias) {
    if (alias) *alias = (AliasHandle)NewHandle(sizeof(AliasRecord));
    return noErr;
}

/* GetHandleSize provided by Memory Manager */

OSErr FSpCreateResFile(const FSSpec* spec, OSType creator, OSType fileType, SInt16 scriptTag) {
    return noErr;
}

/* File Manager stubs - Core functions now implemented in FileManager.c */

OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, SInt16 scriptTag) {
    return noErr;
}

OSErr FSpOpenDF(const FSSpec* spec, SInt16 permission, SInt16* refNum) {
    if (refNum) *refNum = 1;
    return noErr;
}

OSErr FSpOpenResFile(const FSSpec* spec, SInt16 permission) {
    return 1; /* Return fake resource file ref */
}

OSErr FSpDelete(const FSSpec* spec) {
    return noErr;
}

OSErr FSpDirDelete(const FSSpec* spec) {
    return noErr;
}

OSErr FSpCatMove(const FSSpec* source, const FSSpec* dest) {
    return noErr;
}


OSErr PBHGetVInfoSync(HParamBlockRec* pb) {
    if (pb) {
        pb->u.volumeParam.ioVAlBlkSiz = 512;
        pb->u.volumeParam.ioVNmAlBlks = 800;  /* Simulate 400K disk */
    }
    return noErr;
}


OSErr SetEOF(SInt16 refNum, SInt32 logEOF) {
    return noErr;
}

/* Resource Manager stubs */
/* NewHandle and DisposeHandle provided by Memory Manager */
/* GetResource provided by simple_resource_manager.c */

Handle Get1Resource(ResType theType, SInt16 theID) {
    extern Handle GetResource(ResType type, short id);
    return GetResource(theType, theID);
}

void AddResource(Handle theData, ResType theType, SInt16 theID, ConstStr255Param name) {
    /* Stub */
}

void RemoveResource(Handle theResource) {
    /* Stub */
}

void WriteResource(Handle theResource) {
    /* Stub */
}

void CloseResFile(SInt16 refNum) {
    /* Stub */
}

OSErr ResError(void) {
    return noErr;
}

void AddResMenu(MenuHandle theMenu, ResType theType) {
    /* Stub - would add resources to menu */
}

/* Memory Manager functions provided by Memory Manager */
/* MemError moved to System71StdLib.c */

void BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount) {
    unsigned char* d = destPtr;
    const unsigned char* s = srcPtr;
    while (byteCount--) *d++ = *s++;
}

/* Finder-specific stubs */
OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder, SInt16* foundVRefNum, SInt32* foundDirID) {
    if (foundVRefNum) *foundVRefNum = vRefNum;
    if (foundDirID) *foundDirID = 2;  /* Root directory */
    return noErr;
}

OSErr GenerateUniqueTrashName(Str255 baseName, Str255 uniqueName) {
    if (uniqueName && baseName) {
        BlockMoveData(baseName, uniqueName, baseName[0] + 1);
    }
    return noErr;
}

OSErr InitializeWindowManager(void) {
    return noErr;
}

/* InitializeTrashFolder provided by trash_folder.c */

OSErr HandleGetInfo(void) {
    return noErr;
}

OSErr ShowFind(void) {
    return noErr;
}

OSErr FindAgain(void) {
    return noErr;
}

OSErr ShowAboutFinder(void) {
    return noErr;
}

OSErr HandleContentClick(WindowPtr window, EventRecord* event) {
    return noErr;
}

OSErr HandleGrowWindow(WindowPtr window, EventRecord* event) {
    return noErr;
}

OSErr CloseFinderWindow(WindowPtr window) {
    return noErr;
}

Boolean TrackGoAway(WindowPtr window, Point pt) {
    return false;
}

Boolean TrackBox(WindowPtr window, Point pt, SInt16 part) {
    return false;
}

void ZoomWindow(WindowPtr window, SInt16 part, Boolean front) {
    /* Stub */
}

void DoUpdate(WindowPtr window) {
    /* Stub */
}

void DoActivate(WindowPtr window, Boolean activate) {
    /* Stub */
}

void DoBackgroundTasks(void) {
    /* Stub */
}

/* WaitNextEvent - get next event with sleep support */
Boolean WaitNextEvent(short eventMask, EventRecord* theEvent, UInt32 sleep, RgnHandle mouseRgn) {
    /* For now, just call GetNextEvent */
    return GetNextEvent(eventMask, theEvent);
}

/* Menu and Window functions provided by their respective managers:
 * MenuSelect - MenuSelection.c
 * SystemClick - DeskManagerCore.c
 * HiliteMenu - MenuManagerCore.c
 * FrontWindow - WindowDisplay.c
 */

OSErr ShowConfirmDialog(ConstStr255Param message, Boolean* confirmed) {
    if (confirmed) *confirmed = true;  /* Always confirm for testing */
    return noErr;
}

OSErr CloseAllWindows(void) {
    return noErr;
}

OSErr CleanUpSelection(WindowPtr window) {
    return noErr;
}

OSErr CleanUpBy(WindowPtr window, SInt16 sortType) {
    return noErr;
}

OSErr CleanUpWindow(WindowPtr window, SInt16 cleanupType) {
    return noErr;
}

/* ParamText provided by DialogManagerCore.c */

SInt16 Alert(SInt16 alertID, ModalFilterProcPtr filterProc) {
    return 1;  /* OK button */
}

UInt32 TickCount(void) {
    static UInt32 ticks = 0;
    return ticks++;
}

void GetMenuItemText(MenuHandle menu, SInt16 item, Str255 itemString) {
    if (itemString) {
        itemString[0] = 0;  /* Empty string */
    }
}

/* OpenDeskAcc provided by DeskManagerCore.c */

SInt16 HiWord(SInt32 x) {
    return (x >> 16) & 0xFFFF;
}

SInt16 LoWord(SInt32 x) {
    return x & 0xFFFF;
}

void InvalRect(const Rect* badRect) {
    /* Stub */
}

OSErr ScanDirectoryForDesktopEntries(SInt16 vRefNum, SInt32 dirID, SInt16 databaseRefNum) {
    return noErr;
}

/* Region Manager stubs */
RgnHandle NewRgn(void) {
    static Region dummyRegion;
    static Region* regionPtr = &dummyRegion;
    dummyRegion.rgnSize = sizeof(Region);
    SetRect(&dummyRegion.rgnBBox, 0, 0, 1024, 768);
    return (RgnHandle)&regionPtr;
}

void DisposeRgn(RgnHandle rgn) {
    /* Stub */
}

void RectRgn(RgnHandle rgn, const Rect* r) {
    if (rgn && r && *rgn) {
        (*rgn)->rgnBBox = *r;
    }
}

void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn) {
    if (srcRgn && dstRgn && *srcRgn && *dstRgn) {
        **dstRgn = **srcRgn;
    }
}

void SetEmptyRgn(RgnHandle rgn) {
    if (rgn && *rgn) {
        SetRect(&(*rgn)->rgnBBox, 0, 0, 0, 0);
        (*rgn)->rgnSize = sizeof(Region);
    }
}

/* Standard library functions moved to System71StdLib.c:
 * sprintf, __assert_fail, strlen, abs
 */

/* Minimal math functions for -nostdlib build */
double atan2(double y, double x) {
    /* Very basic approximation - just return 0 for now */
    return 0.0;
}

double cos(double x) {
    /* Very basic approximation - just return 1 for now */
    return 1.0;
}

double sin(double x) {
    /* Very basic approximation - just return 0 for now */
    return 0.0;
}

/* 64-bit division for -nostdlib build */
long long __divdi3(long long a, long long b) {
    /* Simple implementation for positive numbers */
    if (b == 0) return 0;
    if (a < 0 || b < 0) return 0;  /* Don't handle negative for now */

    long long quotient = 0;
    long long remainder = a;

    while (remainder >= b) {
        remainder -= b;
        quotient++;
    }

    return quotient;
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

/* DeskHook support */
static DeskHookProc g_deskHook = NULL;

/* External QuickDraw globals */
extern QDGlobals qd;
extern void* framebuffer;

/* Window Manager update pipeline functions */
void WM_Update(void) {
    /* Create a screen port if qd.thePort is NULL */
    static GrafPort screenPort;
    if (qd.thePort == NULL) {
        /* Initialize the screen port */
        OpenPort(&screenPort);
        screenPort.portBits = qd.screenBits;  /* Use screen bitmap */
        screenPort.portRect = qd.screenBits.bounds;
        qd.thePort = &screenPort;
    }

    /* Use QuickDraw to draw desktop */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(qd.thePort);  /* Draw to screen port */

    /* 1. Draw desktop pattern first */
    Rect desktopRect;
    SetRect(&desktopRect, 0, 20,
            qd.screenBits.bounds.right,
            qd.screenBits.bounds.bottom);
    FillRect(&desktopRect, &qd.gray);  /* Gray desktop pattern */

    /* 2. Draw all visible windows before menu bar */
    /* TODO: Window drawing disabled until WindowManagerState is properly available */
    #if 0
    extern WindowManagerState* GetWindowManagerState(void);
    WindowManagerState* wmState = GetWindowManagerState();

    /* Save pen position before window drawing */
    Point savedPenPos = {0, 0};
    if (qd.thePort) {
        savedPenPos = qd.thePort->pnLoc;
    }

    if (wmState && wmState->windowList) {
        /* Draw windows from back to front */
        WindowPtr window = wmState->windowList;
        WindowPtr* windowStack = NULL;
        int windowCount = 0;

        /* Count windows and build stack */
        while (window) {
            windowCount++;
            window = window->nextWindow;
        }

        if (windowCount > 0) {
            windowStack = (WindowPtr*)malloc(windowCount * sizeof(WindowPtr));
            if (windowStack) {
                /* Fill stack */
                window = wmState->windowList;
                for (int i = 0; i < windowCount && window; i++) {
                    windowStack[i] = window;
                    window = window->nextWindow;
                }

                /* Draw windows from back to front */
                for (int i = windowCount - 1; i >= 0; i--) {
                    WindowPtr w = windowStack[i];
                    if (w && w->visible) {
                        /* Draw window frame */
                        Rect frameRect = w->port.portRect;

                        /* Draw window shadow */
                        Rect shadowRect = frameRect;
                        OffsetRect(&shadowRect, 2, 2);
                        PenPat(&qd.black);
                        FrameRect(&shadowRect);

                        /* Draw window background */
                        PenPat(&qd.white);
                        PaintRect(&frameRect);

                        /* Draw window frame */
                        PenPat(&qd.black);
                        FrameRect(&frameRect);

                        /* Draw title bar */
                        Rect titleBar = frameRect;
                        titleBar.bottom = titleBar.top + 20;

                        if (w->hilited) {
                            /* Active window - draw with stripes */
                            for (int y = titleBar.top; y < titleBar.bottom; y += 2) {
                                MoveTo(titleBar.left, y);
                                LineTo(titleBar.right - 1, y);
                            }
                        } else {
                            /* Inactive window - solid gray */
                            FillRect(&titleBar, &qd.gray);
                        }

                        /* Draw window title if present */
                        if (w->titleHandle && *(w->titleHandle)) {
                            /* Save and restore pen position for title drawing */
                            Point titlePenPos = qd.thePort ? qd.thePort->pnLoc : (Point){0, 0};
                            MoveTo(titleBar.left + 25, titleBar.top + 14);
                            DrawString(*(w->titleHandle));
                            if (qd.thePort) {
                                qd.thePort->pnLoc = titlePenPos;
                            }
                        }

                        /* Draw close box if present */
                        if (w->goAwayFlag) {
                            Rect closeBox;
                            closeBox.left = frameRect.left + 2;
                            closeBox.top = frameRect.top + 2;
                            closeBox.right = closeBox.left + 16;
                            closeBox.bottom = closeBox.top + 16;
                            FrameRect(&closeBox);
                        }
                    }
                }

                free(windowStack);
            }
        }
    }

    /* Restore pen position after window drawing */
    if (qd.thePort) {
        qd.thePort->pnLoc = savedPenPos;
    }
    #endif /* Window drawing disabled */

    /* 3. Call DeskHook if registered for icons */
    if (g_deskHook) {
        /* Create a region for the desktop */
        RgnHandle desktopRgn = NewRgn();
        RectRgn(desktopRgn, &desktopRect);
        g_deskHook(desktopRgn);
        DisposeRgn(desktopRgn);
    }

    /* Draw menu bar LAST to ensure clean pen position */
    MoveTo(0, 0);  /* Reset pen position before drawing menu bar */
    DrawMenuBar();  /* Menu Manager draws the menu bar */

    /* Mouse cursor is now drawn separately in main.c for better performance */
    /* Old cursor drawing code removed to prevent double cursor issue */

    SetPort(savePort);
}

void BeginUpdate(WindowPtr theWindow) {
    /* Stub - would save port and set up clipping */
}

void EndUpdate(WindowPtr theWindow) {
    /* Stub - would restore port and clear update region */
}

void SetDeskHook(DeskHookProc proc) {
    g_deskHook = proc;
}

/* QuickDraw region drawing functions */
void FillRgn(RgnHandle rgn, ConstPatternParam pat) {
    /* Stub - would fill region with pattern */
}

Boolean RectInRgn(const Rect *r, RgnHandle rgn) {
    /* Stub - check if rectangle intersects region */
    return true;
}