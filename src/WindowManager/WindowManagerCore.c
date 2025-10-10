/* #include "SystemTypes.h" */
#include <stdlib.h>
/* #include <stdio.h> - removed for bare metal compatibility */
/*
 * WindowManagerCore.c - Core Window Manager Implementation
 *
 * This file implements the essential window creation, disposal, and management
 * functions of the Apple Macintosh System 7.1 Window Manager. This is the
 * foundation that all Mac applications depend on for window functionality.
 *
 * Key functions implemented:
 * - Window Manager initialization
 * - Window creation (NewWindow, NewCWindow, GetNewWindow)
 * - Window disposal (CloseWindow, DisposeWindow)
 * - Window list management
 * - Auxiliary window record management
 * - Basic window properties and reference constants
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WindowKinds.h"
#include "WindowManager/LayoutGuards.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "DialogManager/DialogManager.h"
#include "WindowManager/WMLogging.h"

/* ============================================================================
 * Global Window Manager State
 * ============================================================================ */

/* Single global instance of Window Manager state */
static WindowManagerState g_wmState = {
    NULL,       /* wMgrPort */
    NULL,       /* wMgrCPort */
    NULL,       /* windowList */
    NULL,       /* activeWindow */
    NULL,       /* auxWinHead */
    {{0}},      /* desktopPattern */
    NULL,       /* desktopPixPat */
    1000,       /* nextWindowID */
    false,      /* colorQDAvailable */
    false,      /* initialized */
    NULL,       /* platformData */
    {{0}},      /* port */
    NULL,       /* ghostWindow */
    20,         /* menuBarHeight */
    NULL,       /* grayRgn */
    {{0}},      /* deskPattern */
    false,      /* isDragging */
    {0, 0},     /* dragOffset */
    false       /* isGrowing */
};

/* Focus suspend/restore for window activation */
static ControlHandle s_lastFocus = NULL;

/* ============================================================================
 * Internal Helper Function Declarations
 * ============================================================================ */

static void InitializeWMgrPort(void);
static void InitializeDesktopPattern(void);
static WindowPtr AllocateWindowRecord(Boolean isColorWindow);
static void DeallocateWindowRecord(WindowPtr window);
static void InitializeWindowRecord(WindowPtr window, const Rect* bounds,
                                 ConstStr255Param title, short procID,
                                 Boolean visible, Boolean goAwayFlag);
static void AddWindowToList(WindowPtr window, WindowPtr behind);
static void RemoveWindowFromList(WindowPtr window);
static AuxWinHandle CreateAuxiliaryWindowRecord(WindowPtr owner);
static void DisposeAuxiliaryWindowRecord(AuxWinHandle auxWin);
/* Pascal string helpers - using the one from SuperCompat.h */
#define WM_CopyPascalString CopyPascalString

/* ============================================================================
 * Window Manager Initialization
 * ============================================================================ */

void InitWindows(void) {
    if (g_wmState.initialized) {
        return; /* Already initialized */
    }

    /* Initialize platform windowing system */
    Platform_InitWindowing();

    /* Initialize Window Manager port */
    InitializeWMgrPort();

    /* Set up desktop pattern */
    InitializeDesktopPattern();

    /* Initialize window list */
    g_wmState.windowList = NULL;
    g_wmState.activeWindow = NULL;

    /* Initialize auxiliary window list */
    g_wmState.auxWinHead = NULL;

    /* Check for Color QuickDraw availability */
    g_wmState.colorQDAvailable = Platform_HasColorQuickDraw();

    /* Mark as initialized */
    g_wmState.initialized = true;

    #ifdef DEBUG_WINDOW_MANAGER
    printf("Window Manager initialized successfully\n");
    #endif
}

void GetWMgrPort(GrafPtr* wPort) {
    if (wPort == NULL) return;

    if (!g_wmState.initialized) {
        InitWindows();
    }

    if (g_wmState.wMgrPort) {
        *wPort = &g_wmState.port;
    } else {
        *wPort = NULL;
    }
}

void GetCWMgrPort(CGrafPtr* wMgrCPort) {
    if (wMgrCPort == NULL) return;

    if (!g_wmState.initialized) {
        InitWindows();
    }

    *wMgrCPort = g_wmState.wMgrCPort;
}

/* ============================================================================
 * Window Creation Functions
 * ============================================================================ */

WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                   ConstStr255Param title, Boolean visible,
                   short theProc, WindowPtr behind,
                   Boolean goAwayFlag, long refCon) {

    WM_LOG_TRACE("[NEWWIN] A - ENTERED\n");

    if (!g_wmState.initialized) {
        WM_LOG_TRACE("[NEWWIN] B - calling InitWindows\n");
        InitWindows();
    }

    if (boundsRect == NULL) {
        WM_LOG_TRACE("[NEWWIN] C - boundsRect NULL, returning\n");
        #ifdef DEBUG_WINDOW_MANAGER
        printf("NewWindow: boundsRect is NULL\n");
        #endif
        return NULL;
    }

    WM_LOG_TRACE("[NEWWIN] D - after boundsRect check\n");

    WindowPtr window;

    /* Allocate window storage if not provided */
    if (wStorage == NULL) {
        WM_LOG_TRACE("[NEWWIN] E - allocating storage\n");
        window = AllocateWindowRecord(false); /* Black & white window */
        if (window == NULL) {
            WM_LOG_ERROR("[NEWWIN] F - allocation failed\n");
            #ifdef DEBUG_WINDOW_MANAGER
            printf("NewWindow: Failed to allocate window record\n");
            #endif
            return NULL;
        }
    } else {
        WM_LOG_TRACE("[NEWWIN] G - using provided storage\n");
        window = (WindowPtr)wStorage;
        memset(window, 0, sizeof(WindowRecord));
    }

    WM_LOG_TRACE("[NEWWIN] H - calling InitializeWindowRecord\n");
    /* Initialize the window record */
    InitializeWindowRecord(window, boundsRect, title, theProc, visible, goAwayFlag);
    window->refCon = refCon;

    WM_LOG_TRACE("[NEWWIN] I - initializing port\n");
    /* Initialize the window's graphics port */
    if (!Platform_InitializeWindowPort(window)) {
        WM_LOG_ERROR("[NEWWIN] J - port init failed\n");
        if (wStorage == NULL) {
            DeallocateWindowRecord(window);
        }
        return NULL;
    }

    /* Regions already initialized in InitializeWindowRecord - don't recalculate */
    /* Platform_CalculateWindowRegions would overwrite with local coordinates */

    /* Create offscreen GWorld for double-buffering */
    WM_LOG_TRACE("[NEWWIN] Creating offscreen GWorld for double-buffering\n");
    Rect contentRect = window->port.portRect;
    SInt16 width = contentRect.right - contentRect.left;
    SInt16 height = contentRect.bottom - contentRect.top;

    if (width > 0 && height > 0) {
        OSErr err = NewGWorld(&window->offscreenGWorld, 32, &contentRect, NULL, NULL, 0);
        if (err != noErr) {
            WM_LOG_WARN("[NEWWIN] Failed to create offscreen GWorld (err=%d)\n", err);
            window->offscreenGWorld = NULL;
        } else {
            WM_LOG_TRACE("[NEWWIN] Offscreen GWorld created successfully\n");
        }
    } else {
        window->offscreenGWorld = NULL;
    }

    WM_LOG_TRACE("[NEWWIN] K - adding to window list\n");
    /* Add window to the window list */
    AddWindowToList(window, behind);

    WM_LOG_TRACE("[NEWWIN] L - creating native window\n");
    /* Create native platform window */
    Platform_CreateNativeWindow(window);

    WM_LOG_TRACE("[NEWWIN] M - checking visible flag\n");
    /* Make visible if requested */
    if (visible) {
        WM_LOG_TRACE("[NEWWIN] N - calling ShowWindow\n");
        ShowWindow(window);
        WM_LOG_TRACE("[NEWWIN] O - ShowWindow returned\n");
    }

    WM_LOG_TRACE("[NEWWIN] P - returning window\n");
    #ifdef DEBUG_WINDOW_MANAGER
    printf("NewWindow: Created window at (%d,%d) size (%d,%d)\n",
           boundsRect->left, boundsRect->top,
           boundsRect->right - boundsRect->left,
           boundsRect->bottom - boundsRect->top);
    #endif

    return window;
}

