/**
 * @file EventDispatcher.c
 * @brief Event Dispatcher - Routes events from EventManager to appropriate handlers
 *
 * This file provides the event dispatching mechanism that connects the Event Manager
 * to various system components like Window Manager, Menu Manager, Dialog Manager, etc.
 * It implements the standard Mac OS event dispatching model.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"  /* Include EventTypes.h first to define activeFlag */
#include "EventManager/EventManager.h"
#include "WindowManager/WindowManager.h"
#include "MenuManager/MenuManager.h"
#include <stdlib.h>  /* For abs() */

/* External functions */
extern void serial_printf(const char* fmt, ...);
extern short FindWindow(Point thePoint, WindowPtr* theWindow);
extern WindowPtr FrontWindow(void);
extern void SelectWindow(WindowPtr theWindow);
extern void DragWindow(WindowPtr window, Point startPt, const struct Rect* boundsRect);
extern Boolean TrackGoAway(WindowPtr window, Point thePt);
extern void CloseWindow(WindowPtr window);
extern void BeginUpdate(WindowPtr window);
extern void EndUpdate(WindowPtr window);
extern void DrawGrowIcon(WindowPtr window);
extern void DoMenuCommand(short menuID, short item);
extern long MenuSelect(Point startPt);
extern void HiliteMenu(short menuID);

/* Forward declarations of event handler functions */
Boolean HandleNullEvent(EventRecord* event);
Boolean HandleMouseDown(EventRecord* event);
Boolean HandleMouseUp(EventRecord* event);
Boolean HandleKeyDownEvent(EventRecord* event);
Boolean HandleKeyUp(EventRecord* event);
Boolean HandleUpdate(EventRecord* event);
Boolean HandleActivate(EventRecord* event);
Boolean HandleDisk(EventRecord* event);
Boolean HandleOSEvent(EventRecord* event);

/* Global event dispatcher state */
static struct {
    Boolean initialized;
    WindowPtr activeWindow;
    UInt32 lastActivateTime;
    Boolean menuVisible;
    Boolean trackingDesktop;  /* Tracking mouse on desktop */
} g_dispatcher = {
    false,
    NULL,
    0,
    false,
    false
};

/**
 * Initialize the event dispatcher
 */
void InitEventDispatcher(void)
{
    g_dispatcher.initialized = true;
    g_dispatcher.activeWindow = NULL;
    g_dispatcher.lastActivateTime = 0;
    g_dispatcher.menuVisible = false;
}

/**
 * Main event dispatcher - routes events to appropriate handlers
 * @param event The event to dispatch
 * @return true if event was handled
 */
Boolean DispatchEvent(EventRecord* event)
{
    if (!event || !g_dispatcher.initialized) {
        return false;
    }

    switch (event->what) {
        case nullEvent:
            return HandleNullEvent(event);

        case mouseDown:
            return HandleMouseDown(event);

        case mouseUp:
            return HandleMouseUp(event);

        case keyDown:
        case autoKey:
            return HandleKeyDownEvent(event);

        case keyUp:
            return HandleKeyUp(event);

        case updateEvt:
            return HandleUpdate(event);

        case activateEvt:
            return HandleActivate(event);

        case diskEvt:
            return HandleDisk(event);

        case osEvt:
            return HandleOSEvent(event);

        default:
            return false;
    }
}

/**
 * Handle null events (idle processing)
 */
Boolean HandleNullEvent(EventRecord* event)
{
    /* Null events are used for idle processing */
    /* Could be used for cursor animation, background tasks, etc. */

    /* Check if we're tracking desktop drag */
    if (g_dispatcher.trackingDesktop) {
        /* Check if mouse button is still down */
        extern Boolean Button(void);
        Boolean buttonDown = Button();

        /* Handle drag tracking */
        extern Boolean HandleDesktopDrag(Point mousePt, Boolean buttonDown);
        HandleDesktopDrag(event->where, buttonDown);

        /* Stop tracking if button released */
        if (!buttonDown) {
            g_dispatcher.trackingDesktop = false;
        }
    }

    return true;
}

/**
 * Handle mouse down events
 */
