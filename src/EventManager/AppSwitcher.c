/**
 * @file AppSwitcher.c
 * @brief Application Switcher Implementation (Command-Tab)
 *
 * Implements the classic Mac OS application switcher functionality.
 * Allows users to cycle through open applications using Command-Tab.
 *
 * Copyright (c) 2025 System 7.1 Portable Project
 */

#include "EventManager/AppSwitcher.h"
#include "ProcessMgr/ProcessMgr.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "Finder/Icon/icon_port.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

static AppSwitcherState gSwitcherState = {0};
static Boolean gSwitcherInitialized = false;
static Boolean gSwitcherVisible = false;
static UInt32 gHideTimeout = 0;
static UInt32 gHideTimeoutStart = 0;

/* External functions */
extern UInt32 TickCount(void);
extern OSErr ProcessManager_SetFrontProcess(ProcessSerialNumber psn);
extern ProcessSerialNumber ProcessManager_GetFrontProcess(void);
extern ProcessQueue* ProcessManager_GetProcessQueue(void);
extern void serial_printf(const char* fmt, ...);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Center a rectangle on the screen
 */
static void CenterRectOnScreen(Rect* rect, SInt16 width, SInt16 height) {
    if (!rect) return;

    /* Assume 800x600 screen for now */
    SInt16 screenWidth = 800;
    SInt16 screenHeight = 600;

    rect->left = (screenWidth - width) / 2;
    rect->top = (screenHeight - height) / 2;
    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

/**
 * Calculate layout position for app at index
 */
static Rect GetAppIconRect(SInt16 index, const Rect* windowBounds) {
    Rect rect = {0, 0, 0, 0};

    if (!windowBounds) return rect;

    /* Simple grid layout: 4 columns */
    const SInt16 iconsPerRow = 4;
    const SInt16 iconSize = kAppSwitcher_IconSize;
    const SInt16 spacing = 20;
    const SInt16 leftPadding = 20;
    const SInt16 topPadding = 40;

    SInt16 col = index % iconsPerRow;
    SInt16 row = index / iconsPerRow;

    rect.left = windowBounds->left + leftPadding + (col * (iconSize + spacing));
    rect.top = windowBounds->top + topPadding + (row * (iconSize + spacing));
    rect.right = rect.left + iconSize;
    rect.bottom = rect.top + iconSize;

    return rect;
}

/**
 * Get index of app at point
 */
static SInt16 GetAppAtPoint(Point pt, const Rect* windowBounds) {
    if (!windowBounds) return -1;

    const SInt16 iconsPerRow = 4;
    const SInt16 iconSize = kAppSwitcher_IconSize;
    const SInt16 spacing = 20;
    const SInt16 leftPadding = 20;
    const SInt16 topPadding = 40;

    SInt16 relX = pt.h - (windowBounds->left + leftPadding);
    SInt16 relY = pt.v - (windowBounds->top + topPadding);

    if (relX < 0 || relY < 0) return -1;

    SInt16 col = relX / (iconSize + spacing);
    SInt16 row = relY / (iconSize + spacing);

    SInt16 index = row * iconsPerRow + col;

    if (index < 0 || index >= gSwitcherState.appCount) return -1;

    return index;
}

/*---------------------------------------------------------------------------
 * Initialization
 *---------------------------------------------------------------------------*/

/**
 * Initialize application switcher
 */
OSErr AppSwitcher_Init(void) {
    extern void serial_puts(const char*);
    serial_puts("[AppSwitcher] Initializing\n");

    if (gSwitcherInitialized) {
        return noErr;
    }

    /* Allocate app list using Mac OS Memory Manager */
    gSwitcherState.apps = (SwitchableApp*)NewPtr(sizeof(SwitchableApp) * kAppSwitcher_MaxApps);
    if (!gSwitcherState.apps) {
        serial_puts("[AppSwitcher] ERROR: Failed to allocate app list\n");
        return memFullErr;
    }

    gSwitcherState.appCount = 0;
    gSwitcherState.currentIndex = 0;
    gSwitcherState.isActive = false;
    gSwitcherState.switcherPort = NULL;

    /* Update app list */
    OSErr err = AppSwitcher_UpdateAppList();
    if (err != noErr) {
        serial_puts("[AppSwitcher] WARNING: Failed to update app list\n");
        /* Continue anyway - list may be empty initially */
    }

    gSwitcherInitialized = true;
    serial_puts("[AppSwitcher] Initialized\n");

    return noErr;
}

/**
 * Shutdown application switcher
 */
void AppSwitcher_Shutdown(void) {
    if (!gSwitcherInitialized) return;

    AppSwitcher_HideWindow();

    if (gSwitcherState.apps) {
        DisposePtr((Ptr)gSwitcherState.apps);
        gSwitcherState.apps = NULL;
    }

    gSwitcherInitialized = false;
    serial_puts("[AppSwitcher] Shutdown\n");
}

/*---------------------------------------------------------------------------
 * Window Management
 *---------------------------------------------------------------------------*/

/**
 * Show switcher window
 */
Boolean AppSwitcher_ShowWindow(void) {
    if (!gSwitcherInitialized) {
        return false;
    }

    if (gSwitcherVisible) {
        /* Already visible */
        AppSwitcher_CancelHideTimeout();
        return true;
    }

    /* Update app list first */
    AppSwitcher_UpdateAppList();

    if (gSwitcherState.appCount == 0) {
        serial_printf("[AppSwitcher] No apps to switch\n");
        return false;
    }

    /* Set window bounds */
    CenterRectOnScreen(&gSwitcherState.windowBounds,
                       kAppSwitcher_WindowWidth,
                       kAppSwitcher_WindowHeight);

    gSwitcherState.windowPos.h = gSwitcherState.windowBounds.left;
    gSwitcherState.windowPos.v = gSwitcherState.windowBounds.top;

    gSwitcherVisible = true;
    gSwitcherState.isActive = true;
    gSwitcherState.activationTime = TickCount();

    /* Mark display as needing update so WM_Update will render the switcher */
    extern void WM_InvalidateDisplay_Public(void);
    WM_InvalidateDisplay_Public();

    serial_printf("[AppSwitcher] Window shown with %d apps\n", gSwitcherState.appCount);

    return true;
}

/**
 * Hide switcher window
 */
void AppSwitcher_HideWindow(void) {
    if (!gSwitcherVisible) return;

    gSwitcherVisible = false;
    gSwitcherState.isActive = false;
    gHideTimeout = 0;

    /* Mark display as needing update so switcher gets erased */
    extern void WM_InvalidateDisplay_Public(void);
    WM_InvalidateDisplay_Public();

    serial_printf("[AppSwitcher] Window hidden\n");
}

/**
 * Check if switcher is active
 */
Boolean AppSwitcher_IsActive(void) {
    return gSwitcherState.isActive;
}

/*---------------------------------------------------------------------------
 * Cycling & Selection
 *---------------------------------------------------------------------------*/

/**
 * Cycle to next application
 */
void AppSwitcher_CycleForward(void) {
    if (!gSwitcherInitialized) return;

    if (gSwitcherState.appCount == 0) return;

    /* Show window if not visible */
    if (!gSwitcherVisible) {
        AppSwitcher_ShowWindow();
    } else {
        /* Move to next app */
        gSwitcherState.currentIndex++;
        if (gSwitcherState.currentIndex >= gSwitcherState.appCount) {
            gSwitcherState.currentIndex = 0;
        }
    }

    /* Reset hide timeout */
    AppSwitcher_CancelHideTimeout();
    AppSwitcher_StartHideTimeout(kAppSwitcher_HideTimeout);

    serial_printf("[AppSwitcher] Cycle forward: index=%d\n", gSwitcherState.currentIndex);
}

/**
 * Cycle to previous application
 */
void AppSwitcher_CycleBackward(void) {
    if (!gSwitcherInitialized) return;

    if (gSwitcherState.appCount == 0) return;

    /* Show window if not visible */
    if (!gSwitcherVisible) {
        AppSwitcher_ShowWindow();
    } else {
        /* Move to previous app */
        gSwitcherState.currentIndex--;
        if (gSwitcherState.currentIndex < 0) {
            gSwitcherState.currentIndex = gSwitcherState.appCount - 1;
        }
    }

    /* Reset hide timeout */
    AppSwitcher_CancelHideTimeout();
    AppSwitcher_StartHideTimeout(kAppSwitcher_HideTimeout);

    serial_printf("[AppSwitcher] Cycle backward: index=%d\n", gSwitcherState.currentIndex);
}

/**
 * Switch to currently selected app
 */
void AppSwitcher_SelectCurrent(void) {
    if (!gSwitcherInitialized || !gSwitcherVisible) return;

    if (gSwitcherState.currentIndex < 0 || gSwitcherState.currentIndex >= gSwitcherState.appCount) {
        return;
    }

    SwitchableApp* selectedApp = &gSwitcherState.apps[gSwitcherState.currentIndex];

    /* Switch to selected app */
    OSErr err = ProcessManager_SetFrontProcess(selectedApp->psn);
    if (err != noErr) {
        serial_printf("[AppSwitcher] ERROR: Failed to set front process (err=%d)\n", err);
    } else {
        serial_printf("[AppSwitcher] Switched to app: %s\n", selectedApp->appName);
    }

    /* Hide switcher */
    AppSwitcher_HideWindow();
}

/**
 * Handle Key Up event
 */
void AppSwitcher_HandleKeyUp(void) {
    if (!gSwitcherVisible) return;

    AppSwitcher_SelectCurrent();
}

/*---------------------------------------------------------------------------
 * Drawing
 *---------------------------------------------------------------------------*/

/**
 * Draw switcher window
 */
void AppSwitcher_Draw(void) {
    if (!gSwitcherVisible || !gSwitcherInitialized) return;

    Rect windowBounds = gSwitcherState.windowBounds;

    /* Draw window background with light gray fill */
    ForeColor(0xCCCCCC);  /* Light gray */
    PaintRect(&windowBounds);

    /* Draw window border - thick black frame */
    ForeColor(0x000000);  /* Black */
    FrameRect(&windowBounds);

    /* Draw inner border for 3D effect */
    Rect innerBorder = windowBounds;
    InsetRect(&innerBorder, 1, 1);
    FrameRect(&innerBorder);

    /* Draw title bar background */
    Rect titleBar = windowBounds;
    titleBar.bottom = titleBar.top + 25;
    ForeColor(0x666666);  /* Dark gray */
    PaintRect(&titleBar);

    /* Draw title text */
    ForeColor(0xFFFFFF);  /* White */
    MoveTo(windowBounds.left + 8, windowBounds.top + 18);

    Str255 title;
    title[0] = 19;  /* Length */
    BlockMoveData("Application Switcher", title + 1, 19);
    DrawString(title);

    /* Draw content area background */
    Rect contentArea = windowBounds;
    contentArea.top = titleBar.bottom + 1;
    contentArea.left += 10;
    contentArea.right -= 10;
    contentArea.bottom -= 10;

    ForeColor(0xCCCCCC);  /* Light gray */
    PaintRect(&contentArea);

    /* Draw grid of app icons and names */
    SInt16 col = 0, row = 0;
    const SInt16 maxCols = 3;
    const SInt16 itemWidth = 80;
    const SInt16 itemHeight = 90;
    const SInt16 startX = contentArea.left + 10;
    const SInt16 startY = contentArea.top + 10;

    for (SInt16 i = 0; i < gSwitcherState.appCount; i++) {
        SwitchableApp* app = &gSwitcherState.apps[i];

        /* Calculate position */
        SInt16 x = startX + (col * itemWidth);
        SInt16 y = startY + (row * itemHeight);

        /* Icon rectangle */
        Rect iconRect;
        iconRect.left = x;
        iconRect.top = y;
        iconRect.right = x + 32;
        iconRect.bottom = y + 32;

        /* Draw icon background as white box */
        ForeColor(0xFFFFFF);  /* White */
        PaintRect(&iconRect);

        /* Draw icon border */
        ForeColor(0x000000);  /* Black */
        FrameRect(&iconRect);

        /* Try to render actual app icon if available (loaded on-demand) */
        Boolean iconRendered = false;
        if (app->iconID > 0) {
            /* Load icon on-demand (temporary, not persistent) */
            extern bool IconGen_FindByID(int16_t id, IconFamily* out);
            IconFamily tempIcon = {0};

            if (IconGen_FindByID(app->iconID, &tempIcon) &&
                tempIcon.large.argb32 && tempIcon.large.w >= 16 && tempIcon.large.h >= 16) {
                /* Render a small version of the icon (16x16 from top-left of 32x32) */
                Rect innerRect = iconRect;
                InsetRect(&innerRect, 2, 2);  /* Leave border */

                /* Simple pixel blit from icon data */
                for (SInt16 iy = 0; iy < 16 && (iy + innerRect.top) < innerRect.bottom; iy++) {
                    for (SInt16 ix = 0; ix < 16 && (ix + innerRect.left) < innerRect.right; ix++) {
                        UInt32 pixel = tempIcon.large.argb32[iy * tempIcon.large.w + ix];
                        UInt8 alpha = (pixel >> 24) & 0xFF;

                        /* Only draw if not fully transparent */
                        if (alpha > 0x80) {
                            UInt8 r = (pixel >> 16) & 0xFF;
                            UInt8 g = (pixel >> 8) & 0xFF;
                            UInt8 b = pixel & 0xFF;
                            UInt32 color = (r << 16) | (g << 8) | b;
                            ForeColor(color);

                            /* Draw single pixel using pen */
                            MoveTo(innerRect.left + ix, innerRect.top + iy);
                            LineTo(innerRect.left + ix + 1, innerRect.top + iy);
                        }
                    }
                }
                iconRendered = true;
            }
        }

        /* Draw fallback generic icon if no actual icon */
        if (!iconRendered) {
            Rect iconInner = iconRect;
            InsetRect(&iconInner, 4, 4);
            ForeColor(0xCCCCCC);  /* Light gray */
            PaintRect(&iconInner);
        }

        /* Highlight current selection with thick border */
        if (i == gSwitcherState.currentIndex) {
            ForeColor(0x000000);  /* Black */
            FrameRect(&iconRect);
            Rect highlightRect = iconRect;
            InsetRect(&highlightRect, -2, -2);
            FrameRect(&highlightRect);
        }

        /* Draw app name below icon */
        SInt16 nameY = iconRect.bottom + 6;
        MoveTo(x, nameY);

        ForeColor(0x000000);  /* Black */

        /* Draw app name (truncate if needed) */
        Str255 displayName;
        BlockMoveData(app->appName, displayName, sizeof(Str255));
        if (displayName[0] > 10) {
            displayName[0] = 10;  /* Truncate to 10 chars */
        }
        DrawString(displayName);

        /* Move to next position */
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }

        /* Stop if we're off the bottom */
        if (startY + ((row + 1) * itemHeight) > contentArea.bottom) {
            break;
        }
    }

    /* Draw status text at bottom */
    Rect statusArea = windowBounds;
    statusArea.top = statusArea.bottom - 20;
    statusArea.left += 10;
    statusArea.right -= 10;

    ForeColor(0x666666);  /* Dark gray */
    PaintRect(&statusArea);

    ForeColor(0xFFFFFF);  /* White */
    MoveTo(statusArea.left + 5, statusArea.top + 14);

    Str255 status;
    status[0] = 21;  /* Length */
    BlockMoveData("Release Tab to switch", status + 1, 21);
    DrawString(status);
}

