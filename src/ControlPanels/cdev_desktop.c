/*
 * RE-AGENT-BANNER
 * Desktop Patterns Control Panel (cdev)
 * System 7.1 Desktop Pattern Selector
 *
 * Control Panel for selecting desktop patterns.
 * Shows a grid of available patterns and allows the user to choose one.
 * Changes are applied immediately and saved to PRAM.
 */

#include "ControlPanels/DesktopPatterns.h"
#include "PatternMgr/pattern_manager.h"
#include "PatternMgr/pattern_resources.h"
#include "ControlManager/ControlManager.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "ColorManager.h"
#include "FontManager/FontManager.h"
#include "FontManager/FontInternal.h"
#include "DialogManager/DialogManager.h"
#include "System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Layout constants */
#define GRID_COLS 8
#define GRID_ROWS 4
#define CELL_W    32
#define CELL_H    32
#define CELL_PAD   8
#define WINDOW_MARGIN 16

/* External QuickDraw globals */
extern QDGlobals qd;

/* Control button type */
#define pushButProc 0

/* Global state for the control panel */
static WindowPtr gDesktopCdevWin = NULL;
static GrafPtr gDesktopPrevPort = NULL;
static ControlHandle gOKButton = NULL;
static ControlHandle gCancelButton = NULL;
static int16_t gSelectedPatID = 16;
static Pattern gOriginalPattern;
static RGBColor gOriginalColor;
static DesktopPref gOriginalPref;

/* Forward declarations */
static void DrawPatternCell(int col, int row, int16_t patID, bool selected);
static void DrawPatternGrid(void);
static int16_t GetPatternIDAtPosition(Point pt);
static void ApplySelectedPattern(void);
static void RestoreOriginalPattern(void);

/*
 * OpenDesktopCdev - Open the Desktop Patterns control panel
 */
void OpenDesktopCdev(void) {
    serial_puts("[CDEV] OpenDesktopCdev start\n");
    if (gDesktopCdevWin != NULL) {
        /* Already open, bring to front */
        serial_puts("[CDEV] Window already open, selecting\n");
        SelectWindow(gDesktopCdevWin);
        return;
    }

    serial_puts("[CDEV] Creating new desktop patterns window\n");
    /* Calculate window size */
    Rect winRect;
    winRect.top = 50;
    winRect.left = 50;
    winRect.bottom = winRect.top + 40 + (GRID_ROWS * (CELL_H + CELL_PAD)) + 60;
    winRect.right = winRect.left + (GRID_COLS * (CELL_W + CELL_PAD)) + 32;

    /* Create the window */
    static unsigned char winTitle[] = {17, 'D','e','s','k','t','o','p',' ','P','a','t','t','e','r','n','s'};
    gDesktopCdevWin = NewWindow(NULL, &winRect, winTitle,
                                 true, documentProc, (WindowPtr)-1L, true, 0);
    if (!gDesktopCdevWin) {
        serial_puts("[CDEV] NewWindow failed\n");
        return;
    }

    serial_puts("[CDEV] Window created, setting port\n");
    GetPort(&gDesktopPrevPort);
    SetPort((GrafPtr)gDesktopCdevWin);

    /* Ensure consistent black/white rendering for 1-bit patterns */
    ForeColor(blackColor);
    BackColor(whiteColor);

    /* Create OK and Cancel buttons - use LOCAL window coordinates */
    Rect portRect = gDesktopCdevWin->port.portRect;
    Rect buttonRect;
    buttonRect.bottom = portRect.bottom - 20;
    buttonRect.top = buttonRect.bottom - 20;
    buttonRect.right = portRect.right - 20;
    buttonRect.left = buttonRect.right - 80;

    serial_puts("[CDEV] Creating buttons\n");
    static unsigned char okTitle[] = {2, 'O','K'};
    gOKButton = NewControl(gDesktopCdevWin, &buttonRect, okTitle,
                           true, 0, 0, 1, pushButProc, 0);

    buttonRect.right = buttonRect.left - 10;
    buttonRect.left = buttonRect.right - 80;
    static unsigned char cancelTitle[] = {6, 'C','a','n','c','e','l'};
    gCancelButton = NewControl(gDesktopCdevWin, &buttonRect, cancelTitle,
                               true, 0, 0, 1, pushButProc, 0);

    serial_puts("[CDEV] Loading preferences\n");
    /* Save current pattern so we can restore on cancel */
    gOriginalPref = PM_GetSavedDesktopPref();
    serial_puts("[CDEV] Getting background pattern\n");
    PM_GetBackPat(&gOriginalPattern);
    serial_puts("[CDEV] Getting background color\n");
    PM_GetBackColor(&gOriginalColor);

    /* Align Color Manager state with the current desktop colors */
    serial_puts("[CDEV] Initializing ColorManager\n");
    if (ColorManager_Init() == noErr) {
        ColorManager_SetBackground(&gOriginalColor);
        ColorManager_CommitQuickDraw();
    }
    gSelectedPatID = gOriginalPref.patID;
    if (gSelectedPatID < 16 || gSelectedPatID > 47) {
        gSelectedPatID = 16;
    }

    serial_puts("[CDEV] Drawing pattern grid\n");
    /* Draw the window contents */
    DrawPatternGrid();
    serial_puts("[CDEV] Drawing controls\n");
    DrawControls(gDesktopCdevWin);
    serial_puts("[CDEV] Showing window\n");
    ShowWindow(gDesktopCdevWin);
    serial_puts("[CDEV] OpenDesktopCdev complete\n");
}