Boolean HandleMouseDown(EventRecord* event)
{
    WindowPtr whichWindow;
    short windowPart;
    Rect dragBounds;

    /* Find which window part was clicked */
    windowPart = FindWindow(event->where, &whichWindow);

    /* Debug with explicit cast to ensure correct values */
    serial_printf("HandleMouseDown: event=%p, where={v=%d,h=%d}, modifiers=0x%04x\n",
                 event, (int)event->where.v, (int)event->where.h, event->modifiers);
    serial_printf("HandleMouseDown: part=%d, window=%p at (%d,%d)\n",
                 windowPart, whichWindow, (int)event->where.h, (int)event->where.v);

    switch (windowPart) {
        case inMenuBar:
            /* Handle menu selection */
            {
                long menuChoice = MenuSelect(event->where);
                if (menuChoice) {
                    short menuID = (menuChoice >> 16) & 0xFFFF;
                    short menuItem = menuChoice & 0xFFFF;
                    DoMenuCommand(menuID, menuItem);
                    HiliteMenu(0);  /* Unhighlight menu */
                }
            }
            return true;

        case inSysWindow:
            /* System window - let system handle it */
            return false;

        case inContent:
            /* Click in window content */
            if (whichWindow != FrontWindow()) {
                /* Bring window to front first */
                SelectWindow(whichWindow);
            } else {
                /* Pass click to window content handler */
                /* Application would handle this */
                serial_printf("Click in content of window %p\n", whichWindow);
            }
            return true;

        case inDrag:
            /* Drag window */
            if (whichWindow) {
                /* Set up drag bounds (entire screen minus menu bar) */
                dragBounds.top = 20;     /* Below menu bar */
                dragBounds.left = 0;
                dragBounds.bottom = 768;  /* Screen height */
                dragBounds.right = 1024;  /* Screen width */

                DragWindow(whichWindow, event->where, &dragBounds);
            }
            return true;

        case inGrow:
            /* Resize window */
            if (whichWindow) {
                /* TODO: Implement window resizing */
                serial_printf("Grow window %p\n", whichWindow);
            }
            return true;

        case inGoAway:
            /* Close box clicked */
            if (whichWindow) {
                if (TrackGoAway(whichWindow, event->where)) {
                    CloseWindow(whichWindow);
                }
            }
            return true;

        case inZoomIn:
        case inZoomOut:
            /* Zoom box clicked */
            if (whichWindow) {
                /* TODO: Implement window zooming */
                serial_printf("Zoom window %p\n", whichWindow);
            }
            return true;

        case inDesk:
            /* Click on desktop - check if it's on an icon */
            {
                /* Extract click count from high word of message */
                UInt16 clickCount = (event->message >> 16) & 0xFFFF;
                Boolean doubleClick = (clickCount >= 2);

                /* Check if click was on a desktop icon */
                extern Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick);
                if (HandleDesktopClick(event->where, doubleClick)) {
                    serial_printf("Desktop icon clicked (count=%d)\n", clickCount);
                    /* Start tracking for potential drag */
                    g_dispatcher.trackingDesktop = true;
                    return true;
                }

                /* Otherwise just a desktop click */
                serial_printf("Click on desktop (no icon)\n");
            }
            return true;

        default:
            return false;
    }
}

/**
 * Handle mouse up events
 */
Boolean HandleMouseUp(EventRecord* event)
{
    /* Mouse up events are often handled implicitly by tracking functions */
    /* But we can use them for drag completion, etc. */

    /* End desktop tracking if active */
    if (g_dispatcher.trackingDesktop) {
        extern Boolean HandleDesktopDrag(Point mousePt, Boolean buttonDown);
        HandleDesktopDrag(event->where, false);  /* Button up */
        g_dispatcher.trackingDesktop = false;
    }

    return true;
}

/**
 * Handle keyboard events
 */
Boolean HandleKeyDownEvent(EventRecord* event)
{
    char key = event->message & charCodeMask;
    Boolean cmdKeyDown = (event->modifiers & cmdKey) != 0;

    serial_printf("HandleKeyDownEvent: key='%c' (0x%02x), cmd=%d\n",
                 (key >= 32 && key < 127) ? key : '?', key, cmdKeyDown);

    if (cmdKeyDown) {
        /* Command key combinations often map to menu items */
        /* TODO: Implement command key menu shortcuts */

        /* Common shortcuts */
        switch (key) {
            case 'q':
            case 'Q':
                /* Quit */
                serial_printf("Quit requested\n");
                return true;

            case 'w':
            case 'W':
                /* Close window */
                {
                    WindowPtr frontWindow = FrontWindow();
                    if (frontWindow) {
                        CloseWindow(frontWindow);
                    }
                }
                return true;

            case 'n':
            case 'N':
                /* New */
                serial_printf("New requested\n");
                return true;

            case 'o':
            case 'O':
                /* Open */
                serial_printf("Open requested\n");
                return true;

            default:
                break;
        }
    }

    /* Pass to active window or application */
    WindowPtr frontWindow = FrontWindow();
    if (frontWindow) {
        /* Application would handle this */
        serial_printf("Key '%c' to window %p\n", key, frontWindow);
    }

    return true;
}

