/*
 * MacPaint_EventLoop.c - Main Event Loop and Window Management
 *
 * Implements the main application event loop for MacPaint:
 * - WaitNextEvent-based event dispatching
 * - Window creation and management
 * - Mouse event tracking and tool routing
 * - Keyboard event handling
 * - Menu command processing
 * - Window update and redraw
 *
 * Integrates with System 7.1 EventManager, WindowManager, MenuManager
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "WindowManager/WindowManager.h"
#include "DialogManager/DialogManager.h"
#include "MenuManager/MenuManager.h"
#include "EventManager/EventManager.h"
#include "System71StdLib.h"
#include "sys71_stubs.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

/*
 * EVENT TYPE CONSTANTS (System 7.1 compatible)
 * Local constants for event types - use local names to avoid conflicts
 */

enum {
    kEventMouseDown   = 1,
    kEventMouseUp     = 2,
    kEventKeyDown     = 3,
    kEventAutoKey     = 5,
    kEventUpdateEvt   = 6,
    kEventOSEvt       = 15,
    kEventNullEvent   = 0,
    kEventQuitEvent   = 0xFFFFFFF,
    kEventCloseEvt    = 8
};

/* MenuResult type for menu selection return */
typedef UInt32 MenuResult;

/* Suspend/resume messages */
#define kSuspendResumeMessage 0x01
#define kResumeFlag 1

/*
 * APPLICATION STATE FOR EVENT LOOP
 */

typedef struct {
    int running;                /* Application should continue running */
    int mouseDown;              /* Mouse button currently pressed */
    int lastMouseX, lastMouseY; /* Last mouse position for drag tracking */
    int dragInProgress;         /* Currently dragging (mouse down + moved) */
    WindowPtr paintWindow;      /* Main paint window */
    int windowNeedsRedraw;      /* Window should be redrawn */
    int updateRect;             /* Rect that needs updating */
} EventLoopState;

static EventLoopState gEventState = {1, 0, 0, 0, 0, 0, 0, 0};

/* External state from MacPaint modules */
extern WindowPtr gPaintWindow;
extern int gCurrentTool;
extern int gDocDirty;

/*
 * WINDOW MANAGEMENT FOR EVENT LOOP
 */

/**
 * MacPaint_CreateMainWindow - Create the main paint canvas window
 */
OSErr MacPaint_CreateMainWindow(void)
{
    Rect bounds;
    extern WindowPtr NewWindow(void *, const Rect *, ConstStr255Param, Boolean, short,
                              WindowPtr, Boolean, long);
    extern void ShowWindow(WindowPtr);

    /* Create window bounds - centered on screen with reasonable size */
    bounds.left = 40;
    bounds.top = 80;
    bounds.right = bounds.left + (MACPAINT_DOC_WIDTH + MACPAINT_TOOLBOX_WIDTH);
    bounds.bottom = bounds.top + 480;

    /* Create a document window */
    Str255 title;
    const char* titleStr = "MacPaint";
    size_t len = strlen(titleStr);
    if (len > 255) len = 255;
    title[0] = (unsigned char)len;
    memcpy(&title[1], titleStr, len);

    gPaintWindow = NewWindow(NULL, &bounds, title, true, 0 /* documentProc */,
                            (WindowPtr)-1, true, 'PANT');

    if (!gPaintWindow) {
        return memFullErr;
    }

    gEventState.paintWindow = gPaintWindow;
    gEventState.windowNeedsRedraw = 1;
    MacPaint_RegisterMainWindow(gPaintWindow);

    ShowWindow(gPaintWindow);

    return noErr;
}

/**
 * MacPaint_DrawPaintWindow - Render paint buffer to window
 */
void MacPaint_DrawPaintWindow(void)
{
    if (!gEventState.paintWindow) {
        return;
    }

    /* TODO: Render paint buffer to window
     * - Get current GrafPort for window
     * - Set drawing parameters (mode, pen size)
     * - Use CopyBits or similar to draw bitmap
     * - Draw selection rectangle if active
     * - Draw grid if enabled
     * - Draw toolbox if visible
     */
}

/**
 * MacPaint_HandleWindowUpdate - Process window update event
 */