/*
 * CloseDesktopCdev - Close the control panel
 */
void CloseDesktopCdev(void) {
    serial_puts("[CDC] Start\n");
    if (gDesktopCdevWin) {
        serial_puts("[CDC] Restoring port\n");
        if (gDesktopPrevPort) {
            SetPort(gDesktopPrevPort);
            gDesktopPrevPort = NULL;
        }
        serial_puts("[CDC] About to dispose window\n");
        DisposeWindow(gDesktopCdevWin);
        serial_puts("[CDC] Window disposed\n");
        gDesktopCdevWin = NULL;
        gOKButton = NULL;
        gCancelButton = NULL;

        /* The control panel is closing, clear out any dirty Color Manager state */
        serial_puts("[CDC] Cleaning up ColorManager\n");
        if (ColorManager_IsAvailable()) {
            serial_puts("[CDC] CM available, setting background\n");
            ColorManager_SetBackground(&gOriginalColor);
            serial_puts("[CDC] CM background set\n");
            ColorManager_CommitQuickDraw();
            serial_puts("[CDC] CM committed\n");
            /* ColorManager_Shutdown(); */ /* DISABLED: Testing if this causes freeze */
        }
    }
    serial_puts("[CDC] Complete\n");
}

/*
 * HandleDesktopCdevEvent - Handle events for the Desktop Patterns control panel
 */
