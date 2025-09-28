/* #include "SystemTypes.h" */
#include <stdlib.h>
/**
 * RE-AGENT-BANNER
 * Window Manager - Apple System 7.1 Window Manager Reimplementation
 *
 * This file contains reimplemented Window Manager functions from Apple System 7.1.
 * Based on evidence extracted from System.rsrc (ROM disassembly)
 *
 * Evidence sources:
 * - radare2 binary analysis showing trap call patterns (24 total Window Manager traps found)
 * - Mac OS Toolbox trap numbers: 0xA910 (GetWMgrPort), 0xA912 (InitWindows), etc.
 * - Structure layouts verified against Inside Macintosh specifications
 *
 * Implementation approach:
 * - Trap-based function dispatch (original Mac OS used A-line traps)
 * - WindowRecord structure maintains 156-byte layout for binary compatibility
 * - GrafPort integration for QuickDraw compatibility
 *
 * Architecture: Motorola 68000
 * ABI: Mac OS Toolbox calling conventions
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/window_manager.h"


/* Global Window Manager state - Evidence: Window Manager maintains global state */
static WindowPtr g_windowList = NULL;      /* Head of window list */
static WindowPtr g_frontWindow = NULL;     /* Currently front window */
static GrafPtr g_wMgrPort = NULL;          /* Window Manager port */
static Boolean g_windowsInitialized = false;

/* Internal helper functions */
static void LinkWindow(WindowPtr window);
static void UnlinkWindow(WindowPtr window);
static void UpdateWindowList(void);
static WindowPtr FindWindowAtPoint(Point pt);

/**

 * Source: mappings.windowmgr.json shows trap call with 3 hits in binary
 * Purpose: Initialize the Window Manager
 */
void InitWindows(void) {
    if (g_windowsInitialized) {
        return;  /* Already initialized */
    }

    /* Initialize global state */
    g_windowList = NULL;
    g_frontWindow = NULL;
    g_wMgrPort = NULL;

    /*
    g_wMgrPort = (GrafPtr)malloc(sizeof(GrafPort));
    if (g_wMgrPort) {
        memset(g_wMgrPort, 0, sizeof(GrafPort));
        /* Initialize basic port settings */
        g_wMgrPort->device = -1;  /* Screen device */
        /* CRITICAL: Set portRect to cover entire screen so ClipToPort doesn't clip everything away */
        SetRect(&g_wMgrPort->portRect, 0, 0, 800, 600);  /* Standard VGA resolution */
        /* CRITICAL: Ensure portBits.baseAddr is NULL so we draw to screen framebuffer, not a bitmap */
        g_wMgrPort->portBits.baseAddr = NULL;
        SetRect(&g_wMgrPort->portBits.bounds, 0, 0, 800, 600);
    }

    g_windowsInitialized = true;
}

/**

 * Source: mappings.windowmgr.json shows complex signature with 8 parameters
 * Purpose: Create a new window with specified attributes
 */
WindowPtr NewWindow(void* wStorage, const Rect* boundsRect, ConstStr255Param title,
                   Boolean visible, SInt16 theProc, WindowPtr behind,
                   Boolean goAwayFlag, SInt32 refCon) {
    WindowPtr newWindow;

    if (!g_windowsInitialized) {
        InitWindows();
    }

    /*
    if (wStorage) {
        newWindow = (WindowPtr)wStorage;
    } else {
        newWindow = (WindowPtr)malloc(sizeof(WindowRecord));
        if (!newWindow) {
            return NULL;
        }
    }

    /* Initialize WindowRecord structure based on evidence from layouts.curated.windowmgr.json */
    memset(newWindow, 0, sizeof(WindowRecord));

    /* Initialize embedded GrafPort (first 108 bytes) */
    (newWindow)->device = -1;
    if (boundsRect) {
        (newWindow)->portRect = *boundsRect;
    }

    /* Window-specific fields */
    newWindow->windowKind = 0;  /* User window */
    newWindow->visible = visible;
    newWindow->hilited = false;
    newWindow->goAwayFlag = goAwayFlag;
    newWindow->spareFlag = false;
    newWindow->refCon = refCon;

    /*
    newWindow->windowDefProc = (Handle)(intptr_t)theProc;

    /* Title handling - Evidence: titleHandle field at offset 134 */
    if (title && title[0] > 0) {
        /* Create Pascal string handle */
        size_t titleLen = title[0] + 1;  /* Length byte + string */
        newWindow->titleHandle = (StringHandle)malloc(sizeof(void*));
        if (newWindow->titleHandle) {
            *newWindow->titleHandle = malloc(titleLen);
            if (*newWindow->titleHandle) {
                memcpy(*newWindow->titleHandle, title, titleLen);
            }
        }
    }

    /* Link into window list */
    LinkWindow(newWindow);

    if (visible) {
        ShowWindow(newWindow);
    }

    return newWindow;
}