void MacPaint_HandleWindowUpdate(WindowPtr window)
{
    if (window == gEventState.paintWindow) {
        /* Use the rendering system's full window update
         * This handles all invalidation regions properly
         */
        MacPaint_FullWindowUpdate();

        /* Clear invalidation state after redraw */
        MacPaint_ClearInvalidState();
    }
}

/**
 * MacPaint_HandleWindowClose - Handle window close button click
 */
void MacPaint_HandleWindowClose(WindowPtr window)
{
    if (window == gEventState.paintWindow) {
        /* Check if document needs saving */
        int promptResult = MacPaint_PromptSaveChanges();
        if (promptResult == 1) {
            /* User wants to save */
            MacPaint_FileSave();
        } else if (promptResult == 2) {
            /* User cancelled */
            return;
        }

        /* Close the window */
        gEventState.paintWindow = NULL;
        gEventState.running = 0;
    }
}

/**
 * MacPaint_HandleWindowResize - Handle window resize or bounds change
 * Called when the window is resized to recalculate invalidation regions
 */
void MacPaint_HandleWindowResize(WindowPtr window)
{
    if (window == gEventState.paintWindow) {
        /* Invalidate entire window to force complete redraw at new size
         * This call automatically recalculates invalidation region bounds
         * based on the new window dimensions
         */
        MacPaint_InvalidateWindowArea();
    }
}

/*
 * MOUSE EVENT HANDLING
 */

/**
 * MacPaint_HandleMouseDownEvent - Process mouse button press (event dispatcher)
 */
void MacPaint_HandleMouseDownEvent(int x, int y, int modifiers)
{
    Rect paintRect;
    int canvasX;
    int canvasY;

    gEventState.mouseDown = 1;
    gEventState.dragInProgress = 0;

    /* Update cursor for current position */
    MacPaint_UpdateCursorPosition(x, y);

    MacPaint_GetPaintRect(&paintRect);
    canvasX = x - paintRect.left;
    canvasY = y - paintRect.top;

    if (canvasX < 0) canvasX = 0;
    if (canvasY < 0) canvasY = 0;
    if (canvasX >= MACPAINT_DOC_WIDTH) canvasX = MACPAINT_DOC_WIDTH - 1;
    if (canvasY >= MACPAINT_DOC_HEIGHT) canvasY = MACPAINT_DOC_HEIGHT - 1;

    gEventState.lastMouseX = canvasX;
    gEventState.lastMouseY = canvasY;

    /* Save undo state before drawing */
    MacPaint_SaveUndoState("Drawing");

    /* Route to tool handler */
    MacPaint_HandleToolMouseEvent(gCurrentTool, canvasX, canvasY, 1);

    /* Invalidate only paint area for efficiency */
    MacPaint_InvalidatePaintArea();
}

/**
 * MacPaint_HandleMouseDragEvent - Process mouse movement during drag (event dispatcher)
 */
void MacPaint_HandleMouseDragEvent(int x, int y)
{
    Rect paintRect;
    int canvasX;
    int canvasY;

    if (!gEventState.mouseDown) {
        return;
    }

    /* Update cursor position for any position changes */
    MacPaint_UpdateCursorPosition(x, y);

    MacPaint_GetPaintRect(&paintRect);
    canvasX = x - paintRect.left;
    canvasY = y - paintRect.top;

    if (canvasX < 0) canvasX = 0;
    if (canvasY < 0) canvasY = 0;
    if (canvasX >= MACPAINT_DOC_WIDTH) canvasX = MACPAINT_DOC_WIDTH - 1;
    if (canvasY >= MACPAINT_DOC_HEIGHT) canvasY = MACPAINT_DOC_HEIGHT - 1;

    /* Detect if movement is significant (drag threshold) */
    int dx = (canvasX > gEventState.lastMouseX) ?
        (canvasX - gEventState.lastMouseX) : (gEventState.lastMouseX - canvasX);
    int dy = (canvasY > gEventState.lastMouseY) ?
        (canvasY - gEventState.lastMouseY) : (gEventState.lastMouseY - canvasY);

    if (!gEventState.dragInProgress && (dx > 2 || dy > 2)) {
        gEventState.dragInProgress = 1;
    }

    if (gEventState.dragInProgress) {
        gEventState.lastMouseX = canvasX;
        gEventState.lastMouseY = canvasY;

        /* Route to tool handler for continuous drawing */
        MacPaint_HandleToolMouseEvent(gCurrentTool, canvasX, canvasY, 1);

        MacPaint_InvalidatePaintArea();
    }
}

