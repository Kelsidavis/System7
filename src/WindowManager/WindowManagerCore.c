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
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WindowKinds.h"
#include "WindowManager/LayoutGuards.h"

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
static short GetPascalStringLength(const unsigned char* str);

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

    serial_printf("[NEWWIN] A - ENTERED\n");

    if (!g_wmState.initialized) {
        serial_printf("[NEWWIN] B - calling InitWindows\n");
        InitWindows();
    }

    if (boundsRect == NULL) {
        serial_printf("[NEWWIN] C - boundsRect NULL, returning\n");
        #ifdef DEBUG_WINDOW_MANAGER
        printf("NewWindow: boundsRect is NULL\n");
        #endif
        return NULL;
    }

    serial_printf("[NEWWIN] D - after boundsRect check\n");

    WindowPtr window;

    /* Allocate window storage if not provided */
    if (wStorage == NULL) {
        serial_printf("[NEWWIN] E - allocating storage\n");
        window = AllocateWindowRecord(false); /* Black & white window */
        if (window == NULL) {
            serial_printf("[NEWWIN] F - allocation failed\n");
            #ifdef DEBUG_WINDOW_MANAGER
            printf("NewWindow: Failed to allocate window record\n");
            #endif
            return NULL;
        }
    } else {
        serial_printf("[NEWWIN] G - using provided storage\n");
        window = (WindowPtr)wStorage;
        memset(window, 0, sizeof(WindowRecord));
    }

    serial_printf("[NEWWIN] H - calling InitializeWindowRecord\n");
    /* Initialize the window record */
    InitializeWindowRecord(window, boundsRect, title, theProc, visible, goAwayFlag);
    window->refCon = refCon;

    serial_printf("[NEWWIN] I - initializing port\n");
    /* Initialize the window's graphics port */
    if (!Platform_InitializeWindowPort(window)) {
        serial_printf("[NEWWIN] J - port init failed\n");
        if (wStorage == NULL) {
            DeallocateWindowRecord(window);
        }
        return NULL;
    }

    /* Regions already initialized in InitializeWindowRecord - don't recalculate */
    /* Platform_CalculateWindowRegions would overwrite with local coordinates */

    serial_printf("[NEWWIN] K - adding to window list\n");
    /* Add window to the window list */
    AddWindowToList(window, behind);

    serial_printf("[NEWWIN] L - creating native window\n");
    /* Create native platform window */
    Platform_CreateNativeWindow(window);

    serial_printf("[NEWWIN] M - checking visible flag\n");
    /* Make visible if requested */
    if (visible) {
        serial_printf("[NEWWIN] N - calling ShowWindow\n");
        ShowWindow(window);
        serial_printf("[NEWWIN] O - ShowWindow returned\n");
    }

    serial_printf("[NEWWIN] P - returning window\n");
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

void CloseWindow(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    #ifdef DEBUG_WINDOW_MANAGER
    printf("CloseWindow: Closing window\n");
    #endif

    /* Hide the window if it's visible */
    if (theWindow->visible) {
        HideWindow(theWindow);
    }

    /* Remove from window list */
    RemoveWindowFromList(theWindow);

    /* Dispose of auxiliary window record if it exists */
    AuxWinHandle auxWin;
    if (GetAuxWin(theWindow, &auxWin)) {
        DisposeAuxiliaryWindowRecord(auxWin);
    }

    /* Destroy native platform window */
    Platform_DestroyNativeWindow(theWindow);

    /* Dispose of window regions */
    if (theWindow->strucRgn) {
        Platform_DisposeRgn(theWindow->strucRgn);
        theWindow->strucRgn = NULL;
    }
    if (theWindow->contRgn) {
        Platform_DisposeRgn(theWindow->contRgn);
        theWindow->contRgn = NULL;
    }
    if (theWindow->updateRgn) {
        Platform_DisposeRgn(theWindow->updateRgn);
        theWindow->updateRgn = NULL;
    }

    /* Dispose of title */
    if (theWindow->titleHandle) {
        if (*(theWindow->titleHandle)) {
            free(*(theWindow->titleHandle));
        }
        free(theWindow->titleHandle);
        theWindow->titleHandle = NULL;
    }

    /* Clean up the window's port */
    Platform_CleanupWindowPort(theWindow);

    /* Clear the window record (but don't free it) */
    memset(theWindow, 0, sizeof(WindowRecord));
}