/**

 * Source: mappings.windowmgr.json shows single parameter signature
 * Purpose: Dispose of a window and free its memory
 */
void DisposeWindow(WindowPtr window) {
    if (!window) {
        return;
    }

    /* Hide window first */
    HideWindow(window);

    /* Remove from window list */
    UnlinkWindow(window);

    /* Free title handle if present */
    if (window->titleHandle) {
        if (*window->titleHandle) {
            free(*window->titleHandle);
        }
        free(window->titleHandle);
    }

    /* Free regions - Evidence: strucRgn, contRgn, updateRgn at offsets 114, 118, 122 */
    if (window->strucRgn) {
        free(window->strucRgn);
    }
    if (window->contRgn) {
        free(window->contRgn);
    }
    if (window->updateRgn) {
        free(window->updateRgn);
    }

    /* Free the window itself */
    free(window);

    UpdateWindowList();
}

/**

 * Source: mappings.windowmgr.json shows 1 hit in binary analysis
 * Purpose: Make window visible
 */
void ShowWindow(WindowPtr window) {
    if (!window || window->visible) {
        return;
    }

    /*
    window->visible = true;

    /* Bring to front when showing */
    BringToFront(window);

    /* Paint the window to make it visible on screen */
    extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);
    PaintOne(window, NULL);
}

/**

 * Source: mappings.windowmgr.json shows 4 hits in binary analysis
 * Purpose: Hide window
 */
void HideWindow(WindowPtr window) {
    if (!window || !window->visible) {
        return;
    }

    /*
    window->visible = false;

    /* If this was front window, find new front window */
    if (g_frontWindow == window) {
        g_frontWindow = NULL;
        UpdateWindowList();
    }
}

/**

 * Source: mappings.windowmgr.json shows 1 hit in binary analysis
 * Purpose: Get Window Manager port
 */
void GetWMgrPort(GrafPtr* wMgrPort) {
    if (!g_windowsInitialized) {
        InitWindows();
    }

    if (wMgrPort) {
        *wMgrPort = g_wMgrPort;
    }
}

/**

 * Source: mappings.windowmgr.json shows 5 hits in binary analysis
 * Purpose: Begin window update sequence
 */
void BeginUpdate(WindowPtr window) {
    if (!window) {
        return;
    }

    /*
    /* Set clipping to update region */
    if (window->updateRgn) {
        (window)->clipRgn = window->updateRgn;
    }
}

/**

 * Source: mappings.windowmgr.json shows 4 hits in binary analysis
 * Purpose: End window update sequence
 */
void EndUpdate(WindowPtr window) {
    if (!window) {
        return;
    }

    /*
    if (window->contRgn) {
        (window)->clipRgn = window->contRgn;
    }

    /* Clear update region */
    if (window->updateRgn) {
        /* In real implementation, this would empty the region */
        /* For now, we'll leave it as-is */
    }
}

/**

 * Source: mappings.windowmgr.json shows 2 hits in binary analysis
 * Purpose: Find window at specified point and return hit test result
 */
SInt16 FindWindow(Point thePoint, WindowPtr* whichWindow) {
    if (whichWindow) {
        *whichWindow = NULL;
    }

    if (!g_windowsInitialized) {
        return inDesk;
    }

    /* Search through visible windows from front to back */
    WindowPtr window = g_frontWindow;
    while (window) {
        if (window->visible) {
            Rect* bounds = &(window)->portRect;

            /*
            if (thePoint.h >= bounds->left && thePoint.h < bounds->right &&
                thePoint.v >= bounds->top && thePoint.v < bounds->bottom) {

                if (whichWindow) {
                    *whichWindow = window;
                }

                /*
                /* Simplified hit testing - real implementation would check regions */
                if (thePoint.v < bounds->top + 20) {
                    return inDrag;  /* Title bar area */
                }

                return inContent;  /* Content area */
            }
        }
        window = window->nextWindow;
    }

    return inDesk;  /* Desktop */
}

/**

 * Source: mappings.windowmgr.json shows 4 hits in binary analysis
 * Purpose: Select and activate window
 */