/**
 * MacPaint_HandleMouseUpEvent - Process mouse button release (event dispatcher)
 */
void MacPaint_HandleMouseUpEvent(int x, int y)
{
    Rect paintRect;
    int canvasX;
    int canvasY;

    if (!gEventState.mouseDown) {
        return;
    }

    gEventState.mouseDown = 0;
    gEventState.dragInProgress = 0;

    /* Update cursor for current position */
    MacPaint_UpdateCursorPosition(x, y);

    MacPaint_GetPaintRect(&paintRect);
    canvasX = x - paintRect.left;
    canvasY = y - paintRect.top;

    if (canvasX < 0) canvasX = 0;
    if (canvasY < 0) canvasY = 0;
    if (canvasX >= MACPAINT_DOC_WIDTH) canvasX = MACPAINT_DOC_WIDTH - 1;
    if (canvasY >= MACPAINT_DOC_HEIGHT) canvasY = MACPAINT_DOC_HEIGHT - 1;

    /* Route to tool handler to finalize drawing */
    MacPaint_HandleToolMouseEvent(gCurrentTool, canvasX, canvasY, 0);

    /* Invalidate only paint area for efficiency */
    MacPaint_InvalidatePaintArea();
}

/*
 * KEYBOARD EVENT HANDLING
 */

/**
 * MacPaint_HandleKeyDownEvent - Process keyboard key press (event dispatcher)
 */
void MacPaint_HandleKeyDownEvent(int keyCode, int modifiers)
{
    /* Route to menu/tool keyboard handlers from MacPaint_Menus.c */
    MacPaint_HandleKeyDown(keyCode, modifiers);

    /* Keyboard events may affect tool selection or other UI state
     * Invalidate appropriate regions based on what changed
     */
    MacPaint_InvalidatePaintArea();
}

/*
 * MENU EVENT HANDLING
 */

/**
 * MacPaint_HandleMenuClickEvent - Process menu selection (event dispatcher)
 */
void MacPaint_HandleMenuClickEvent(int menuID, int itemID)
{
    if (menuID == 0) {
        return;  /* Menu was closed without selection */
    }

    /* Execute the menu command */
    MacPaint_ExecuteMenuCommand(menuID, itemID);

    /* Menu commands may affect entire UI
     * Invalidate all regions to ensure consistency
     */
    MacPaint_InvalidateWindowArea();
}

/*
 * EVENT ROUTING
 */

/**
 * COORDINATE MAPPING AND HIT TESTING
 */

/**
 * MacPaint_GetWindowPort - Helper to safely get window port
 */
static GrafPtr MacPaint_GetWindowPort(WindowPtr window)
{
    if (!window) return NULL;
    return GetWindowPort(window);
}

/**
 * MacPaint_IsPointInToolbox - Check if point is in toolbox region
 * Returns 1 if in toolbox, 0 otherwise
 */
static int MacPaint_IsPointInToolbox(int localX, int localY)
{
    if (!gEventState.paintWindow) {
        return 0;
    }

    GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
    if (!port) return 0;

    int toolboxLeft = port->portRect.left;
    int toolboxTop = port->portRect.top;
    int toolboxRight = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
    int toolboxBottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT; /* Exclude status bar */

    return (localX >= toolboxLeft && localX <= toolboxRight &&
            localY >= toolboxTop && localY <= toolboxBottom);
}

/**
 * MacPaint_IsPointInCanvas - Check if point is in canvas (paint) region
 * Returns 1 if in canvas, 0 otherwise
 */
static int MacPaint_IsPointInCanvas(int localX, int localY)
{
    if (!gEventState.paintWindow) {
        return 0;
    }

    GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
    if (!port) return 0;

    int canvasLeft = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
    int canvasTop = port->portRect.top;
    int canvasRight = port->portRect.right;   /* Exclude toolbox */
    int canvasBottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT; /* Exclude status bar */

    return (localX >= canvasLeft && localX <= canvasRight &&
            localY >= canvasTop && localY <= canvasBottom);
}