/**
 * Handle key up events
 */
Boolean HandleKeyUp(EventRecord* event)
{
    /* Key up events are usually ignored unless tracking key state */
    return true;
}

/**
 * Handle update events
 */
Boolean HandleUpdate(EventRecord* event)
{
    WindowPtr updateWindow = (WindowPtr)(event->message);

    serial_printf("HandleUpdate: window=%p\n", updateWindow);

    if (updateWindow) {
        /* Begin update to set up clipping */
        BeginUpdate(updateWindow);

        /* Draw window contents */
        /* Application would do the actual drawing */
        SetPort((GrafPtr)updateWindow);

        /* Draw grow icon if window has grow box */
        if (updateWindow->windowKind >= 0) {
            DrawGrowIcon(updateWindow);
        }

        /* End update to restore clipping */
        EndUpdate(updateWindow);
    }

    return true;
}

/**
 * Handle activate/deactivate events
 */
Boolean HandleActivate(EventRecord* event)
{
    WindowPtr window = (WindowPtr)(event->message);
    Boolean activating = (event->modifiers & activeFlag) != 0;

    serial_printf("HandleActivate: window=%p, activating=%d\n",
                 window, activating);

    if (window) {
        if (activating) {
            /* Window is being activated */
            g_dispatcher.activeWindow = window;

            /* Highlight window controls, enable menus, etc. */
            /* Application would handle this */
        } else {
            /* Window is being deactivated */
            if (g_dispatcher.activeWindow == window) {
                g_dispatcher.activeWindow = NULL;
            }

            /* Unhighlight window controls, etc. */
            /* Application would handle this */
        }

        g_dispatcher.lastActivateTime = event->when;
    }

    return true;
}

/**
 * Handle disk events
 */
Boolean HandleDisk(EventRecord* event)
{
    /* Disk insertion/ejection events */
    /* Would be handled by File Manager */
    serial_printf("HandleDisk: message=0x%08lx\n", event->message);
    return true;
}

/**
 * Handle operating system events
 */
Boolean HandleOSEvent(EventRecord* event)
{
    /* Suspend/resume, mouse moved out of region, etc. */
    short osMessage = (event->message >> 24) & 0xFF;

    switch (osMessage) {
        case 1:  /* Suspend event */
            serial_printf("Application suspended\n");
            break;

        case 2:  /* Resume event */
            serial_printf("Application resumed\n");
            break;

        case 0xFA:  /* Mouse moved out of region */
            serial_printf("Mouse moved out of region\n");
            break;

        default:
            serial_printf("OS Event: 0x%02x\n", osMessage);
            break;
    }

    return true;
}

/**
 * Get the currently active window
 */
WindowPtr GetActiveWindow(void)
{
    return g_dispatcher.activeWindow;
}

/**
 * Set the active window
 */
void SetActiveWindow(WindowPtr window)
{
    if (g_dispatcher.activeWindow != window) {
        /* Deactivate old window */
        if (g_dispatcher.activeWindow) {
            EventRecord deactivateEvent;
            deactivateEvent.what = activateEvt;
            deactivateEvent.message = (SInt32)g_dispatcher.activeWindow;
            deactivateEvent.when = TickCount();
            deactivateEvent.where.h = 0;
            deactivateEvent.where.v = 0;
            deactivateEvent.modifiers = 0;  /* No activeFlag means deactivate */

            HandleActivate(&deactivateEvent);
        }

        /* Activate new window */
        if (window) {
            EventRecord activateEvent;
            activateEvent.what = activateEvt;
            activateEvent.message = (SInt32)window;
            activateEvent.when = TickCount();
            activateEvent.where.h = 0;
            activateEvent.where.v = 0;
            activateEvent.modifiers = activeFlag;  /* activeFlag means activate */

            HandleActivate(&activateEvent);
        }

        g_dispatcher.activeWindow = window;
    }
}