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

/* Menu tracking functions from MenuTrack.c */
extern Boolean IsMenuTrackingNew(void);
extern void UpdateMenuTrackingNew(Point mousePt);
extern long EndMenuTrackingNew(void);
extern void GetMouse(Point* mouseLoc);

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
    /* Zero entire structure to prevent partial-init regressions */
    memset(&g_dispatcher, 0, sizeof(g_dispatcher));

    /* Set non-zero initial values */
    g_dispatcher.initialized = true;
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

    /* Entry log for all events */
    serial_printf("DispatchEvent: type=%d at (%d,%d)\n",
                  event->what, event->where.h, event->where.v);

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

    /* If we're tracking a menu, handle it specially */
    if (IsMenuTrackingNew()) {
        /* Mouse down while tracking = potential selection */
        UpdateMenuTrackingNew(event->where);
        /* Don't end tracking yet - wait for mouse up */
        return true;
    }

    /* Find which window part was clicked */
    windowPart = FindWindow(event->where, &whichWindow);

    serial_printf("HandleMouseDown: event=0x%08x, where={v=%d,h=%d}, modifiers=0x%04x\n",
                 (unsigned int)event, (int)event->where.v, (int)event->where.h, event->modifiers);
    serial_printf("HandleMouseDown: part=%d, window=0x%08x at (%d,%d)\n",
                 windowPart, (unsigned int)whichWindow, (int)event->where.h, (int)event->where.v);

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
            serial_printf("HandleMouseDown: inContent case - whichWindow=0x%08x\n", (unsigned int)whichWindow);

            WindowPtr frontWin = FrontWindow();
            serial_printf("HandleMouseDown: FrontWindow returned 0x%08x\n", (unsigned int)frontWin);

            if (whichWindow != frontWin) {
                /* Bring window to front first */
                serial_printf("HandleMouseDown: Calling SelectWindow(0x%08x)\n", (unsigned int)whichWindow);
                SelectWindow(whichWindow);
                serial_printf("HandleMouseDown: SelectWindow returned\n");
            } else {
                /* Pass click to window content handler */
                /* Application would handle this */
                serial_printf("Click in content of window 0x%08x\n", (unsigned int)whichWindow);
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

                serial_printf("HandleMouseDown: inDrag window=0x%08x bounds=(%d,%d,%d,%d)\n",
                             (unsigned int)whichWindow, dragBounds.top, dragBounds.left,
                             dragBounds.bottom, dragBounds.right);
                DragWindow(whichWindow, event->where, &dragBounds);
                serial_printf("HandleMouseDown: DragWindow returned\n");
            } else {
                serial_printf("HandleMouseDown: inDrag but whichWindow is NULL!\n");
            }
            return true;

        case inGrow:
            /* Resize window */
            if (whichWindow) {
                /* TODO: Implement window resizing */
                serial_printf("Grow window 0x%08x\n", (unsigned int)whichWindow);
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
                serial_printf("Zoom window 0x%08x\n", (unsigned int)whichWindow);
            }
            return true;

        case inDesk:
            /* Click on desktop - check if it's on an icon */
            {
                /* Classic System 7: click count in high word of message */
                UInt16 clickCount = (event->message >> 16) & 0xFFFF;
                Boolean doubleClick = (clickCount >= 2);

                /* Check if click was on a desktop icon */
                extern Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick);
                if (HandleDesktopClick(event->where, doubleClick)) {
                    serial_printf("Desktop icon clicked (clickCount=%d)\n", clickCount);
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
    /* Check if we're tracking a menu */
    if (IsMenuTrackingNew()) {
        /* Update position one more time */
        UpdateMenuTrackingNew(event->where);

        /* End menu tracking and get selection */
        long menuChoice = EndMenuTrackingNew();
        if (menuChoice) {
            short menuID = (menuChoice >> 16) & 0xFFFF;
            short menuItem = menuChoice & 0xFFFF;
            DoMenuCommand(menuID, menuItem);
            HiliteMenu(0);  /* Unhighlight menu */
        }
        return true;
    }

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

    /* Handle special keys without command modifier */
    if (!cmdKeyDown) {
        switch (key) {
            case 0x09:  /* Tab key */
                {
                    extern void SelectNextDesktopIcon(void);
                    SelectNextDesktopIcon();
                    serial_printf("Tab pressed - selecting next desktop icon\n");
                }
                return true;

            case 0x0D:  /* Enter/Return key */
                {
                    extern void OpenSelectedDesktopIcon(void);
                    OpenSelectedDesktopIcon();
                    serial_printf("Enter pressed - opening selected icon\n");
                }
                return true;
        }
    }

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
        /* Check if this is the TextEdit window and forward to TEKey */
        extern Boolean TextEdit_IsRunning(void);
        extern void TextEdit_HandleEvent(EventRecord* event);

        if (TextEdit_IsRunning()) {
            serial_printf("Key '%c' (0x%02x) → TextEdit window 0x%08x\n",
                         (key >= 32 && key < 127) ? key : '?', key, (unsigned int)frontWindow);
            TextEdit_HandleEvent(event);
            return true;
        }

        /* Other applications would handle their own keys */
        serial_printf("Key '%c' to window 0x%08x (no handler)\n", key, (unsigned int)frontWindow);
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

    serial_printf("HandleUpdate: window=0x%08x\n", (unsigned int)updateWindow);

    if (updateWindow) {
        /* Begin update to set up clipping */
        BeginUpdate(updateWindow);

        /* Draw window contents */
        SetPort((GrafPtr)updateWindow);

        /* Check if this is a Finder window and draw its content */
        if (updateWindow->refCon == 'TRSH' || updateWindow->refCon == 'DISK') {
            /* Call Finder's window drawing */
            extern void DrawFolderWindowContents(WindowPtr window, Boolean isTrash);
            DrawFolderWindowContents(updateWindow, updateWindow->refCon == 'TRSH');
        } else {
            /* Application would do the actual drawing */
            /* For now, just fill with white to show content area */
            Rect r = updateWindow->port.portRect;
            EraseRect(&r);
        }

        /* Draw grow icon if window has grow box */
        if (updateWindow->windowKind >= 0) {
            DrawGrowIcon(updateWindow);
        }

        /* End update to restore clipping */
        EndUpdate(updateWindow);

        /* Log successful update */
        serial_printf("UPDATE: drew content for window=%p\n", updateWindow);
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

    serial_printf("HandleActivate: window=0x%08x, activating=%d\n",
                 (unsigned int)window, activating);

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