/**
 * MacPaint_GetToolboxToolID - Determine which tool was clicked in toolbox
 * Returns tool ID (0-11) or -1 if click was not on a tool button
 */
static int MacPaint_GetToolboxToolID(int localX, int localY)
{
    if (!gEventState.paintWindow) {
        return -1;
    }

    GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
    if (!port) return -1;

    int toolboxLeft = port->portRect.left;
    int toolboxTop = port->portRect.top;

    /* Tool layout: 2 columns x 6 rows, 30x30 pixels each with 2 pixel spacing */
    int toolSize = 30;
    int spacing = 2;

    /* Calculate which row and column */
    int relativeX = localX - toolboxLeft - spacing;
    int relativeY = localY - toolboxTop - spacing;

    /* Check if click is in the tool grid area */
    if (relativeX < 0 || relativeY < 0) {
        return -1;
    }

    int col = relativeX / (toolSize + spacing);
    int row = relativeY / (toolSize + spacing);

    /* Verify we're actually on a tool button, not in the spacing */
    if ((relativeX % (toolSize + spacing)) > toolSize ||
        (relativeY % (toolSize + spacing)) > toolSize) {
        return -1;
    }

    int toolID = row * 2 + col;

    /* Validate tool ID (0-11 are valid) */
    if (toolID >= 0 && toolID < 12) {
        return toolID;
    }

    return -1;
}

/**
 * MacPaint_HandleToolboxClick - Process click in toolbox area
 */
static void MacPaint_HandleToolboxClick(int localX, int localY, int modifiers)
{
    int toolID = MacPaint_GetToolboxToolID(localX, localY);

    if (toolID >= 0 && toolID < 12) {
        /* Valid tool click - select the tool */
        MacPaint_SetActiveTool(toolID);
    }
}

/**
 * MacPaint_HandleCanvasClick - Process click in canvas area
 * Route to current tool for drawing
 */
static void MacPaint_HandleCanvasClick(int localX, int localY, int modifiers)
{
    Rect paintRect;
    int canvasX;
    int canvasY;

    MacPaint_GetPaintRect(&paintRect);
    canvasX = localX - paintRect.left;
    canvasY = localY - paintRect.top;

    if (canvasX < 0) canvasX = 0;
    if (canvasY < 0) canvasY = 0;
    if (canvasX >= MACPAINT_DOC_WIDTH) canvasX = MACPAINT_DOC_WIDTH - 1;
    if (canvasY >= MACPAINT_DOC_HEIGHT) canvasY = MACPAINT_DOC_HEIGHT - 1;

    /* Save undo state before drawing */
    MacPaint_SaveUndoState("Drawing");

    /* Update mouse state */
    gEventState.mouseDown = 1;
    gEventState.lastMouseX = canvasX;
    gEventState.lastMouseY = canvasY;
    gEventState.dragInProgress = 0;

    /* Route to tool handler */
    MacPaint_HandleToolMouseEvent(gCurrentTool, canvasX, canvasY, 1);

    /* Invalidate only paint area for efficiency */
    MacPaint_InvalidatePaintArea();
}

/**
 * MacPaint_RouteEventToWindow - Determine if event is for paint window
 */
int MacPaint_IsEventInPaintWindow(int x, int y)
{
    if (!gEventState.paintWindow) {
        return 0;
    }

    GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
    if (!port) return 0;

    return (x >= port->portRect.left && x <= port->portRect.right &&
            y >= port->portRect.top && y <= port->portRect.bottom);
}

/*
 * MAIN EVENT LOOP
 */

/**
 * MacPaint_RunEventLoop - Main application event loop
 * Processes events until application should quit using WaitNextEvent
 */
