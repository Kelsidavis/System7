/* [WM-019] Provenance: IM:Windows Vol I pp. 2-54 to 2-58 */
/* stdio.h removed - not needed for Window Manager internals */
/*
 * WindowLayering.c - Window Z-Order and Layering Management
 *
 * This file implements window layering and z-order management, which controls
 * how windows stack on top of each other and how they interact visually.
 * This is crucial for proper window management and visual hierarchy.
 *
 * Key functions implemented:
 * - Window z-order management and manipulation
 * - Window visibility calculation with occlusion
 * - Window layer coordination and updating
 * - Window intersection and overlap detection
 * - Modal window and floating window support
 * - Window group management
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "WindowManager/WindowManagerInternal.h"

/* [WM-055] Window kind constants now in canonical header */
#include "WindowManager/WindowKinds.h"

/* [WM-019] Forward declarations for file-local Z-order helpers */
/* Provenance: IM:Windows Vol I pp. 2-54 to 2-58 "Window Ordering" */
static long WM_CalculateWindowStackLevel(WindowPtr window);
static Boolean WM_IsFloatingWindow(WindowPtr window);
static Boolean WM_IsAlertDialog(WindowPtr window);
static void WM_RecalculateAllVisibleRegions(void);
static void WM_ClipVisibleRegionsByHigherWindows(WindowPtr baseWindow);
static void WM_UpdateWindowVisibilityStats(WindowPtr window, RgnHandle visibleRgn);
static long WM_CalculateRegionArea(RgnHandle rgn);
static void WM_DisableWindowsBehindModal(WindowPtr modalWindow);
static void WM_UpdatePlatformWindowOrder(void);
static void WM_CalculateWindowVisibility(WindowPtr window);
static Boolean WM_WindowsOverlap(WindowPtr window1, WindowPtr window2);

/* ============================================================================
 * Window Layer Constants and Types
 * ============================================================================ */

/* Window layer levels */
typedef enum {
    kLayerDesktop = 0,          /* Desktop background */
    kLayerNormal = 100,         /* Normal application windows */
    kLayerFloating = 200,       /* Floating windows */
    kLayerModal = 300,          /* Modal dialogs */
    kLayerSystem = 400,         /* System windows */
    kLayerAlert = 500           /* Alert dialogs (topmost) */
} WindowLayer;

/* Window visibility state */
typedef struct WindowVisibility {
    RgnHandle visibleRgn;       /* Actually visible region */
    RgnHandle obscuredRgn;      /* Region obscured by other windows */
    Boolean fullyVisible;       /* True if no part is obscured */
    Boolean fullyObscured;      /* True if completely hidden */
    short visibilityPercent;    /* Percentage visible (0-100) */
} WindowVisibility;

/* ============================================================================
 * Internal Layer Management State
 * ============================================================================ */

static struct {
    WindowPtr modalWindow;      /* Current modal window */
    WindowPtr floatingHead;     /* Head of floating window list */
    Boolean layersInvalid;     /* True if layers need recalculation */
    unsigned long updateCounter; /* Layer update counter */
} g_layerState = { NULL, NULL, true, 0 };

/* ============================================================================
 * Window Layer Queries and Management
 * ============================================================================ */

static WindowLayer WM_GetWindowLayer(WindowPtr window) {
    if (window == NULL) return kLayerNormal;

    /* Determine layer based on window kind and properties */
    switch (window->windowKind) {
        case dialogKind:
            /* Check if modal */
            if (window == g_layerState.modalWindow) {
                return kLayerModal;
            }
            /* Check for alert style */
            if (WM_IsAlertDialog(window)) {
                return kLayerAlert;
            }
            return kLayerFloating;

        case systemKind:
            return kLayerSystem;

        case userKind:  /* deskKind and userKind are same value (8) */
        default:
            /* Check for floating window attribute */
            if (WM_IsFloatingWindow(window)) {
                return kLayerFloating;
            }
            return kLayerNormal;
    }
}