void SelectWindow(WindowPtr window) {
    if (!window) {
        return;
    }

    BringToFront(window);

    /*
    if (g_frontWindow) {
        g_frontWindow->hilited = false;  /* Deactivate previous front window */
    }
    window->hilited = true;  /* Activate this window */
}

/* Additional Window Manager functions */

void MoveWindow(WindowPtr window, SInt16 hGlobal, SInt16 vGlobal, Boolean front) {
    if (!window) return;

    /*
    SInt16 width = (window)->portRect.right - (window)->portRect.left;
    SInt16 height = (window)->portRect.bottom - (window)->portRect.top;

    (window)->portRect.left = hGlobal;
    (window)->portRect.top = vGlobal;
    (window)->portRect.right = hGlobal + width;
    (window)->portRect.bottom = vGlobal + height;

    if (front) {
        BringToFront(window);
    }
}

void BringToFront(WindowPtr window) {
    if (!window || g_frontWindow == window) {
        return;
    }

    /* Remove from current position */
    UnlinkWindow(window);

    /* Add to front of list */
    window->nextWindow = g_windowList;
    g_windowList = window;
    g_frontWindow = window;

    /* Repaint the window if it's visible */
    if (window->visible) {
        extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);
        PaintOne(window, NULL);
    }
}

WindowPtr FrontWindow(void) {
    return g_frontWindow;
}

/* Internal helper functions */

static void LinkWindow(WindowPtr window) {
    if (!window) return;

    /*
    window->nextWindow = g_windowList;
    g_windowList = window;

    if (!g_frontWindow) {
        g_frontWindow = window;
    }
}

static void UnlinkWindow(WindowPtr window) {
    if (!window) return;

    if (g_windowList == window) {
        g_windowList = window->nextWindow;
    } else {
        WindowPtr prev = g_windowList;
        while (prev && prev->nextWindow != window) {
            prev = prev->nextWindow;
        }
        if (prev) {
            prev->nextWindow = window->nextWindow;
        }
    }

    window->nextWindow = NULL;

    if (g_frontWindow == window) {
        g_frontWindow = NULL;
    }
}

static void UpdateWindowList(void) {
    /* Find the frontmost visible window */
    g_frontWindow = NULL;
    WindowPtr window = g_windowList;
    while (window) {
        if (window->visible) {
            g_frontWindow = window;
            break;
        }
        window = window->nextWindow;
    }
}

/* Stub implementations for additional functions */
void SizeWindow(WindowPtr window, SInt16 w, SInt16 h, Boolean fUpdate) {
    if (!window) return;

    (window)->portRect.right = (window)->portRect.left + w;
    (window)->portRect.bottom = (window)->portRect.top + h;
}

void DragWindow(WindowPtr window, Point startPt, const Rect* boundsRect) {
    /* Stub - would implement interactive dragging */
    (void)window; (void)startPt; (void)boundsRect;
}

SInt32 GrowWindow(WindowPtr window, Point startPt, const Rect* bBox) {
    /* Stub - would implement interactive resizing */
    (void)window; (void)startPt; (void)bBox;
    return 0;
}

void SendBehind(WindowPtr window, WindowPtr behindWindow) {
    /* Stub - would reorder window in list */
    (void)window; (void)behindWindow;
}

void SetWTitle(WindowPtr window, ConstStr255Param title) {
    if (!window || !title) return;

    /* Free existing title */
    if (window->titleHandle && *window->titleHandle) {
        free(*window->titleHandle);
        *window->titleHandle = NULL;
    }

    /* Set new title */
    if (title[0] > 0) {
        size_t titleLen = title[0] + 1;
        if (!window->titleHandle) {
            window->titleHandle = (StringHandle)malloc(sizeof(void*));
        }
        if (window->titleHandle) {
            *window->titleHandle = malloc(titleLen);
            if (*window->titleHandle) {
                memcpy(*window->titleHandle, title, titleLen);
            }
        }
    }
}

void GetWTitle(WindowPtr window, Str255 title) {
    if (!window || !title) return;

    if (window->titleHandle && *window->titleHandle) {
        UInt8* titleStr = (UInt8*)*window->titleHandle;
        size_t len = titleStr[0];
        if (len > 255) len = 255;
        memcpy(title, titleStr, len + 1);
    } else {
        title[0] = 0;  /* Empty string */
    }
}

void DrawControls(WindowPtr window) {
    /* Stub - would draw all controls in window */
    (void)window;
}

void DrawGrowIcon(WindowPtr window) {
    /* Stub - would draw grow box icon */
    (void)window;
}

/*