Boolean DesktopPatterns_HandleEvent(EventRecord *event) {
    if (!event || !gDesktopCdevWin) {
        return false;
    }

    WindowPtr whichWindow;
    Point where;
    ControlHandle control;
    int16_t part;

    switch (event->what) {
        case updateEvt:
            serial_puts("[CDEV-EVT] Update event\n");
            if ((WindowPtr)(uintptr_t)event->message != gDesktopCdevWin) {
                return false;
            }
            BeginUpdate(gDesktopCdevWin);
            SetPort((GrafPtr)gDesktopCdevWin);
            DrawPatternGrid();
            DrawControls(gDesktopCdevWin);
            EndUpdate(gDesktopCdevWin);
            return true;

        case mouseDown:
            serial_puts("[CDEV-EVT] Mouse down event\n");
            part = FindWindow(event->where, &whichWindow);
            if (whichWindow == gDesktopCdevWin) {
                switch (part) {
                    case inContent:
                        SelectWindow(gDesktopCdevWin);
                        SetPort((GrafPtr)gDesktopCdevWin);
                        where = event->where;
                        GlobalToLocal(&where);

                        /* Check if click is on a control */
                        part = FindControl(where, gDesktopCdevWin, &control);
                        if (part && control) {
                            serial_puts("[CDEV-EVT] Control found, tracking\n");
                            if (TrackControl(control, where, NULL)) {
                                serial_puts("[CDEV-EVT] Control tracked successfully\n");
                                if (control == gOKButton) {
                                    serial_puts("[OK] Button tracked\n");
                                    /* Save and apply the selected pattern */
                                    serial_puts("[OK] Calling ApplySelectedPattern\n");
                                    ApplySelectedPattern();
                                    serial_puts("[OK] ApplySelectedPattern done\n");
                                    serial_puts("[OK] Calling CloseDesktopCdev\n");
                                    CloseDesktopCdev();
                                    serial_puts("[OK] CloseDesktopCdev done\n");
                                } else if (control == gCancelButton) {
                                    serial_puts("[CDEV-EVT] Cancel button clicked\n");
                                    /* Restore original pattern and close */
                                    RestoreOriginalPattern();
                                    CloseDesktopCdev();
                                }
                            } else {
                                serial_puts("[CDEV-EVT] Control tracking returned false\n");
                            }
                        } else {
                            /* Check if click is on a pattern cell */
                            int16_t patID = GetPatternIDAtPosition(where);
                            if (patID != 0 && patID != gSelectedPatID) {
                                gSelectedPatID = patID;
                                serial_puts("[CDEV-EVT] Pattern selected\n");

                                /* Apply pattern immediately for preview on the DESKTOP, not the window */
                                Pattern pat;
                                if (PM_LoadPAT(patID, &pat)) {
                                    /* Switch to desktop port to apply pattern there */
                                    GrafPtr savedPort;
                                    GetPort(&savedPort);
                                    if (gDesktopPrevPort) {
                                        SetPort(gDesktopPrevPort);
                                    } else {
                                        SetPort(NULL);  /* Switch to main screen port */
                                    }
                                    PM_SetBackPat(&pat);
                                    SetPort(savedPort);  /* Restore window port */
                                }

                                /* Redraw the grid to show new selection */
                                DrawPatternGrid();
                                DrawControls(gDesktopCdevWin);
                            }
                        }
                        break;

                    case inDrag:
                        DragWindow(gDesktopCdevWin, event->where, &qd.screenBits.bounds);
                        break;

                    case inGoAway:
                        serial_puts("[CDEV-EVT] Close button clicked, tracking go-away\n");
                        if (TrackGoAway(gDesktopCdevWin, event->where)) {
                            serial_puts("[CDEV-EVT] Go-away tracked, closing\n");
                            RestoreOriginalPattern();
                            CloseDesktopCdev();
                        } else {
                            serial_puts("[CDEV-EVT] Go-away tracking returned false\n");
                        }
                        break;
                }
            }
            return true;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gDesktopCdevWin) {
                /* Future: highlight controls if needed */
                return true;
            }
            return false;

        default:
            break;
    }

    return false;
}

/*
 * DrawPatternCell - Draw a single pattern cell in the grid
 */
static void DrawPatternCell(int col, int row, int16_t patID, bool selected) {
    Rect cellRect;
    cellRect.left = WINDOW_MARGIN + col * (CELL_W + CELL_PAD);
    cellRect.top = 40 + row * (CELL_H + CELL_PAD);
    cellRect.right = cellRect.left + CELL_W;
    cellRect.bottom = cellRect.top + CELL_H;

    /* Draw border */
    if (selected) {
        /* Highlight selected pattern */
        PenSize(2, 2);
        FrameRect(&cellRect);
        PenSize(1, 1);
    } else {
        FrameRect(&cellRect);
    }

    /* Create interior rect for fill - don't modify the original */
    Rect fillRect = cellRect;
    InsetRect(&fillRect, 1, 1);

    /* Fill with pattern */
    Pattern pat;
    if (LoadPATResource(patID, &pat)) {
        FillRect(&fillRect, &pat);
    } else {
        /* Fallback: lightly shade missing pattern - pattern load failed */
        Pattern fallback = qd.ltGray;
        FillRect(&fillRect, &fallback);
    }
}

/*
 * DrawPatternGrid - Draw the entire grid of patterns
 */