static Boolean WM_IsFloatingWindow(WindowPtr window) {
    if (window == NULL) return false;

    /* Check auxiliary window record for floating attribute */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            /* Check flags for floating window indicator */
            return ((**auxWin).awFlags & 0x0001) != 0;
        }
    }

    return false;
}

static Boolean WM_IsAlertDialog(WindowPtr window) {
    if (window == NULL) return false;

    /* [WM-021] Provenance: IM:Windows Vol I p.2-90 "Alert and Dialog WDEFs" */
    /* Alert dialogs have dialogKind = 2 */
    return (window->windowKind == dialogKind);
}

static void WM_SetWindowLayer(WindowPtr window, WindowLayer layer) {
    if (window == NULL) return;

    WM_DEBUG("WM_SetWindowLayer: Setting window to layer %d", layer);

    /* Update auxiliary window record with layer information */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            /* Store layer in flags */
            (**auxWin).awFlags &= ~0xFF00; /* Clear layer bits */
            (**auxWin).awFlags |= (layer << 8); /* Set new layer */
        }
    }

    /* Mark layers as needing recalculation */
    g_layerState.layersInvalid = true;
}

/* ============================================================================
 * Window List Management by Layer
 * ============================================================================ */

void WM_RecalculateWindowOrder(void) {
    WM_DEBUG("WM_RecalculateWindowOrder: Recalculating window order by layers");

    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState->windowList == NULL) return;

    /* Build lists for each layer */
    WindowPtr layerLists[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
    WindowPtr layerTails[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

    /* Separate windows by layer */
    WindowPtr current = wmState->windowList;
    WindowPtr next;

    while (current != NULL) {
        next = current->nextWindow;
        WindowLayer layer = WM_GetWindowLayer(current);
        int layerIndex = (layer / 100) % 6;

        /* Add to appropriate layer list */
        current->nextWindow = NULL;
        if (layerLists[layerIndex] == NULL) {
            layerLists[layerIndex] = current;
            layerTails[layerIndex] = current;
        } else {
            layerTails[layerIndex]->nextWindow = current;
            layerTails[layerIndex] = current;
        }

        current = next;
    }

    /* Rebuild main window list with proper layer ordering */
    /* Higher layers come first (rendered on top) */
    wmState->windowList = NULL;
    WindowPtr* linkPoint = &wmState->windowList;

    for (int i = 5; i >= 0; i--) {
        if (layerLists[i] != NULL) {
            *linkPoint = layerLists[i];
            linkPoint = &layerTails[i]->nextWindow;
        }
    }

    /* Update platform layer ordering */
    WM_UpdatePlatformWindowOrder();

    /* Recalculate visibility for all windows */
    WM_RecalculateAllVisibility();

    g_layerState.layersInvalid = false;
    g_layerState.updateCounter++;

    WM_DEBUG("WM_RecalculateWindowOrder: Window order recalculated");
}

static void WM_UpdatePlatformWindowOrder(void) {
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    WM_DEBUG("WM_UpdatePlatformWindowOrder: Updating platform window order");

    /* Update native window stacking order */
    while (current != NULL) {
        if (current->visible) {
            Platform_UpdateNativeWindowOrder(current);
        }
        current = current->nextWindow;
    }
}

/* ============================================================================
 * Window Visibility Calculation
 * ============================================================================ */

void WM_RecalculateAllVisibility(void) {
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    WM_DEBUG("WM_RecalculateAllVisibility: Recalculating visibility for all windows");

    while (current != NULL) {
        if (current->visible) {
            WM_CalculateWindowVisibility(current);
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_RecalculateAllVisibility: Visibility recalculation complete");
}

static void WM_CalculateWindowVisibility(WindowPtr window) {
    if (window == NULL || !window->visible) return;

    WM_DEBUG("WM_CalculateWindowVisibility: Calculating visibility for window");

    /* Start with the window's structure region */
    RgnHandle workingRgn = Platform_NewRgn();
    if (workingRgn == NULL) return;

    Platform_CopyRgn(window->strucRgn, workingRgn);

    /* Subtract regions of windows in front of this window */
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    while (current != window && current != NULL) {
        if (current->visible && current->strucRgn) {
            /* Check if windows overlap */
            if (WM_WindowsOverlap(current, window)) {
                Platform_DiffRgn(workingRgn, current->strucRgn, workingRgn);
            }
        }
        current = current->nextWindow;
    }

    /* Update window's visible region */
    if ((window)->visRgn) {
        Platform_CopyRgn(workingRgn, (window)->visRgn);
    }

    /* Calculate visibility statistics */
    WM_UpdateWindowVisibilityStats(window, workingRgn);

    Platform_DisposeRgn(workingRgn);

    WM_DEBUG("WM_CalculateWindowVisibility: Visibility calculated");
}

static Boolean WM_WindowsOverlap(WindowPtr window1, WindowPtr window2) {
    if (window1 == NULL || window2 == NULL) return false;
    if (window1->strucRgn == NULL || window2->strucRgn == NULL) return false;

    /* Test for region intersection */
    RgnHandle testRgn = Platform_NewRgn();
    if (testRgn == NULL) return false;

    Platform_IntersectRgn(window1->strucRgn, window2->strucRgn, testRgn);
    Boolean overlap = !Platform_EmptyRgn(testRgn);

    Platform_DisposeRgn(testRgn);
    return overlap;
}

static void WM_UpdateWindowVisibilityStats(WindowPtr window, RgnHandle visibleRgn) {
    if (window == NULL || visibleRgn == NULL) return;

    /* Calculate visibility percentage */
    long totalArea = WM_CalculateRegionArea(window->strucRgn);
    long visibleArea = WM_CalculateRegionArea(visibleRgn);

    short visibilityPercent = 0;
    if (totalArea > 0) {
        visibilityPercent = (short)((visibleArea * 100) / totalArea);
        if (visibilityPercent > 100) visibilityPercent = 100;
    }

    /* Store visibility information in auxiliary window record */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            /* Store visibility percentage in upper bits of awFlags */
            (**auxWin).awFlags &= 0x00FFFF; /* Clear visibility bits */
            (**auxWin).awFlags |= ((long)visibilityPercent << 16);
        }
    }

    WM_DEBUG("WM_UpdateWindowVisibilityStats: Window is %d%% visible", visibilityPercent);
}

static long WM_CalculateRegionArea(RgnHandle rgn) {
    if (rgn == NULL) return 0;

    /* Get region bounding rectangle and calculate area */
    Rect bounds;
    Platform_GetRegionBounds(rgn, &bounds);

    long width = bounds.right - bounds.left;
    long height = bounds.bottom - bounds.top;

    if (width < 0) width = 0;
    if (height < 0) height = 0;

    return width * height;
}

/* ============================================================================
 * Modal Window Management
 * ============================================================================ */

void WM_SetModalWindow(WindowPtr window) {
    WM_DEBUG("WM_SetModalWindow: Setting modal window");

    /* Remove previous modal window if any */
    if (g_layerState.modalWindow != NULL) {
        WM_ClearModalWindow();
    }

    /* Set new modal window */
    g_layerState.modalWindow = window;

    if (window != NULL) {
        /* Move modal window to modal layer */
        WM_SetWindowLayer(window, kLayerModal);

        /* Bring to front */
        BringToFront(window);

        /* Disable windows behind modal window */
        WM_DisableWindowsBehindModal(window);
    }

    /* Recalculate window order */
    WM_RecalculateWindowOrder();

    WM_DEBUG("WM_SetModalWindow: Modal window set");
}

void WM_ClearModalWindow(void) {
    if (g_layerState.modalWindow == NULL) return;

    WM_DEBUG("WM_ClearModalWindow: Clearing modal window");

    WindowPtr modalWindow = g_layerState.modalWindow;
    g_layerState.modalWindow = NULL;

    /* Re-enable windows */
    WM_EnableAllWindows();

    /* Move modal window back to normal layer */
    WM_SetWindowLayer(modalWindow, kLayerNormal);

    /* Recalculate window order */
    WM_RecalculateWindowOrder();

    WM_DEBUG("WM_ClearModalWindow: Modal window cleared");
}

WindowPtr WM_GetModalWindow(void) {
    return g_layerState.modalWindow;
}

static void WM_DisableWindowsBehindModal(WindowPtr modalWindow) {
    if (modalWindow == NULL) return;

    WM_DEBUG("WM_DisableWindowsBehindModal: Disabling windows behind modal");

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    /* Find the modal window in the list */
    while (current != NULL && current != modalWindow) {
        current = current->nextWindow;
    }

    /* Disable all windows after the modal window */
    if (current != NULL) {
        current = current->nextWindow;
        while (current != NULL) {
            if (current->visible) {
                Platform_DisableWindow(current);
            }
            current = current->nextWindow;
        }
    }

    WM_DEBUG("WM_DisableWindowsBehindModal: Windows disabled");
}

void WM_EnableAllWindows(void) {
    WM_DEBUG("WM_EnableAllWindows: Re-enabling all windows");

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    while (current != NULL) {
        if (current->visible) {
            Platform_EnableWindow(current);
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_EnableAllWindows: All windows enabled");
}

/* ============================================================================
 * Floating Window Management
 * ============================================================================ */

void WM_AddFloatingWindow(WindowPtr window) {
    if (window == NULL) return;

    WM_DEBUG("WM_AddFloatingWindow: Adding floating window");

    /* Mark window as floating */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            (**auxWin).awFlags |= 0x0001; /* Set floating flag */
        }
    }

    /* Set floating layer */
    WM_SetWindowLayer(window, kLayerFloating);

    /* Add to floating window list */
    /* TODO: Maintain separate floating window list if needed */

    /* Recalculate window order */
    WM_RecalculateWindowOrder();

    WM_DEBUG("WM_AddFloatingWindow: Floating window added");
}

void WM_RemoveFloatingWindow(WindowPtr window) {
    if (window == NULL) return;

    WM_DEBUG("WM_RemoveFloatingWindow: Removing floating window");

    /* Clear floating flag */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            (**auxWin).awFlags &= ~0x0001; /* Clear floating flag */
        }
    }

    /* Move to normal layer */
    WM_SetWindowLayer(window, kLayerNormal);

    /* Recalculate window order */
    WM_RecalculateWindowOrder();

    WM_DEBUG("WM_RemoveFloatingWindow: Floating window removed");
}