void MacPaint_RunEventLoop(void)
{
    EventRecord event;
    WindowPtr eventWindow;
    int localX, localY;
    MenuResult menuResult;
    int menuID, itemID;
    int keyCode;

    gEventState.running = 1;

    while (gEventState.running) {
        /* Wait for next event (max 30 ticks / ~500ms per iteration for responsiveness) */
        event.what = nullEvent;

        if (!WaitNextEvent(everyEvent, &event, 30, NULL)) {
            /* No event received - handle idle time processing */
            MacPaint_ProcessIdleTime();
            continue;
        }

        /* Process different event types */
        switch (event.what) {

            case kEventMouseDown:
                /* Determine which window/region the click is in */
                eventWindow = FrontWindow();

                /* Check for menu bar click */
                if (event.where.v < 20) {
                    /* Click is in menu bar region */
                    menuResult = MenuSelect(event.where);
                    menuID = HiWord(menuResult);
                    itemID = LoWord(menuResult);
                    if (menuID != 0) {
                        MacPaint_HandleMenuClickEvent(menuID, itemID);
                    }
                } else if (eventWindow == gEventState.paintWindow) {
                    /* Convert global to local window coordinates */
                    GrafPtr port = MacPaint_GetWindowPort(eventWindow);
                    if (port) {
                        SetPort(port);
                        localX = event.where.h - port->portRect.left;
                        localY = event.where.v - port->portRect.top;

                        /* Route click based on region */
                        if (MacPaint_IsPointInToolbox(localX, localY)) {
                            /* Click in toolbox - select tool */
                            MacPaint_HandleToolboxClick(localX, localY, event.modifiers);
                        } else if (MacPaint_IsPointInCanvas(localX, localY)) {
                            /* Click in canvas - start drawing with current tool */
                            MacPaint_HandleCanvasClick(localX, localY, event.modifiers);
                        }
                    }
                }
                break;

            case kEventMouseUp:
                /* Mouse button released */
                if (gEventState.mouseDown) {
                    /* Only process if we were in a drawing state */
                    eventWindow = FrontWindow();
                    if (eventWindow == gEventState.paintWindow) {
                        GrafPtr port = MacPaint_GetWindowPort(eventWindow);
                        if (port) {
                            SetPort(port);
                            localX = event.where.h - port->portRect.left;
                            localY = event.where.v - port->portRect.top;

                            /* Release mouse button and finalize current drawing */
                            gEventState.mouseDown = 0;
                            gEventState.dragInProgress = 0;

                            /* Update cursor for current position */
                            MacPaint_UpdateCursorPosition(localX, localY);

                            Rect paintRect;
                            int canvasX;
                            int canvasY;

                            MacPaint_GetPaintRect(&paintRect);
                            canvasX = localX - paintRect.left;
                            canvasY = localY - paintRect.top;

                            if (canvasX < 0) canvasX = 0;
                            if (canvasY < 0) canvasY = 0;
                            if (canvasX >= MACPAINT_DOC_WIDTH) canvasX = MACPAINT_DOC_WIDTH - 1;
                            if (canvasY >= MACPAINT_DOC_HEIGHT) canvasY = MACPAINT_DOC_HEIGHT - 1;

                            /* Route to tool handler to finalize drawing */
                            MacPaint_HandleToolMouseEvent(gCurrentTool, canvasX, canvasY, 0);

                            /* Invalidate only paint area for efficiency */
                            MacPaint_InvalidatePaintArea();
                        }
                    }
                }
                break;

            case kEventKeyDown:
            case kEventAutoKey:
                /* Extract key code from event message
                 * Format: bits 8-15 contain the key code
                 */
                keyCode = (event.message >> 8) & 0xFF;
                MacPaint_HandleKeyDownEvent(keyCode, event.modifiers);
                break;

            case kEventUpdateEvt:
                /* Window needs redraw */
                eventWindow = (WindowPtr)(uintptr_t)event.message;
                MacPaint_HandleWindowUpdate(eventWindow);
                break;

            case kEventOSEvt:
                /* Operating system event (suspend/resume) */
                /* Bit 24 indicates event type */
                if ((event.message >> 24) == kSuspendResumeMessage) {
                    if (event.message & kResumeFlag) {
                        /* Application is being resumed */
                        MacPaint_InvalidateWindowArea();
                    } else {
                        /* Application is being suspended */
                        /* Could save state or pause animations here */
                    }
                }
                break;

            case kEventCloseEvt:
                /* Window close event */
                eventWindow = (WindowPtr)(uintptr_t)event.message;
                if (eventWindow == gEventState.paintWindow) {
                    MacPaint_HandleWindowClose(eventWindow);
                }
                break;

            /* Note: kEventQuitEvent not valid in classic Mac OS event model
            case kEventQuitEvent:
                // Quit event from system
                gEventState.running = 0;
                break;
            */

            case kEventNullEvent:
                /* No event, but we got control back from WaitNextEvent */
                MacPaint_ProcessIdleTime();
                break;

            default:
                /* Unknown event type - just process idle time */
                MacPaint_ProcessIdleTime();
                break;
        }

        /* Update menu items to reflect current application state */
        MacPaint_AdjustMenus();
    }
}