WindowPtr NewCWindow(void* wStorage, const Rect* boundsRect,
                    ConstStr255Param title, Boolean visible,
                    short procID, WindowPtr behind,
                    Boolean goAwayFlag, long refCon) {

    if (!g_wmState.initialized) {
        InitWindows();
    }

    if (boundsRect == NULL) {
        #ifdef DEBUG_WINDOW_MANAGER
        printf("NewCWindow: boundsRect is NULL\n");
        #endif
        return NULL;
    }

    /* Check if Color QuickDraw is available */
    if (!g_wmState.colorQDAvailable) {
        /* Fall back to black & white window */
        return NewWindow(wStorage, boundsRect, title, visible, procID,
                        behind, goAwayFlag, refCon);
    }

    CWindowPtr window;

    /* Allocate window storage if not provided */
    if (wStorage == NULL) {
        window = (CWindowPtr)AllocateWindowRecord(true); /* Color window */
        if (window == NULL) {
            #ifdef DEBUG_WINDOW_MANAGER
            printf("NewCWindow: Failed to allocate color window record\n");
            #endif
            return NULL;
        }
    } else {
        window = (CWindowPtr)wStorage;
        memset(window, 0, sizeof(CWindowRecord));
    }

    /* Initialize the window record */
    InitializeWindowRecord((WindowPtr)window, boundsRect, title, procID,
                          visible, goAwayFlag);
    ((WindowPtr)window)->refCon = refCon;

    /* Initialize the window's color graphics port */
    if (!Platform_InitializeColorWindowPort((WindowPtr)window)) {
        if (wStorage == NULL) {
            DeallocateWindowRecord((WindowPtr)window);
        }
        return NULL;
    }

    /* Create auxiliary window record for color information */
    AuxWinHandle auxWin = CreateAuxiliaryWindowRecord((WindowPtr)window);
    if (auxWin == NULL) {
        Platform_CleanupWindowPort((WindowPtr)window);
        if (wStorage == NULL) {
            DeallocateWindowRecord((WindowPtr)window);
        }
        return NULL;
    }

    /* Calculate window regions */
    Platform_CalculateWindowRegions((WindowPtr)window);

    /* Create offscreen GWorld for double-buffering */
    Rect contentRect = ((WindowPtr)window)->port.portRect;
    SInt16 width = contentRect.right - contentRect.left;
    SInt16 height = contentRect.bottom - contentRect.top;

    if (width > 0 && height > 0) {
        OSErr err = NewGWorld(&((WindowPtr)window)->offscreenGWorld, 32, &contentRect, NULL, NULL, 0);
        if (err != noErr) {
            ((WindowPtr)window)->offscreenGWorld = NULL;
        }
    } else {
        ((WindowPtr)window)->offscreenGWorld = NULL;
    }

    /* Add window to the window list */
    AddWindowToList((WindowPtr)window, behind);

    /* Create native platform window */
    Platform_CreateNativeWindow((WindowPtr)window);

    /* Make visible if requested */
    if (visible) {
        ShowWindow((WindowPtr)window);
    }

    #ifdef DEBUG_WINDOW_MANAGER
    printf("NewCWindow: Created color window at (%d,%d) size (%d,%d)\n",
           boundsRect->left, boundsRect->top,
           boundsRect->right - boundsRect->left,
           boundsRect->bottom - boundsRect->top);
    #endif

    return (WindowPtr)window;
}

WindowPtr GetNewWindow(short windowID, void* wStorage, WindowPtr behind) {
    /* In a full implementation, this would load a WIND resource */
    /* For now, create a default window with reasonable parameters */

    Rect defaultBounds;
    defaultBounds.left = 50;
    defaultBounds.top = 50;
    defaultBounds.right = 400;
    defaultBounds.bottom = 300;

    unsigned char defaultTitle[256];
    defaultTitle[0] = 8; /* Length */
    memcpy(&defaultTitle[1], "Untitled", 8);

    #ifdef DEBUG_WINDOW_MANAGER
    printf("GetNewWindow: Creating default window for resource ID %d\n", windowID);
    #endif

    return NewWindow(wStorage, &defaultBounds, defaultTitle, true,
                    documentProc, behind, true, 0);
}

WindowPtr GetNewCWindow(short windowID, void* wStorage, WindowPtr behind) {
    /* In a full implementation, this would load a WIND resource */
    /* For now, create a default color window */

    Rect defaultBounds;
    defaultBounds.left = 50;
    defaultBounds.top = 50;
    defaultBounds.right = 400;
    defaultBounds.bottom = 300;

    unsigned char defaultTitle[256];
    defaultTitle[0] = 8; /* Length */
    memcpy(&defaultTitle[1], "Untitled", 8);

    #ifdef DEBUG_WINDOW_MANAGER
    printf("GetNewCWindow: Creating default color window for resource ID %d\n", windowID);
    #endif

    return NewCWindow(wStorage, &defaultBounds, defaultTitle, true,
                     documentProc, behind, true, 0);
}

/* ============================================================================
 * Window Disposal Functions
 * ============================================================================ */

/* Temporarily disable ALL WM logging to prevent heap corruption from variadic serial_logf */
#undef WM_LOG_DEBUG
#undef WM_LOG_TRACE
#undef WM_LOG_WARN
#undef WM_LOG_ERROR
#define WM_LOG_DEBUG(...) do {} while(0)
#define WM_LOG_TRACE(...) do {} while(0)
#define WM_LOG_WARN(...) do {} while(0)
#define WM_LOG_ERROR(...) do {} while(0)

