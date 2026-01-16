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
#include "EventManager/EventManagerInternal.h"
#include "EventManager/EventTypes.h"  /* Include EventTypes.h first to define activeFlag */
#include "EventManager/EventManager.h"
#include "WindowManager/WindowManager.h"
#include "MenuManager/MenuManager.h"
#include "ControlPanels/DesktopPatterns.h"
#include "ControlPanels/Sound.h"
#include "ControlPanels/Mouse.h"
#include "ControlPanels/Keyboard.h"
#include "ControlPanels/ControlStrip.h"
#include "Datetime/datetime_cdev.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryMgr/MemoryManager.h"
#include "Finder/AboutThisMac.h"  /* About This Macintosh window */
#include "Apps/SimpleText.h"
#include <stdlib.h>  /* For abs() */
#include "EventManager/EventLogging.h"

/* External functions */
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
extern long MenuKey(short ch);

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

static Boolean gControlStripBootstrap = true;

/**
 * Initialize the event dispatcher
 */
void InitEventDispatcher(void)
{
    extern void serial_puts(const char* s);
    serial_puts("[INIT_DISP] InitEventDispatcher ENTRY\n");

    /* Zero entire structure to prevent partial-init regressions */
    memset(&g_dispatcher, 0, sizeof(g_dispatcher));

    /* Set non-zero initial values */
    g_dispatcher.initialized = true;

    serial_puts("[INIT_DISP] InitEventDispatcher EXIT\n");
}

/**
 * Main event dispatcher - routes events to appropriate handlers
 * @param event The event to dispatch
 * @return true if event was handled
 */