static void DrawPatternGrid(void) {
    serial_puts("[CDEV-GRID] DrawPatternGrid start\n");
    if (!gDesktopCdevWin) {
        serial_puts("[CDEV-GRID] No window, returning\n");
        return;
    }

    SetPort((GrafPtr)gDesktopCdevWin);

    /* Clear the window */
    Rect winRect;
    winRect = gDesktopCdevWin->port.portRect;
    serial_puts("[CDEV-GRID] Erasing rect\n");
    EraseRect(&winRect);

    /* Draw title */
    MoveTo(WINDOW_MARGIN, 25);
    static unsigned char titleStr[] = {24, 'S','e','l','e','c','t',' ','D','e','s','k','t','o','p',' ','P','a','t','t','e','r','n',':'};
    serial_puts("[CDEV-GRID] Drawing title\n");
    DrawString(titleStr);

    /* Draw pattern grid */
    /* Start with standard pattern IDs */
    int16_t patID = 16;  /* Start with kDesktopPatternID */
    serial_puts("[CDEV-GRID] Starting grid drawing loop\n");

    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            DrawPatternCell(col, row, patID, (patID == gSelectedPatID));
            patID++;
        }
    }
    serial_puts("[CDEV-GRID] DrawPatternGrid complete\n");
}

/*
 * GetPatternIDAtPosition - Determine which pattern was clicked
 */
static int16_t GetPatternIDAtPosition(Point pt) {
    /* Check if point is in grid area */
    if (pt.h < WINDOW_MARGIN || pt.v < 40) {
        return 0;
    }

    int col = (pt.h - WINDOW_MARGIN) / (CELL_W + CELL_PAD);
    int row = (pt.v - 40) / (CELL_H + CELL_PAD);

    if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS) {
        /* Check if click is actually within cell (not in padding) */
        int cellX = WINDOW_MARGIN + col * (CELL_W + CELL_PAD);
        int cellY = 40 + row * (CELL_H + CELL_PAD);

        if (pt.h >= cellX && pt.h < cellX + CELL_W &&
            pt.v >= cellY && pt.v < cellY + CELL_H) {
            return 16 + (row * GRID_COLS) + col;
        }
    }

    return 0;
}

/*
 * ApplySelectedPattern - Apply and save the selected pattern
 */
static void ApplySelectedPattern(void) {
    serial_puts("[ASP] Start\n");
    if (gSelectedPatID == 0) {
        serial_puts("[ASP] Invalid ID, returning\n");
        return;
    }

    /* Update the preference */
    serial_puts("[ASP] Creating pref\n");
    DesktopPref pref = gOriginalPref;
    pref.usePixPat = false;
    pref.patID = gSelectedPatID;

    /* Save to PRAM - this persists the user's choice */
    serial_puts("[ASP] About to save pref\n");
    PM_SaveDesktopPref(&pref);
    serial_puts("[ASP] Pref saved\n");
    gOriginalPref = pref;

    serial_puts("[ASP] Done\n");
    /* Don't apply pattern here - it's already being shown as preview on the desktop.
     * Just save the preference and close the window. The Finder will update the
     * desktop when it gets the notification. Applying here would try to set the
     * pattern on the applet window's port, which causes issues. */
}

/*
 * RestoreOriginalPattern - Restore the original pattern (for cancel)
 */
static void RestoreOriginalPattern(void) {
    serial_puts("[CDEV] RestoreOriginalPattern start\n");
    PM_SetBackPat(&gOriginalPattern);
    PM_SetBackColor(&gOriginalColor);
    serial_puts("[CDEV] Colors set, committing\n");
    if (ColorManager_IsAvailable()) {
        ColorManager_SetBackground(&gOriginalColor);
        ColorManager_CommitQuickDraw();
    }
    serial_puts("[CDEV] RestoreOriginalPattern complete\n");
}

Boolean DesktopPatterns_IsWindow(WindowPtr window) {
    return window != NULL && window == gDesktopCdevWin;
}

WindowPtr DesktopPatterns_GetWindow(void) {
    return gDesktopCdevWin;
}