void CloseWindow(WindowPtr theWindow) {
    extern void CleanupFolderWindow(WindowPtr w);

    WM_LOG_DEBUG("CloseWindow: ENTRY, window=0x%08x\n", (unsigned int)P2UL(theWindow));
    if (theWindow == NULL) {
        WM_LOG_WARN("CloseWindow: NULL window, returning\n");
        return;
    }

    /* Clean up folder window state if this is a folder window */
    WM_LOG_DEBUG("CloseWindow: Calling CleanupFolderWindow\n");
    CleanupFolderWindow(theWindow);
    __asm__ volatile("nop");  /* Marker after call */
    /* NO LOGGING - WM_LOG_DEBUG uses variadic serial_logf which corrupts stack! */

    #ifdef DEBUG_WINDOW_MANAGER
    printf("CloseWindow: Closing window\n");
    #endif

    /* Hide the window if it's visible */
    WM_LOG_DEBUG("CloseWindow: About to check visible flag, window=0x%08x\n", (unsigned int)P2UL(theWindow));
    WM_LOG_DEBUG("CloseWindow: Reading theWindow->visible...\n");
    Boolean isVisible = theWindow->visible;
    WM_LOG_DEBUG("CloseWindow: visible flag=%d\n", isVisible);
    if (isVisible) {
        WM_LOG_DEBUG("CloseWindow: Window is visible, about to call HideWindow(0x%08x)\n", (unsigned int)P2UL(theWindow));
        __asm__ volatile("nop; nop; nop;");
        HideWindow(theWindow);
        __asm__ volatile("nop; nop; nop;");
        WM_LOG_DEBUG("CloseWindow: HideWindow returned\n");
    } else {
        WM_LOG_DEBUG("CloseWindow: Window not visible, skipping HideWindow\n");
    }

    /* Remove from window list */
    WM_LOG_DEBUG("CloseWindow: About to call RemoveWindowFromList(0x%08x)\n", (unsigned int)P2UL(theWindow));
    RemoveWindowFromList(theWindow);
    WM_LOG_DEBUG("CloseWindow: RemoveWindowFromList returned\n");

    /* Dispose of auxiliary window record if it exists */
    WM_LOG_DEBUG("CloseWindow: Checking for AuxWin\n");
    AuxWinHandle auxWin;
    if (GetAuxWin(theWindow, &auxWin)) {
        WM_LOG_DEBUG("CloseWindow: Disposing AuxWin\n");
        DisposeAuxiliaryWindowRecord(auxWin);
        WM_LOG_DEBUG("CloseWindow: AuxWin disposed\n");
    }

    /* Dispose of offscreen GWorld if it exists */
    WM_LOG_DEBUG("CloseWindow: About to check offscreenGWorld\n");
    if (theWindow->offscreenGWorld) {
        WM_LOG_DEBUG("CloseWindow: Calling DisposeGWorld\n");
        DisposeGWorld(theWindow->offscreenGWorld);
        theWindow->offscreenGWorld = NULL;
        WM_LOG_DEBUG("CloseWindow: DisposeGWorld completed\n");
    }

    /* Destroy native platform window */
    WM_LOG_DEBUG("CloseWindow: About to destroy native window\n");
    Platform_DestroyNativeWindow(theWindow);
    WM_LOG_DEBUG("CloseWindow: Native window destroyed\n");

    /* Dispose of window regions */
    WM_LOG_DEBUG("CloseWindow: About to dispose regions\n");
    if (theWindow->strucRgn) {
        Platform_DisposeRgn(theWindow->strucRgn);
        theWindow->strucRgn = NULL;
    }
    if (theWindow->contRgn) {
        Platform_DisposeRgn(theWindow->contRgn);
        theWindow->contRgn = NULL;
    }
    if (theWindow->visRgn) {
        Platform_DisposeRgn(theWindow->visRgn);
        theWindow->visRgn = NULL;
    }
    if (theWindow->updateRgn) {
        Platform_DisposeRgn(theWindow->updateRgn);
        theWindow->updateRgn = NULL;
    }
    WM_LOG_DEBUG("CloseWindow: Regions disposed\n");

    /* Dispose of title using Memory Manager */
    WM_LOG_TRACE("CloseWindow: Disposing title\n");
    if (theWindow->titleHandle) {
        DisposeHandle((Handle)theWindow->titleHandle);
        theWindow->titleHandle = NULL;
    }
    WM_LOG_TRACE("CloseWindow: Title disposed\n");

    /* Clean up the window's port */
    WM_LOG_TRACE("CloseWindow: Cleaning up port\n");
    Platform_CleanupWindowPort(theWindow);
    WM_LOG_TRACE("CloseWindow: Port cleaned up\n");

    /* Mark window as invisible and invalid - but DON'T zero memory yet!
     * Memory will be freed by DisposeWindow or reused by NewWindow.
     * Zeroing here causes crashes if any code has cached pointers. */
    theWindow->visible = false;
    WM_LOG_TRACE("CloseWindow: EXIT\n");
}