Boolean DispatchEvent(EventRecord* event)
{
    extern void serial_puts(const char* s);

    EVT_LOG_DEBUG("[DISP] >>> DispatchEvent ENTRY event=%p\n", event);

    if (!event || !g_dispatcher.initialized) {
        EVT_LOG_DEBUG("[DISP] Early return: event=%p, init=%d\n", event, g_dispatcher.initialized);
        return false;
    }

    /* Entry log for all events */
    EVT_LOG_DEBUG("[DISP] DispatchEvent: event->what=%d\n", event->what);

    if (ControlStrip_HandleEvent(event)) {
        return true;
    }

    if (SoundPanel_HandleEvent(event)) {
        return true;
    }

    if (MousePanel_HandleEvent(event)) {
        return true;
    }

    if (KeyboardPanel_HandleEvent(event)) {
        return true;
    }

    if (DesktopPatterns_HandleEvent(event)) {
        return true;
    }

    if (DateTimePanel_HandleEvent(event)) {
        return true;
    }

    /* Notepad event handling - selective interception */
    /* IMPORTANT: Allow menu bar clicks to bypass Notepad interception */
    extern void Notepad_HandleEvent(EventRecord *event);
    extern WindowPtr Notepad_GetWindow(void);
    extern short FindWindow(Point thePoint, WindowPtr* theWindow);

    if (Notepad_GetWindow() != NULL) {
        /* For mouseDown events, check if click is on menu bar */
        if (event->what == mouseDown) {
            /* Menu bar is at v < 20 (pixel coordinates) */
            if (event->where.v < 20) {
                /* Let menu bar click through - don't intercept */
            } else {
                /* Non-menu bar click - let Notepad handle it */
                Notepad_HandleEvent(event);
                return true;
            }
        } else if (event->what != mouseUp && event->what != nullEvent) {
            /* For non-mouse events (keyDown, keyUp, etc), let Notepad handle them */
            Notepad_HandleEvent(event);
            return true;
        }
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
            EVT_LOG_DEBUG("[DISP] Case updateEvt reached, calling HandleUpdate\n");
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

    if (gControlStripBootstrap) {
        ControlStrip_Show();
        gControlStripBootstrap = false;
    }

    DateTimePanel_Tick();

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

    if (SimpleText_IsRunning()) {
        SimpleText_Idle();
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

    if (SimpleText_DispatchEvent(event)) {
        return true;
    }

    /* Find which window part was clicked */
    windowPart = FindWindow(event->where, &whichWindow);

    EVT_LOG_DEBUG("HandleMouseDown: event=%p, where={v=%d,h=%d}, modifiers=0x%04x\n",
                 (void*)event, (int)event->where.v, (int)event->where.h, event->modifiers);
    EVT_LOG_DEBUG("HandleMouseDown: part=%d, window=%p at (%d,%d)\n",
                 windowPart, (void*)whichWindow, (int)event->where.h, (int)event->where.v);

    /* Check if this is the About This Macintosh window - handle specially */
    if (whichWindow && AboutWindow_IsOurs(whichWindow)) {
        Point localPt = event->where;
        /* AboutWindow_HandleMouseDown expects the part code and point */
        if (AboutWindow_HandleMouseDown(whichWindow, windowPart, localPt)) {
            return true;
        }
    }

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

        case inContent: {
            /* Click in window content */
            EVT_LOG_DEBUG("HandleMouseDown: inContent case - whichWindow=%p\n", (void*)whichWindow);

            WindowPtr frontWin = FrontWindow();
            EVT_LOG_DEBUG("HandleMouseDown: FrontWindow returned %p\n", (void*)frontWin);

            if (whichWindow != frontWin) {
                /* Bring window to front first */
                EVT_LOG_DEBUG("HandleMouseDown: Calling SelectWindow(%p)\n", (void*)whichWindow);
                SelectWindow(whichWindow);
                EVT_LOG_DEBUG("HandleMouseDown: SelectWindow returned\n");
                return true;
            }

            /* Window is already front - handle click in content */
            EVT_LOG_DEBUG("HandleMouseDown: Window already front, handling content click\n");

            /* Route to centralized content click handler */
            extern OSErr HandleContentClick(WindowPtr window, EventRecord* event);

            EVT_LOG_DEBUG("HandleMouseDown: Calling HandleContentClick for window=%p, refCon=0x%08x\n",
                         (void*)whichWindow, (unsigned int)whichWindow->refCon);

            OSErr err = HandleContentClick(whichWindow, event);

            EVT_LOG_DEBUG("HandleMouseDown: HandleContentClick returned %d\n", (int)err);
            return true;
        }

        case inDrag:
            /* Drag window */
            if (whichWindow) {
                /* CRITICAL: Select window first to bring it to front and activate it! */
                SelectWindow(whichWindow);
                EVT_LOG_DEBUG("HandleMouseDown: inDrag - called SelectWindow for window=%p\n",
                             (void*)whichWindow);

                /* Set up drag bounds (entire screen minus menu bar) */
                extern QDGlobals qd;
                dragBounds.top = 20;     /* Below menu bar */
                dragBounds.left = qd.screenBits.bounds.left;
                dragBounds.bottom = qd.screenBits.bounds.bottom;
                dragBounds.right = qd.screenBits.bounds.right;

                EVT_LOG_DEBUG("HandleMouseDown: inDrag window=%p bounds=(%d,%d,%d,%d)\n",
                             (void*)whichWindow, dragBounds.top, dragBounds.left,
                             dragBounds.bottom, dragBounds.right);

                extern void serial_puts(const char *str);
                serial_puts("[EVT] ABOUT TO CALL DragWindow\n");
                serial_puts("[EVT] DragWindow function ptr logged\n");

                serial_puts("[MEM] before DragWindow\n");
                MemoryManager_CheckSuspectBlock("before_DragWindow");
                DragWindow(whichWindow, event->where, &dragBounds);
                serial_puts("[MEM] after DragWindow\n");
                MemoryManager_CheckSuspectBlock("after_DragWindow");
                EVT_LOG_DEBUG("HandleMouseDown: DragWindow returned\n");
            } else {
                EVT_LOG_DEBUG("HandleMouseDown: inDrag but whichWindow is NULL!\n");
            }
            return true;

        case inGrow:
            /* Resize window using grow box */
            if (whichWindow) {
                EVT_LOG_DEBUG("Grow window 0x%08x\n", (unsigned int)whichWindow);
                /* Call GrowWindow directly instead of TrackBox to handle resize properly */
                extern long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox);
                long newSize = GrowWindow(whichWindow, event->where, NULL);
                EVT_LOG_DEBUG("GrowWindow returned size=0x%08lX\n", newSize);
            }
            return true;

        case inGoAway:
            /* Close box clicked - directly close without tracking (TrackGoAway not fully implemented) */
            if (whichWindow) {
                extern void DisposeWindow(WindowPtr window);
                EVT_LOG_DEBUG("DISP: Close box clicked, disposing window 0x%08x\n", (unsigned int)whichWindow);
                DisposeWindow(whichWindow);  /* Calls CloseWindow internally + frees memory */
            }
            return true;

        case inZoomIn:
        case inZoomOut:
            /* Zoom box clicked */
            if (whichWindow) {
                extern void ZoomWindow(WindowPtr theWindow, short partCode, Boolean front);
                EVT_LOG_DEBUG("Zoom window 0x%08x (partCode=%d)\n", (unsigned int)whichWindow, windowPart);
                ZoomWindow(whichWindow, windowPart, true);
            }
            return true;

        case inDesk:
            /* Click on desktop - check if it's on an icon */
            {
                /* Classic System 7: click count in high word of message */
                UInt16 clickCount = (event->message >> 16) & 0xFFFF;
                Boolean doubleClick = (clickCount >= 2);

                EVT_LOG_DEBUG("[DESK CLICK] clickCount=%d, doubleClick=%d, where=(%d,%d)\n",
                             clickCount, doubleClick, event->where.h, event->where.v);

                /* Check if click was on a desktop icon */
                extern Boolean HandleDesktopClick(Point clickPoint, Boolean doubleClick);
                if (HandleDesktopClick(event->where, doubleClick)) {
                    EVT_LOG_DEBUG("Desktop icon clicked (clickCount=%d), trackingDesktop=true\n", clickCount);
                    /* Start tracking for potential drag */
                    g_dispatcher.trackingDesktop = true;
                    return true;
                } else {
                    EVT_LOG_DEBUG("[DESK CLICK] No icon hit, trackingDesktop stays %d\n",
                                 g_dispatcher.trackingDesktop);
                }

                /* Otherwise just a desktop click */
                EVT_LOG_DEBUG("Click on desktop (no icon)\n");
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

    EVT_LOG_DEBUG("HandleKeyDownEvent: key='%c' (0x%02x), cmd=%d\n",
                 (key >= 32 && key < 127) ? key : '?', key, cmdKeyDown);

    if (SimpleText_DispatchEvent(event)) {
        return true;
    }

    /* Handle special keys without command modifier */
    if (!cmdKeyDown) {
        switch (key) {
            case 0x09:  /* Tab key */
                {
                    extern void SelectNextDesktopIcon(void);
                    SelectNextDesktopIcon();
                    EVT_LOG_DEBUG("Tab pressed - selecting next desktop icon\n");
                }
                return true;

            case 0x0D:  /* Enter/Return key */
                {
                    extern void OpenSelectedDesktopIcon(void);
                    OpenSelectedDesktopIcon();
                    EVT_LOG_DEBUG("Enter pressed - opening selected icon\n");
                }
                return true;
        }
    }

    if (cmdKeyDown) {
        /* Command key combinations map to menu items via MenuKey */
        long menuChoice = MenuKey(key);

        if (menuChoice != 0) {
            /* MenuKey found a matching menu item */
            short menuID = (menuChoice >> 16) & 0xFFFF;
            short itemID = menuChoice & 0xFFFF;

            EVT_LOG_DEBUG("Command key '%c' mapped to menu %d, item %d\n",
                         key, menuID, itemID);

            /* Execute the menu command */
            DoMenuCommand(menuID, itemID);

            /* Unhighlight the menu */
            HiliteMenu(0);

            return true;
        }

        /* No menu match - check for special system shortcuts */
        switch (key) {
            case 'q':
            case 'Q':
                /* Quit - try to find in File menu if not already mapped */
                EVT_LOG_DEBUG("Quit requested\n");
                return true;

            default:
                EVT_LOG_TRACE("Unhandled command key: '%c'\n", key);
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
            EVT_LOG_DEBUG("Key '%c' (0x%02x) → TextEdit window 0x%08x\n",
                         (key >= 32 && key < 127) ? key : '?', key, (unsigned int)frontWindow);
            TextEdit_HandleEvent(event);
            return true;
        }

        /* Other applications would handle their own keys */
        EVT_LOG_DEBUG("Key '%c' to window 0x%08x (no handler)\n", key, (unsigned int)frontWindow);
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
    EVT_LOG_DEBUG("[HandleUpdate] ENTRY, event=%p\n", event);
    WindowPtr updateWindow = (WindowPtr)(event->message);

    EVT_LOG_DEBUG("HandleUpdate: window=0x%08x\n", (unsigned int)updateWindow);

    if (updateWindow) {
        if (SimpleText_HandleWindowUpdate(updateWindow)) {
            EVT_LOG_DEBUG("HandleUpdate: SimpleText handled update\n");
            return true;
        }
        /* Check if this is the About This Macintosh window */
        EVT_LOG_DEBUG("HandleUpdate: checking if About window...\n");
        if (AboutWindow_IsOurs(updateWindow)) {
            /* About window handles its own BeginUpdate/EndUpdate */
            EVT_LOG_DEBUG("HandleUpdate: About window, delegating...\n");
            AboutWindow_HandleUpdate(updateWindow);
            return true;
        }
        EVT_LOG_DEBUG("HandleUpdate: not About window, proceeding...\n");

        /* Begin update to set up clipping */
        EVT_LOG_DEBUG("HandleUpdate: calling BeginUpdate...\n");
        BeginUpdate(updateWindow);
        EVT_LOG_DEBUG("HandleUpdate: BeginUpdate returned\n");

        /* Draw window contents */
        EVT_LOG_DEBUG("HandleUpdate: calling SetPort...\n");
        SetPort((GrafPtr)updateWindow);
        EVT_LOG_DEBUG("HandleUpdate: SetPort returned\n");

        /* Check if this is a folder window - use new integrated drawing */
        extern Boolean IsFolderWindow(WindowPtr w);
        extern void FolderWindow_Draw(WindowPtr w);

        EVT_LOG_DEBUG("HandleUpdate: checking if folder window...\n");
        if (IsFolderWindow(updateWindow)) {
            EVT_LOG_DEBUG("HandleUpdate: is folder window, calling FolderWindow_Draw...\n");
            /* Call integrated folder window drawing (handles icons, selection, etc.) */
            FolderWindow_Draw(updateWindow);
            EVT_LOG_DEBUG("HandleUpdate: FolderWindow_Draw returned\n");
        } else {
            EVT_LOG_DEBUG("HandleUpdate: not folder window, erasing rect...\n");
            /* Application would do the actual drawing */
            /* For now, just fill with white to show content area */
            Rect r = updateWindow->port.portRect;
            EraseRect(&r);
            EVT_LOG_DEBUG("HandleUpdate: EraseRect returned\n");
        }

        /* Draw grow icon if window has grow box */
        EVT_LOG_DEBUG("HandleUpdate: checking for grow icon...\n");
        if (updateWindow->windowKind >= 0) {
            EVT_LOG_DEBUG("HandleUpdate: drawing grow icon...\n");
            DrawGrowIcon(updateWindow);
            EVT_LOG_DEBUG("HandleUpdate: DrawGrowIcon returned\n");
        }

        /* End update to restore clipping */
        EVT_LOG_DEBUG("HandleUpdate: calling EndUpdate...\n");
        EndUpdate(updateWindow);
        EVT_LOG_DEBUG("HandleUpdate: EndUpdate returned\n");

        /* Log successful update */
        EVT_LOG_DEBUG("UPDATE: drew content for window=%p\n", updateWindow);
    } else {
        /* NULL window = desktop/background update */
        EVT_LOG_DEBUG("HandleUpdate: NULL window, redrawing desktop\n");
        extern void DrawDesktop(void);
        extern void DrawVolumeIcon(void);
        DrawDesktop();
        DrawVolumeIcon();
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

    if (SimpleText_DispatchEvent(event)) {
        return true;
    }

    EVT_LOG_DEBUG("HandleActivate: window=0x%08x, activating=%d\n",
                 (unsigned int)window, activating);

    if (window) {
        if (activating) {
            /* Window is being activated */
            g_dispatcher.activeWindow = window;

            /* Restore keyboard focus and show focus ring */
            WM_OnActivate(window);

            /* Highlight window controls, enable menus, etc. */
            /* Application would handle this */
        } else {
            /* Window is being deactivated */
            if (g_dispatcher.activeWindow == window) {
                g_dispatcher.activeWindow = NULL;
            }

            /* Suspend keyboard focus and hide focus ring */
            WM_OnDeactivate(window);

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
    EVT_LOG_DEBUG("HandleDisk: message=0x%08x\n", (unsigned int)event->message);
    return true;
}

/**
 * Handle operating system events
 */
Boolean HandleOSEvent(EventRecord* event)
{
    /* Suspend/resume, mouse moved out of region, etc. */
    if (SimpleText_DispatchEvent(event)) {
        return true;
    }

    short osMessage = (event->message >> 24) & 0xFF;

    switch (osMessage) {
        case 1:  /* Suspend event */
            EVT_LOG_DEBUG("Application suspended\n");
            break;

        case 2:  /* Resume event */
            EVT_LOG_DEBUG("Application resumed\n");
            break;

        case 0xFA:  /* Mouse moved out of region */
            EVT_LOG_DEBUG("Mouse moved out of region\n");
            break;

        default:
            EVT_LOG_DEBUG("OS Event: 0x%02x\n", osMessage);
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