void DisposeWindow(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    #ifdef DEBUG_WINDOW_MANAGER
    printf("DisposeWindow: Disposing window\n");
    #endif

    /* Close the window first */
    CloseWindow(theWindow);

    /* Free the window record memory */
    DeallocateWindowRecord(theWindow);
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
            current = (**current).awNext;
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
    Platform_GetScreenBounds(g_wmState.grayRgn);

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
    size_t recordSize = isColorWindow ? sizeof(CWindowRecord) : sizeof(WindowRecord);
    WindowPtr window = (WindowPtr)calloc(1, recordSize);

    if (window == NULL) {
        #ifdef DEBUG_WINDOW_MANAGER
        printf("AllocateWindowRecord: Failed to allocate window record\n");
        #endif
    }

    return window;
}

static void DeallocateWindowRecord(WindowPtr window) {
    if (window) {
        free(window);
    }
}

static void InitializeWindowRecord(WindowPtr window, const Rect* bounds,
                                 ConstStr255Param title, short procID,
                                 Boolean visible, Boolean goAwayFlag) {
    if (window == NULL || bounds == NULL) return;

    /* Set basic window properties */
    window->windowKind = userKind;
    window->visible = false; /* Will be set by ShowWindow if requested */
    window->hilited = false;
    window->goAwayFlag = goAwayFlag;
    window->spareFlag = false;

    /* Create window regions */
    window->strucRgn = Platform_NewRgn();
    window->contRgn = Platform_NewRgn();
    window->updateRgn = Platform_NewRgn();

    /* Set window definition procedure based on procID */
    window->windowDefProc = Platform_GetWindowDefProc(procID);
    window->dataHandle = NULL;

    /* Set window title */
    window->titleHandle = NULL;
    window->titleWidth = 0;
    if (title && title[0] > 0) {
        SetWTitle(window, title);
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

    /* Window port uses LOCAL coordinates (0,0,width,height) */
    SInt16 width = clampedBounds.right - clampedBounds.left;
    SInt16 height = clampedBounds.bottom - clampedBounds.top;
    SetRect(&window->port.portRect, 0, 0, width, height);

    /* CRITICAL: portBits.bounds defines where local coords map to global screen coords!
     * This must be set to the window's GLOBAL position so that drawing in local
     * coordinates (0,0) maps to the window's screen position */
    window->port.portBits.bounds = clampedBounds;

    /* Initialize portBits to point to screen framebuffer */
    extern void* framebuffer;
    extern uint32_t fb_width;
    window->port.portBits.baseAddr = (Ptr)framebuffer;
    window->port.portBits.rowBytes = fb_width * 4;

    serial_printf("InitializeWindowRecord: portRect = (%d,%d,%d,%d)\n",
                  window->port.portRect.left, window->port.portRect.top,
                  window->port.portRect.right, window->port.portRect.bottom);
    serial_printf("InitializeWindowRecord: portBits.bounds = (%d,%d,%d,%d)\n",
                  window->port.portBits.bounds.left, window->port.portBits.bounds.top,
                  window->port.portBits.bounds.right, window->port.portBits.bounds.bottom);
    serial_printf("InitializeWindowRecord: clampedBounds = (%d,%d,%d,%d)\n",
                  clampedBounds.left, clampedBounds.top,
                  clampedBounds.right, clampedBounds.bottom);

    /* Initialize strucRgn with global bounds */
    if (window->strucRgn) {
        Platform_SetRectRgn(window->strucRgn, &clampedBounds);
        serial_printf("InitializeWindowRecord: Set strucRgn to clampedBounds\n");
    }
}

static void AddWindowToList(WindowPtr window, WindowPtr behind) {
    extern void serial_printf(const char* fmt, ...);

    if (window == NULL) return;

    serial_printf("WindowManager: AddWindowToList window=%p, behind=%p\n", window, behind);

    /* Remove from list if already in it */
    RemoveWindowFromList(window);

    if (behind == NULL || behind == (WindowPtr)-1L) {
        /* Add to front of list */
        serial_printf("WindowManager: Adding window %p to FRONT (behind=%p is NULL or -1)\n", window, behind);
        window->nextWindow = g_wmState.windowList;
        g_wmState.windowList = window;
        serial_printf("WindowManager: Window list head is now %p\n", g_wmState.windowList);
    } else {
        /* Insert after 'behind' window */
        serial_printf("WindowManager: Inserting window %p AFTER behind=%p\n", window, behind);
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
    auxRec->awNext = g_wmState.auxWinHead;
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
        g_wmState.auxWinHead = (**auxWin).awNext;
    } else {
        AuxWinHandle current = g_wmState.auxWinHead;
        while (current && *current && (**current).awNext != auxWin) {
            current = (**current).awNext;
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

static short GetPascalStringLength(const unsigned char* str) {
    if (str == NULL) return 0;
    return str[0];
}