void DisposeWindow(WindowPtr theWindow) {
    extern void serial_puts(const char *str);

    serial_puts("[WM] DisposeWindow: ENTRY\n");

    if (theWindow == NULL) {
        serial_puts("[WM] DisposeWindow: NULL window\n");
        return;
    }

    DM_ClearFocusForWindow(theWindow);
    CloseWindow(theWindow);
    DeallocateWindowRecord(theWindow);

    serial_puts("[WM] DisposeWindow: EXIT\n");
}

/**
 * Window deactivation - suspend keyboard focus and hide focus ring
 */
void WM_OnDeactivate(WindowPtr w) {
    if (!w) {
        return;
    }
    /* Remember current focus and hide the ring */
    s_lastFocus = DM_GetKeyboardFocus(w);
    if (s_lastFocus) {
        DM_SetKeyboardFocus(w, NULL);
    }
    WM_LOG_TRACE("[WM] Deactivate %p\n", (void*)w);
}

/**
 * Window activation - restore keyboard focus and show focus ring
 */
void WM_OnActivate(WindowPtr w) {
    if (!w) {
        return;
    }
    /* Prefer previously focused control if still valid; else first focusable */
    if (s_lastFocus && (*s_lastFocus) && (*s_lastFocus)->contrlOwner == w) {
        DM_SetKeyboardFocus(w, s_lastFocus);
    } else {
        DM_FocusNextControl(w, false); /* pick first focusable */
    }
    s_lastFocus = NULL; /* one-shot */
    WM_LOG_TRACE("[WM] Activate %p\n", (void*)w);
}

/* ============================================================================
 * Window Information Functions
 * ============================================================================ */

void SetWRefCon(WindowPtr theWindow, long data) {
    if (theWindow == NULL) return;

    theWindow->refCon = data;

    #ifdef DEBUG_WINDOW_MANAGER
    printf("SetWRefCon: Set window refCon to %ld\n", data);
    #endif
}

long GetWRefCon(WindowPtr theWindow) {
    if (theWindow == NULL) return 0;

    return theWindow->refCon;
}

void SetWindowPic(WindowPtr theWindow, PicHandle pic) {
    if (theWindow == NULL) return;

    theWindow->windowPic = pic;

    /* If window is visible, redraw content */
    if (theWindow->visible) {
        Platform_InvalidateWindowContent(theWindow);
    }

    #ifdef DEBUG_WINDOW_MANAGER
    printf("SetWindowPic: Set window picture\n");
    #endif
}

PicHandle GetWindowPic(WindowPtr theWindow) {
    if (theWindow == NULL) return NULL;

    return theWindow->windowPic;
}

/* ============================================================================
 * Auxiliary Window Functions
 * ============================================================================ */

Boolean GetAuxWin(WindowPtr theWindow, AuxWinHandle* awHndl) {
    if (awHndl == NULL) return false;
    *awHndl = NULL;

    if (theWindow == NULL) return false;

    /* Search auxiliary window list */
    AuxWinHandle current = g_wmState.auxWinHead;
    while (current != NULL) {
        if (*current != NULL && (**current).awOwner == theWindow) {
            *awHndl = current;
            return true;
        }
        if (*current != NULL) {
            current = (AuxWinHandle)&(**current).awNext;
        } else {
            break;
        }
    }

    return false;
}

void SetWinColor(WindowPtr theWindow, WCTabHandle newColorTable) {
    if (theWindow == NULL) return;

    AuxWinHandle auxWin;
    if (GetAuxWin(theWindow, &auxWin)) {
        if (*auxWin != NULL) {
            /* Dispose of old color table */
            if ((**auxWin).awCTable) {
                Platform_DisposeCTable((**auxWin).awCTable);
            }

            /* Set new color table */
            (**auxWin).awCTable = newColorTable;

            /* Update window appearance */
            if (theWindow->visible) {
                Platform_UpdateWindowColors(theWindow);
            }
        }
    }

    #ifdef DEBUG_WINDOW_MANAGER
    printf("SetWinColor: Set window color table\n");
    #endif
}

/* ============================================================================
 * Global State Access
 * ============================================================================ */

WindowManagerState* GetWindowManagerState(void) {
    return &g_wmState;
}

/* ============================================================================
 * Internal Helper Function Implementations
 * ============================================================================ */