/*
 * WINDOW REDRAW AND INVALIDATION
 */

/**
 * MacPaint_InvalidateRectArea - Mark specific rectangle for redraw
 */
void MacPaint_InvalidateRectArea(const Rect *rect)
{
    if (!gPaintWindow || !rect) {
        return;
    }

    /* Delegate to rendering module's invalidation system */
    InvalRect(rect);
}

/*
 * EVENT LOOP STATE QUERIES
 */

/**
 * MacPaint_IsMouseDown - Check if mouse button is currently pressed
 */
int MacPaint_IsMouseDown(void)
{
    return gEventState.mouseDown;
}

/**
 * MacPaint_GetLastMousePosition - Get last recorded mouse position
 */
void MacPaint_GetLastMousePosition(int *x, int *y)
{
    if (x) *x = gEventState.lastMouseX;
    if (y) *y = gEventState.lastMouseY;
}

/**
 * MacPaint_ShouldQuit - Check if application should exit
 */
int MacPaint_ShouldQuit(void)
{
    return !gEventState.running;
}

/**
 * MacPaint_RequestQuit - Signal application to quit
 */
void MacPaint_RequestQuit(void)
{
    gEventState.running = 0;
}

/*
 * SAVE PROMPTS
 */

/**
 * MacPaint_PromptSaveChanges - Ask user if they want to save
 * Returns: 0 = don't save, 1 = save, 2 = cancel
 */
int MacPaint_PromptSaveChanges(void)
{
    DialogPtr dialog;
    SInt16 itemHit = 0;
    Rect bounds;
    const unsigned char *title = (const unsigned char *)"\p" "Save Document?";
    const unsigned char *message = (const unsigned char *)"\p" "Save changes to \042Untitled\042 before closing?";

    if (!MacPaint_IsDocumentDirty()) {
        return 0;  /* No changes, don't need to save */
    }

    /* Create a simple dialog with three buttons:
     * 1 = Save
     * 2 = Don't Save (or Cancel in classic Alert)
     * 3 = Cancel
     */

    /* Define dialog bounds (centered, reasonable size) */
    bounds.top = 100;
    bounds.left = 75;
    bounds.bottom = 250;
    bounds.right = 425;

    /* Create modal dialog
     * Using standard dialog ID for alert-style dialog
     * DialogID 128 is typically used for standard alerts
     */
    dialog = GetNewDialog(128, NULL, (WindowPtr)-1);
    if (!dialog) {
        return 1;  /* Fallback: assume save */
    }

    /* Set up dialog title and message text
     * In a real implementation, we'd set static text items
     * For now, use the dialog as-is with default buttons
     */

    /* Show the dialog and wait for user response */
    do {
        ModalDialog(NULL, &itemHit);
    } while (itemHit == 0);

    /* Dispose the dialog */
    DisposeDialog(dialog);

    /* Parse button response:
     * Button 1 = Save
     * Button 2 = Don't Save
     * Button 3 = Cancel
     */
    switch (itemHit) {
        case 1: return 1;  /* Save */
        case 2: return 0;  /* Don't save */
        case 3: return 2;  /* Cancel */
        default: return 1; /* Default to save if uncertain */
    }
}

/*
 * TOOL AND DRAWING STATE MANAGEMENT
 */

/**
 * MacPaint_SetActiveTool - Change current tool
 */
void MacPaint_SetActiveTool(int toolID)
{
    MacPaint_SelectTool(toolID);
    MacPaint_InvalidateToolArea();
}

/**
 * MacPaint_GetActiveTool - Get current tool
 */
int MacPaint_GetActiveTool(void)
{
    return gCurrentTool;
}

/*
 * IDLE TIME PROCESSING
 */