/* ============================================================================
 * Window Intersection and Overlap Detection
 * ============================================================================ */

Boolean WM_WindowIntersectsRect(WindowPtr window, const Rect* rect) {
    if (window == NULL || rect == NULL) return false;
    if (window->strucRgn == NULL) return false;

    /* Create temporary region for the rectangle */
    RgnHandle rectRgn = Platform_NewRgn();
    if (rectRgn == NULL) return false;

    Platform_SetRectRgn(rectRgn, rect);

    /* Test for intersection */
    RgnHandle testRgn = Platform_NewRgn();
    Boolean intersects = false;

    if (testRgn != NULL) {
        Platform_IntersectRgn(window->strucRgn, rectRgn, testRgn);
        intersects = !Platform_EmptyRgn(testRgn);
        Platform_DisposeRgn(testRgn);
    }

    Platform_DisposeRgn(rectRgn);
    return intersects;
}

void WM_GetWindowsInRect(const Rect* rect, WindowPtr* windows, short maxWindows, short* numWindows) {
    if (rect == NULL || windows == NULL || numWindows == NULL) return;

    *numWindows = 0;

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    while (current != NULL && *numWindows < maxWindows) {
        if (current->visible && WM_WindowIntersectsRect(current, rect)) {
            windows[*numWindows] = current;
            (*numWindows)++;
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_GetWindowsInRect: Found %d windows in rectangle", *numWindows);
}

WindowPtr WM_GetTopmostWindowInRect(const Rect* rect) {
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    while (current != NULL) {
        if (current->visible && WM_WindowIntersectsRect(current, rect)) {
            WM_DEBUG("WM_GetTopmostWindowInRect: Found topmost window");
            return current;
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_GetTopmostWindowInRect: No window found in rectangle");
    return NULL;
}

/* ============================================================================
 * Layer Update and Maintenance
 * ============================================================================ */

void WM_InvalidateLayerOrder(void) {
    WM_DEBUG("WM_InvalidateLayerOrder: Marking layer order as invalid");

    g_layerState.layersInvalid = true;

    /* Schedule layer recalculation */
    /* In a full implementation, this might be deferred to the next event loop */
    WM_RecalculateWindowOrder();
}

Boolean WM_LayersNeedUpdate(void) {
    return g_layerState.layersInvalid;
}

void WM_UpdateWindowLayers(void) {
    if (!g_layerState.layersInvalid) return;

    WM_DEBUG("WM_UpdateWindowLayers: Updating window layers");

    WM_RecalculateWindowOrder();

    WM_DEBUG("WM_UpdateWindowLayers: Layer update complete");
}

/* ============================================================================
 * Window Invalidation for Z-Order Changes
 * ============================================================================ */

/* [WM-051] Canonical implementation: invalidate windows below topWindow
 * Provenance: IM:Windows "Update Events" + "Window Ordering"
 * When a window moves/changes, windows behind it may need repainting
 */
void WM_InvalidateWindowsBelow(WindowPtr topWindow, const Rect* rect) {
    if (topWindow == NULL || rect == NULL) return;

    WM_DEBUG("WM_InvalidateWindowsBelow: Invalidating windows below specified window");

    /* Find windows below the top window and invalidate overlapping areas */
    WindowPtr current = topWindow->nextWindow;
    while (current) {
        if (current->visible && current->strucRgn) {
            /* Check if window intersects with invalid area */
            Rect windowBounds;
            Platform_GetRegionBounds(current->strucRgn, &windowBounds);

            Rect intersection;
            WM_IntersectRect(rect, &windowBounds, &intersection);

            if (!WM_EmptyRect(&intersection)) {
                /* Invalidate the intersecting area */
                Platform_InvalidateWindowRect(current, &intersection);
            }
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_InvalidateWindowsBelow: Invalidation complete");
}

/* ============================================================================
 * Platform Abstraction Functions for Layering
 * ============================================================================ */

/* [WM-050] Platform_* functions removed - implemented in WindowPlatform.c */

/* ============================================================================
 * Debug and Diagnostic Functions
 * ============================================================================ */

void WM_DumpWindowLayerInfo(void) {
    #ifdef DEBUG_WINDOW_MANAGER
    printf("\n=== Window Layer Information ===\n");

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;
    int windowIndex = 0;

    while (current != NULL) {
        WindowLayer layer = WM_GetWindowLayer(current);
        Boolean floating = WM_IsFloatingWindow(current);
        Boolean modal = (current == g_layerState.modalWindow);

        printf("Window %d: Layer=%d, Visible=%s, Floating=%s, Modal=%s\n",
               windowIndex++,
               layer,
               current->visible ? "Yes" : "No",
               floating ? "Yes" : "No",
               modal ? "Yes" : "No");

        current = current->nextWindow;
    }

    printf("Layer state: Invalid=%s, UpdateCounter=%lu\n",
           g_layerState.layersInvalid ? "Yes" : "No",
           g_layerState.updateCounter);

    printf("===============================\n\n");
    #endif
}