static void InitializeWMgrPort(void) {
    /* Allocate Window Manager port */
    g_wmState.wMgrPort = (WMgrPort*)calloc(1, sizeof(WMgrPort));
    if (g_wmState.wMgrPort == NULL) {
        #ifdef DEBUG_WINDOW_MANAGER
        printf("InitializeWMgrPort: Failed to allocate WMgrPort\n");
        #endif
        return;
    }

    /* Initialize the base graphics port */
    Platform_InitializePort(&g_wmState.port);

    /* Set Window Manager specific fields */
    g_wmState.windowList = NULL;
    g_wmState.activeWindow = NULL;
    g_wmState.ghostWindow = NULL;
    g_wmState.menuBarHeight = 20; /* Standard Mac OS menu bar height */
    g_wmState.initialized = true;

    /* Get screen bounds for gray region */
    g_wmState.grayRgn = NewRgn();
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);
    Platform_SetRectRgn(g_wmState.grayRgn, &screenBounds);

    /* Initialize Color Window Manager port if available */
    if (g_wmState.colorQDAvailable) {
        g_wmState.wMgrCPort = (CGrafPtr)calloc(1, sizeof(CGrafPort));
        if (g_wmState.wMgrCPort) {
            Platform_InitializeColorPort(g_wmState.wMgrCPort);
        }
    }
}

static void InitializeDesktopPattern(void) {
    /* Set up standard Mac OS gray desktop pattern (50% gray) */
    static const unsigned char standardGrayPattern[8] = {
        0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
    };

    memcpy(g_wmState.desktopPattern.pat, standardGrayPattern, 8);

    if (g_wmState.wMgrPort) {
        g_wmState.deskPattern = g_wmState.desktopPattern;
    }

    /* Initialize color desktop pattern if available */
    if (g_wmState.colorQDAvailable) {
        g_wmState.desktopPixPat = Platform_CreateStandardGrayPixPat();
    }
}

static WindowPtr AllocateWindowRecord(Boolean isColorWindow) {
    extern void serial_puts(const char *str);

    size_t recordSize = isColorWindow ? sizeof(CWindowRecord) : sizeof(WindowRecord);
    /* NO serial_printf - variadic funcs corrupt heap before calloc! */

    WindowPtr window = (WindowPtr)calloc(1, recordSize);

    if (window) {
        serial_puts("[WM] AllocateWindowRecord: SUCCESS\n");
    } else {
        serial_puts("[WM] AllocateWindowRecord: FAILED\n");
        #ifdef DEBUG_WINDOW_MANAGER
        printf("AllocateWindowRecord: Failed to allocate window record\n");
        #endif
    }

    return window;
}

static void DeallocateWindowRecord(WindowPtr window) {
    extern void serial_puts(const char *str);
    extern void DisposePtr(void* p);  /* Direct call to MemoryManager */

    serial_puts("[WM] DeallocateWindowRecord: ENTRY\n");

    if (window) {
        DisposePtr(window);  /* Bypass free(), call DisposePtr directly */
    }

    serial_puts("[WM] DeallocateWindowRecord: EXIT\n");
}