/**
 * MacPaint_ProcessIdleTime - Do background work when no events pending
 * Called periodically during event loop idle time
 */
void MacPaint_ProcessIdleTime(void)
{
    /* Update animated elements (marching ants, blinking cursors, etc) */
    MacPaint_UpdateAnimations();

    /* Handle continuous mouse tracking for drawing operations */
    if (gEventState.mouseDown) {
        /* Check if mouse button is still down */
        if (StillDown()) {
            /* Get current mouse position */
            Point mouseLoc;
            GetMouse(&mouseLoc);

            /* Convert to local window coordinates */
            GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
            if (port) {
                int localX = mouseLoc.h - port->portRect.left;
                int localY = mouseLoc.v - port->portRect.top;

                /* Process continuous drawing via drag handler */
                MacPaint_HandleMouseDragEvent(localX, localY);
            }
        } else {
            /* Mouse button was released - finalize drawing */
            gEventState.mouseDown = 0;
            gEventState.dragInProgress = 0;
            MacPaint_InvalidatePaintArea();
        }
    } else {
        /* Mouse not down - update cursor position dynamically
         * This allows cursor to change based on hover position
         */
        Point mouseLoc;
        GetMouse(&mouseLoc);

        GrafPtr port = MacPaint_GetWindowPort(gEventState.paintWindow);
        if (port) {
            int localX = mouseLoc.h - port->portRect.left;
            int localY = mouseLoc.v - port->portRect.top;
            MacPaint_UpdateCursorPosition(localX, localY);
        }
    }

    /* TODO: Additional background tasks
     * - Check for file system changes
     * - Perform memory cleanup
     * - Update status information
     */
}

/*
 * CLIPBOARD AND DRAG/DROP INTEGRATION
 */

/**
 * MacPaint_PasteFromSystemClipboard - Get data from Mac clipboard
 */
int MacPaint_PasteFromSystemClipboard(void)
{
    /* TODO: Use Scrap Manager
     * Handle h = NewHandle(0);
     * GetScrap(h, 'PICT', &offset);
     * if (h):
     *     MacPaint_PasteFromClipboard(0, 0)
     *     DisposeHandle(h)
     *     return 1
     * return 0
     */

    return 0;  /* Placeholder */
}

/**
 * MacPaint_CopyToSystemClipboard - Put data on Mac clipboard
 */
int MacPaint_CopyToSystemClipboard(void)
{
    /* TODO: Use Scrap Manager
     * if (MacPaint_IsSelectionActive()):
     *     ZeroScrap()
     *     // Copy selection bitmap to PICT on scrap
     *     return 1
     * return 0
     */

    return 0;  /* Placeholder */
}

/*
 * ERROR RECOVERY
 */

/**
 * MacPaint_HandleError - Display error alert and continue
 */
void MacPaint_HandleError(OSErr err, const char *context)
{
    if (err == noErr) {
        return;
    }

    /* TODO: Show error alert
     * char errorMsg[256];
     * snprintf(errorMsg, sizeof(errorMsg),
     *          "Error %d in %s", err, context);
     *
     * AlertStdAlertParamRec params = {};
     * DialogItemIndex itemHit;
     * StandardAlert(kAlertStopAlert,
     *              CFSTR(errorMsg),
     *              NULL, &params, &itemHit);
     */
}

/*
 * SHUTDOWN COORDINATION
 */

/**
 * MacPaint_PrepareForShutdown - Final preparations before exit
 */
void MacPaint_PrepareForShutdown(void)
{
    /* Check if unsaved changes */
    if (MacPaint_IsDocumentDirty()) {
        int savePrompt = MacPaint_PromptSaveChanges();
        if (savePrompt == 1) {
            MacPaint_FileSave();
        }
        /* If savePrompt == 2 (cancel), user would cancel quit,
           but for now we proceed with shutdown */
    }

    /* Close any open dialogs */
    if (MacPaint_IsPatternEditorOpen()) {
        MacPaint_ClosePatternEditorDialog();
    }
    if (MacPaint_IsBrushEditorOpen()) {
        MacPaint_CloseBrushEditorDialog();
    }

    /* Close main window */
    if (gEventState.paintWindow) {
        MacPaint_CloseWindow();
    }
}