/*---------------------------------------------------------------------------
 * App List Management
 *---------------------------------------------------------------------------*/

/**
 * Update application list from ProcessManager
 */
OSErr AppSwitcher_UpdateAppList(void) {
    if (!gSwitcherInitialized) return noErr;

    /* Get process queue from ProcessManager */
    ProcessQueue* procQueue = ProcessManager_GetProcessQueue();
    if (!procQueue) {
        gSwitcherState.appCount = 0;
        return noErr;
    }

    /* Iterate through process list and populate app list */
    ProcessControlBlock* process = procQueue->queueHead;
    SInt16 count = 0;

    while (process && count < kAppSwitcher_MaxApps) {
        /* Skip system process and terminated processes */
        if (process->processState == kProcessTerminated ||
            process->processSignature == 'MACS') {  /* Skip system process */
            process = process->processNextProcess;
            continue;
        }

        SwitchableApp* app = &gSwitcherState.apps[count];

        /* Copy process info */
        app->psn = process->processID;
        app->signature = process->processSignature;

        /* Get application name */
        Str255 name;
        name[0] = 0;
        SInt16 iconID = 0;

        if (process->processSignature == 'FNDR') {
            name[0] = 6;
            BlockMoveData("Finder", name + 1, 6);
            iconID = 999;  /* Custom Finder icon */
        } else if (process->processSignature == 'ttxt') {
            name[0] = 9;
            BlockMoveData("TeachText", name + 1, 9);
            iconID = 130;  /* TeachText icon */
        } else {
            /* Generic app name from signature */
            name[0] = 4;
            name[1] = (process->processSignature >> 24) & 0xFF;
            name[2] = (process->processSignature >> 16) & 0xFF;
            name[3] = (process->processSignature >> 8) & 0xFF;
            name[4] = process->processSignature & 0xFF;
            iconID = 128;  /* Generic app icon */
        }
        BlockMoveData(name, app->appName, name[0] + 1);

        /* Store icon ID for on-demand loading (NOT persistent storage) */
        app->iconID = iconID;  /* Just store ID, load icon when drawing */

        app->isHidden = (process->processState == kProcessBackground);
        app->isSystemProcess = false;

        count++;
        process = process->processNextProcess;
    }

    gSwitcherState.appCount = count;

    /* Clamp current index if necessary */
    if (gSwitcherState.currentIndex >= gSwitcherState.appCount) {
        gSwitcherState.currentIndex = 0;
    }

    serial_printf("[AppSwitcher] Updated app list: %d apps\n", count);

    return noErr;
}