static void InitializeWindowRecord(WindowPtr window, const Rect* bounds,
                                 ConstStr255Param title, short procID,
                                 Boolean visible, Boolean goAwayFlag) {
    extern void serial_puts(const char *str);

    serial_puts("[WM] InitializeWindowRecord: ENTRY\n");

    if (window == NULL || bounds == NULL) return;

    serial_puts("[WM] InitializeWindowRecord: Setting basic properties\n");

    /* Set basic window properties */
    window->windowKind = userKind;
    window->visible = false; /* Will be set by ShowWindow if requested */
    window->hilited = false;
    window->goAwayFlag = goAwayFlag;
    window->spareFlag = false;

    serial_puts("[WM] InitializeWindowRecord: About to create window regions\n");

    /* Create window regions */
    serial_puts("[WM] InitializeWindowRecord: Creating strucRgn\n");
    window->strucRgn = Platform_NewRgn();
    serial_puts("[WM] InitializeWindowRecord: Creating contRgn\n");
    window->contRgn = Platform_NewRgn();
    serial_puts("[WM] InitializeWindowRecord: Creating updateRgn\n");
    window->updateRgn = Platform_NewRgn();
    serial_puts("[WM] InitializeWindowRecord: Creating visRgn\n");
    window->visRgn = Platform_NewRgn();

    serial_puts("[WM] InitializeWindowRecord: Getting window def proc\n");

    /* DEBUG: Log procID value */
    extern void hal_outb(uint16_t port, uint8_t value);
    serial_puts("[WM] InitializeWindowRecord: procID=0x");
    for (int i = 1; i >= 0; i--) {
        uint8_t nibble = (procID >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts("\n");

    /* Set window definition procedure based on procID */
    serial_puts("[WM] InitializeWindowRecord: About to call Platform_GetWindowDefProc\n");
    Handle defProc = Platform_GetWindowDefProc(procID);
    serial_puts("[WM] InitializeWindowRecord: Platform_GetWindowDefProc returned\n");

    /* DEBUG: Log returned value */
    serial_puts("[WM] InitializeWindowRecord: defProc=0x");
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = ((uint32_t)defProc >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts("\n");

    serial_puts("[WM] InitializeWindowRecord: About to assign windowDefProc\n");
    window->windowDefProc = defProc;
    serial_puts("[WM] InitializeWindowRecord: windowDefProc assigned\n");

    window->dataHandle = NULL;

    /* Set window title */
    window->titleHandle = NULL;
    window->titleWidth = 0;
    WM_LOG_TRACE("TITLE_INIT: title ptr=%p, len=%d\n", title, title ? title[0] : -1);
    if (title && title[0] > 0) {
        WM_LOG_TRACE("TITLE_INIT: Calling SetWTitle\n");
        SetWTitle(window, title);
    } else {
        WM_LOG_TRACE("TITLE_INIT: Skipping SetWTitle (NULL or empty title)\n");
    }

    /* Initialize control list */
    window->controlList = NULL;

    /* Initialize window chain */
    window->nextWindow = NULL;

    /* Initialize other fields */
    window->windowPic = NULL;
    window->refCon = 0;

    /* Set initial port bounds, clamping to avoid menu bar overlap */
    #define kMenuBarHeight 20
    Rect clampedBounds = *bounds;
    if (clampedBounds.top < kMenuBarHeight) {
        SInt16 delta = kMenuBarHeight - clampedBounds.top;
        clampedBounds.top += delta;
        clampedBounds.bottom += delta;
    }

    /* Window port uses LOCAL coordinates (0,0,width,height)
     * portRect should contain ONLY the content area dimensions, excluding chrome */
    const SInt16 kBorder = 1;
    const SInt16 kTitleBar = 20;
    const SInt16 kSeparator = 1;

    SInt16 fullWidth = clampedBounds.right - clampedBounds.left;
    SInt16 fullHeight = clampedBounds.bottom - clampedBounds.top;
    WM_LOG_TRACE("[NEWWIN] clampedBounds=(%d,%d,%d,%d) -> fullW=%d fullH=%d\n",
                 clampedBounds.left, clampedBounds.top, clampedBounds.right, clampedBounds.bottom,
                 fullWidth, fullHeight);

    /* Content area is smaller than full window by the chrome dimensions
     * Subtract 3px width (1px left border + 2px right for 3D effect) and extra height for bottom border */
    SInt16 contentWidth = fullWidth - 3;  /* Subtract left border and right 3D highlight */
    SInt16 contentHeight = fullHeight - kTitleBar - kSeparator - 2;  /* Subtract title bar, separator, and bottom border */

    SetRect(&window->port.portRect, 0, 0, contentWidth, contentHeight);
    WM_LOG_TRACE("[NEWWIN] portRect set to (0,0,%d,%d) from content w=%d h=%d\n",
                 window->port.portRect.right, window->port.portRect.bottom,
                 contentWidth, contentHeight);

    /* CRITICAL: portBits.bounds defines where local coords map to global screen coords!
     * NOTE: This will be overwritten by Platform_InitializeWindowPort which calculates
     * the correct mapping from strucRgn. Setting initial values here for reference. */
    SetRect(&window->port.portBits.bounds,
            clampedBounds.left + kBorder,
            clampedBounds.top + kTitleBar + kSeparator,
            clampedBounds.left + kBorder + contentWidth,
            clampedBounds.top + kTitleBar + kSeparator + contentHeight);

    /* Initialize portBits to point to screen framebuffer */
    extern void* framebuffer;
    extern uint32_t fb_width;
    window->port.portBits.baseAddr = (Ptr)framebuffer;
    /* Set PixMap flag (bit 15) to indicate 32-bit PixMap, not 1-bit BitMap */
    window->port.portBits.rowBytes = (fb_width * 4) | 0x8000;


    /* Initialize strucRgn with global bounds */
    if (window->strucRgn) {
        Platform_SetRectRgn(window->strucRgn, &clampedBounds);
        WM_LOG_TRACE("InitializeWindowRecord: Set strucRgn to clampedBounds\n");
    }

    /* CRITICAL: Initialize contRgn to match portBits.bounds EXACTLY!
     * contRgn must match the actual content area for proper clipping */
    if (window->contRgn) {
        Platform_SetRectRgn(window->contRgn, &window->port.portBits.bounds);
        WM_LOG_TRACE("InitializeWindowRecord: Set contRgn to match portBits.bounds (%d,%d,%d,%d)\n",
                     window->port.portBits.bounds.left, window->port.portBits.bounds.top,
                     window->port.portBits.bounds.right, window->port.portBits.bounds.bottom);
    }
}

static void AddWindowToList(WindowPtr window, WindowPtr behind) {

    if (window == NULL) return;

    WM_LOG_TRACE("WindowManager: AddWindowToList window=%p, behind=%p\n", window, behind);

    /* Remove from list if already in it */
    RemoveWindowFromList(window);

    if (behind == NULL || behind == (WindowPtr)-1L) {
        /* Add to front of list */
        WM_LOG_TRACE("WindowManager: Adding window %p to FRONT (behind=%p is NULL or -1)\n", window, behind);
        window->nextWindow = g_wmState.windowList;
        g_wmState.windowList = window;
        WM_LOG_TRACE("WindowManager: Window list head is now %p\n", g_wmState.windowList);
    } else {
        /* Insert after 'behind' window */
        WM_LOG_TRACE("WindowManager: Inserting window %p AFTER behind=%p\n", window, behind);
        window->nextWindow = behind->nextWindow;
        behind->nextWindow = window;
    }

    /* Update Window Manager port reference */
    if (g_wmState.wMgrPort) {
        /* windowList is already updated, nothing to do here */
    }

    #ifdef DEBUG_WINDOW_MANAGER
    printf("AddWindowToList: Added window to list\n");
    #endif
}

static void RemoveWindowFromList(WindowPtr window) {
    if (window == NULL) return;

    if (g_wmState.windowList == window) {
        /* Remove from front of list */
        g_wmState.windowList = window->nextWindow;
    } else {
        /* Find and remove from middle/end of list */
        WindowPtr current = g_wmState.windowList;
        while (current && current->nextWindow != window) {
            current = current->nextWindow;
        }
        if (current) {
            current->nextWindow = window->nextWindow;
        }
    }

    /* Clear next pointer */
    window->nextWindow = NULL;

    /* Update active window if this was it */
    if (g_wmState.activeWindow == window) {
        g_wmState.activeWindow = NULL;
    }

    /* Update Window Manager port reference */
    if (g_wmState.wMgrPort) {
        /* windowList is already updated, nothing to do here */
        /* activeWindow is already updated, nothing to do here */
    }

    #ifdef DEBUG_WINDOW_MANAGER
    printf("RemoveWindowFromList: Removed window from list\n");
    #endif
}

static AuxWinHandle CreateAuxiliaryWindowRecord(WindowPtr owner) {
    if (owner == NULL) return NULL;

    /* Allocate handle */
    AuxWinHandle auxHandle = (AuxWinHandle)malloc(sizeof(AuxWinRec*));
    if (auxHandle == NULL) {
        #ifdef DEBUG_WINDOW_MANAGER
        printf("CreateAuxiliaryWindowRecord: Failed to allocate handle\n");
        #endif
        return NULL;
    }

    /* Allocate record */
    AuxWinRec* auxRec = (AuxWinRec*)calloc(1, sizeof(AuxWinRec));
    if (auxRec == NULL) {
        free(auxHandle);
        #ifdef DEBUG_WINDOW_MANAGER
        printf("CreateAuxiliaryWindowRecord: Failed to allocate record\n");
        #endif
        return NULL;
    }

    /* Initialize record */
    *auxHandle = (AuxWinRec*)auxRec;
    auxRec->awNext = (struct AuxWinRec*)g_wmState.auxWinHead;
    auxRec->awOwner = owner;
    auxRec->awCTable = NULL;
    auxRec->dialogCItem = NULL;
    auxRec->awFlags = 0;
    auxRec->awReserved = NULL;
    auxRec->awRefCon = 0;

    /* Add to head of auxiliary window list */
    g_wmState.auxWinHead = auxHandle;

    #ifdef DEBUG_WINDOW_MANAGER
    printf("CreateAuxiliaryWindowRecord: Created auxiliary record\n");
    #endif

    return auxHandle;
}

static void DisposeAuxiliaryWindowRecord(AuxWinHandle auxWin) {
    if (auxWin == NULL || *auxWin == NULL) return;

    /* Remove from auxiliary window list */
    if (g_wmState.auxWinHead == auxWin) {
        g_wmState.auxWinHead = (AuxWinHandle)&(**auxWin).awNext;
    } else {
        AuxWinHandle current = g_wmState.auxWinHead;
        while (current && *current && (**current).awNext != *auxWin) {
            current = (AuxWinHandle)&(**current).awNext;
        }
        if (current && *current) {
            (**current).awNext = (**auxWin).awNext;
        }
    }

    /* Dispose of color table */
    if ((**auxWin).awCTable) {
        Platform_DisposeCTable((**auxWin).awCTable);
    }

    /* Dispose of dialog items */
    if ((**auxWin).dialogCItem) {
        /* Dialog Manager would handle this */
    }

    /* Free the record and handle */
    free(*auxWin);
    free(auxWin);

    #ifdef DEBUG_WINDOW_MANAGER
    printf("DisposeAuxiliaryWindowRecord: Disposed auxiliary record\n");
    #endif
}

void CopyPascalString(const unsigned char* source, unsigned char* dest) {
    if (source == NULL || dest == NULL) return;

    short length = source[0];
    if (length > 255) length = 255;

    dest[0] = length;
    if (length > 0) {
        memcpy(&dest[1], &source[1], length);
    }
}