/**
 * Get currently selected app
 */
ProcessSerialNumber AppSwitcher_GetSelectedApp(SwitchableApp* outApp) {
    ProcessSerialNumber psn = {0, 0};

    if (!gSwitcherInitialized || gSwitcherState.currentIndex < 0 ||
        gSwitcherState.currentIndex >= gSwitcherState.appCount) {
        return psn;
    }

    SwitchableApp* selectedApp = &gSwitcherState.apps[gSwitcherState.currentIndex];

    if (outApp) {
        *outApp = *selectedApp;
    }

    return selectedApp->psn;
}

/**
 * Get app count
 */
SInt16 AppSwitcher_GetAppCount(void) {
    if (!gSwitcherInitialized) return 0;
    return gSwitcherState.appCount;
}

/**
 * Get app at index
 */
Boolean AppSwitcher_GetAppAt(SInt16 index, SwitchableApp* outApp) {
    if (!gSwitcherInitialized || !outApp) return false;
    if (index < 0 || index >= gSwitcherState.appCount) return false;

    *outApp = gSwitcherState.apps[index];
    return true;
}

/*---------------------------------------------------------------------------
 * Timeout Management
 *---------------------------------------------------------------------------*/

/**
 * Start hide timeout
 */
void AppSwitcher_StartHideTimeout(UInt32 timeoutMs) {
    gHideTimeout = timeoutMs;
    gHideTimeoutStart = TickCount();
}

/**
 * Cancel hide timeout
 */
void AppSwitcher_CancelHideTimeout(void) {
    gHideTimeout = 0;
    gHideTimeoutStart = 0;
}

/**
 * Check and process hide timeout
 * Called periodically during idle time
 */
void AppSwitcher_CheckHideTimeout(void) {
    if (gHideTimeout == 0 || !gSwitcherVisible) return;

    UInt32 elapsed = TickCount() - gHideTimeoutStart;

    if (elapsed >= (gHideTimeout / 16)) {  /* Convert milliseconds to ticks */
        AppSwitcher_HideWindow();
    }
}
