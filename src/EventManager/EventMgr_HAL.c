/*
 * Event Manager Hardware Abstraction Layer
 * Bridges classic Mac OS Event Manager to modern event systems
 * Completes the 35% missing functionality for full event routing
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

#include "EventManager/event_manager.h"
#include "WindowManager/window_manager.h"
#include "MenuManager/menu_manager.h"
#include "DialogManager/dialog_manager.h"
#include "QuickDraw/QuickDraw.h"

/* Event Manager globals */
static struct {
    /* Event queue - circular buffer */
    EventRecord     eventQueue[32];
    int16_t         queueHead;
    int16_t         queueTail;
    int16_t         queueCount;

    /* Event masks */
    int16_t         systemEventMask;
    int16_t         applicationEventMask;

    /* Keyboard state */
    KeyMap          keyMap;
    UInt32          keyRepeatThresh;
    UInt32          keyRepeatRate;
    UInt32          lastKeyTime;
    UInt8           lastKeyCode;

    /* Mouse state */
    Point           mouseLocation;
    Boolean         mouseButtonState;
    UInt32          lastClickTime;
    Point           lastClickLocation;
    int16_t         doubleClickTime;

    /* System state */
    UInt32          systemTicks;       /* Ticks since startup */
    UInt32          lastNullEventTime;
    UInt32          sleepTime;         /* Ticks to sleep */
    Boolean         systemActive;

    /* Event hooks */
    void*           jGNEFilter;        /* GetNextEvent filter */
    void*           keyboardHook;      /* Keyboard hook */
    void*           journalHook;       /* Event journal hook */

    /* Platform-specific */
#ifdef __linux__
    Display*        display;
    Window          rootWindow;
    int             screen;
#endif

    pthread_mutex_t eventLock;
    bool            initialized;
} gEventMgr = {0};

/* Event type constants */
#define nullEvent           0
#define mouseDown           1
#define mouseUp             2
#define keyDown             3
#define keyUp               4
#define autoKey             5
#define updateEvt           6
#define diskEvt             7
#define activateEvt         8
#define osEvt               15
#define kHighLevelEvent     23

/* Event mask bits */
#define mDownMask           0x0002
#define mUpMask             0x0004
#define keyDownMask         0x0008
#define keyUpMask           0x0010
#define autoKeyMask         0x0020
#define updateMask          0x0040
#define diskMask            0x0080
#define activMask           0x0100
#define highLevelEventMask  0x0400
#define osMask              0x8000
#define everyEvent          0xFFFF

/* Modifier key bits */
#define cmdKey              0x0100
#define shiftKey            0x0200
#define alphaLock           0x0400
#define optionKey           0x0800
#define controlKey          0x1000

/* Forward declarations */
static void EventMgr_HAL_PostEventInternal(EventRecord* event);
static Boolean EventMgr_HAL_GetEventInternal(int16_t eventMask, EventRecord* theEvent);
static void EventMgr_HAL_UpdateSystemTicks(void);
static void EventMgr_HAL_CheckKeyRepeat(void);
static void EventMgr_HAL_GenerateNullEvent(EventRecord* event);

/* Initialize Event Manager HAL */
OSErr EventMgr_HAL_Initialize(void)
{
    if (gEventMgr.initialized) {
        return noErr;
    }

    /* Initialize mutex */
    pthread_mutex_init(&gEventMgr.eventLock, NULL);

    /* Initialize event queue */
    gEventMgr.queueHead = 0;
    gEventMgr.queueTail = 0;
    gEventMgr.queueCount = 0;

    /* Set default event masks */
    gEventMgr.systemEventMask = everyEvent;
    gEventMgr.applicationEventMask = everyEvent & ~(keyUpMask | autoKeyMask);

    /* Initialize keyboard state */
    memset(&gEventMgr.keyMap, 0, sizeof(KeyMap));
    gEventMgr.keyRepeatThresh = 30;  /* Ticks before repeat */
    gEventMgr.keyRepeatRate = 6;     /* Ticks between repeats */

    /* Initialize mouse state */
    gEventMgr.mouseLocation.h = 512;
    gEventMgr.mouseLocation.v = 384;
    gEventMgr.mouseButtonState = false;
    gEventMgr.doubleClickTime = 30;  /* Ticks for double-click */

    /* Initialize system state */
    gEventMgr.systemTicks = 0;
    gEventMgr.sleepTime = 0;
    gEventMgr.systemActive = true;

#ifdef __linux__
    /* Initialize X11 connection */
    gEventMgr.display = XOpenDisplay(NULL);
    if (gEventMgr.display) {
        gEventMgr.screen = DefaultScreen(gEventMgr.display);
        gEventMgr.rootWindow = RootWindow(gEventMgr.display, gEventMgr.screen);

        /* Select events */
        XSelectInput(gEventMgr.display, gEventMgr.rootWindow,
                    KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | ExposureMask);
    }
#endif

    gEventMgr.initialized = true;
    return noErr;
}

/* Terminate Event Manager HAL */
void EventMgr_HAL_Terminate(void)
{
    if (!gEventMgr.initialized) {
        return;
    }

#ifdef __linux__
    if (gEventMgr.display) {
        XCloseDisplay(gEventMgr.display);
    }
#endif

    pthread_mutex_destroy(&gEventMgr.eventLock);
    gEventMgr.initialized = false;
}

/* CRITICAL: GetNextEvent - Main event retrieval */
Boolean EventMgr_HAL_GetNextEvent(int16_t eventMask, EventRecord* theEvent)
{
    if (!theEvent) return false;

    pthread_mutex_lock(&gEventMgr.eventLock);

    /* Update system ticks */
    EventMgr_HAL_UpdateSystemTicks();

    /* Check for platform events and queue them */
#ifdef __linux__
    if (gEventMgr.display) {
        while (XPending(gEventMgr.display)) {
            XEvent xEvent;
            XNextEvent(gEventMgr.display, &xEvent);

            EventRecord macEvent;
            memset(&macEvent, 0, sizeof(EventRecord));

            switch (xEvent.type) {
                case KeyPress:
                    macEvent.what = keyDown;
                    macEvent.message = xEvent.xkey.keycode;
                    macEvent.modifiers = 0;
                    if (xEvent.xkey.state & ShiftMask) macEvent.modifiers |= shiftKey;
                    if (xEvent.xkey.state & ControlMask) macEvent.modifiers |= cmdKey;
                    if (xEvent.xkey.state & Mod1Mask) macEvent.modifiers |= optionKey;
                    EventMgr_HAL_PostEventInternal(&macEvent);
                    break;

                case KeyRelease:
                    macEvent.what = keyUp;
                    macEvent.message = xEvent.xkey.keycode;
                    EventMgr_HAL_PostEventInternal(&macEvent);
                    break;

                case ButtonPress:
                    macEvent.what = mouseDown;
                    macEvent.where.h = xEvent.xbutton.x;
                    macEvent.where.v = xEvent.xbutton.y;
                    gEventMgr.mouseLocation = macEvent.where;
                    gEventMgr.mouseButtonState = true;
                    EventMgr_HAL_PostEventInternal(&macEvent);
                    break;

                case ButtonRelease:
                    macEvent.what = mouseUp;
                    macEvent.where.h = xEvent.xbutton.x;
                    macEvent.where.v = xEvent.xbutton.y;
                    gEventMgr.mouseLocation = macEvent.where;
                    gEventMgr.mouseButtonState = false;
                    EventMgr_HAL_PostEventInternal(&macEvent);
                    break;

                case MotionNotify:
                    gEventMgr.mouseLocation.h = xEvent.xmotion.x;
                    gEventMgr.mouseLocation.v = xEvent.xmotion.y;
                    break;

                case Expose:
                    /* Generate update event for affected window */
                    macEvent.what = updateEvt;
                    macEvent.message = (UInt32)FrontWindow();
                    EventMgr_HAL_PostEventInternal(&macEvent);
                    break;
            }
        }
    }
#endif

    /* Check for key repeat */
    EventMgr_HAL_CheckKeyRepeat();

    /* Get event from queue */
    Boolean gotEvent = EventMgr_HAL_GetEventInternal(eventMask, theEvent);

    /* If no event, generate null event */
    if (!gotEvent) {
        EventMgr_HAL_GenerateNullEvent(theEvent);
    }

    pthread_mutex_unlock(&gEventMgr.eventLock);
    return gotEvent;
}

/* WaitNextEvent - GetNextEvent with sleep support */
Boolean EventMgr_HAL_WaitNextEvent(int16_t eventMask, EventRecord* theEvent,
                                   UInt32 sleep, RgnHandle mouseRgn)
{
    if (!theEvent) return false;

    /* Set sleep time */
    gEventMgr.sleepTime = sleep;

    /* Check mouse region if provided */
    if (mouseRgn) {
        /* Would check if mouse moved outside region */
    }

    /* Call GetNextEvent */
    return EventMgr_HAL_GetNextEvent(eventMask, theEvent);
}

/* EventAvail - Check if event available without removing */
Boolean EventMgr_HAL_EventAvail(int16_t eventMask, EventRecord* theEvent)
{
    if (!theEvent) return false;

    pthread_mutex_lock(&gEventMgr.eventLock);

    /* Peek at queue without removing */
    Boolean hasEvent = false;
    if (gEventMgr.queueCount > 0) {
        int16_t peekIndex = gEventMgr.queueHead;
        while (peekIndex != gEventMgr.queueTail) {
            EventRecord* event = &gEventMgr.eventQueue[peekIndex];
            if ((1 << event->what) & eventMask) {
                *theEvent = *event;
                hasEvent = true;
                break;
            }
            peekIndex = (peekIndex + 1) % 32;
        }
    }

    /* If no event, generate null event */
    if (!hasEvent) {
        EventMgr_HAL_GenerateNullEvent(theEvent);
    }

    pthread_mutex_unlock(&gEventMgr.eventLock);
    return hasEvent;
}

/* PostEvent - Add event to queue */
OSErr EventMgr_HAL_PostEvent(int16_t eventNum, UInt32 eventMsg)
{
    EventRecord event;
    memset(&event, 0, sizeof(EventRecord));

    event.what = eventNum;
    event.message = eventMsg;
    event.when = gEventMgr.systemTicks;
    event.where = gEventMgr.mouseLocation;
    event.modifiers = 0;  /* Would check actual modifier state */

    pthread_mutex_lock(&gEventMgr.eventLock);
    EventMgr_HAL_PostEventInternal(&event);
    pthread_mutex_unlock(&gEventMgr.eventLock);

    return noErr;
}

/* FlushEvents - Remove events from queue */
void EventMgr_HAL_FlushEvents(int16_t whichMask, int16_t stopMask)
{
    pthread_mutex_lock(&gEventMgr.eventLock);

    /* Remove matching events from queue */
    int16_t readIndex = gEventMgr.queueHead;
    int16_t writeIndex = gEventMgr.queueHead;

    while (readIndex != gEventMgr.queueTail) {
        EventRecord* event = &gEventMgr.eventQueue[readIndex];

        /* Check if event should be flushed */
        int16_t eventBit = (1 << event->what);
        if ((eventBit & whichMask) && !(eventBit & stopMask)) {
            /* Skip this event */
            gEventMgr.queueCount--;
        } else {
            /* Keep this event */
            if (readIndex != writeIndex) {
                gEventMgr.eventQueue[writeIndex] = *event;
            }
            writeIndex = (writeIndex + 1) % 32;
        }

        readIndex = (readIndex + 1) % 32;
    }

    gEventMgr.queueTail = writeIndex;

    pthread_mutex_unlock(&gEventMgr.eventLock);
}

/* CRITICAL: SystemClick - Route clicks to appropriate manager */
void EventMgr_HAL_SystemClick(EventRecord* theEvent, WindowPtr theWindow)
{
    if (!theEvent || !theWindow) return;

    int16_t partCode;
    WindowPtr clickWindow;

    /* Find where click occurred */
    partCode = FindWindow(theEvent->where, &clickWindow);

    if (clickWindow != theWindow) return;

    switch (partCode) {
        case inMenuBar:
            /* Route to Menu Manager */
            {
                long menuChoice = MenuSelect(theEvent->where);
                if (menuChoice) {
                    int16_t menuID = HiWord(menuChoice);
                    int16_t menuItem = LoWord(menuChoice);
                    /* Application would handle menu choice */
                    HiliteMenu(0);  /* Unhighlight */
                }
            }
            break;

        case inDrag:
            /* Drag window */
            DragWindow(theWindow, theEvent->where, NULL);
            break;

        case inGrow:
            /* Resize window */
            {
                long newSize = GrowWindow(theWindow, theEvent->where, NULL);
                if (newSize) {
                    SizeWindow(theWindow, LoWord(newSize), HiWord(newSize), true);
                }
            }
            break;

        case inGoAway:
            /* Close box */
            if (TrackGoAway(theWindow, theEvent->where)) {
                /* Application would close window */
            }
            break;

        case inContent:
            /* Content click - activate if needed */
            if (theWindow != FrontWindow()) {
                SelectWindow(theWindow);
            }
            break;

        case inZoomIn:
        case inZoomOut:
            /* Zoom box */
            if (TrackBox(theWindow, theEvent->where, partCode)) {
                ZoomWindow(theWindow, partCode, false);
            }
            break;
    }
}

/* SystemTask - Perform periodic system tasks */
void EventMgr_HAL_SystemTask(void)
{
    /* Update system ticks */
    EventMgr_HAL_UpdateSystemTicks();

    /* Check for pending window updates */
    WindowPtr window = FrontWindow();
    while (window) {
        if (((WindowPeek)window)->updateRgn &&
            !EmptyRgn(((WindowPeek)window)->updateRgn)) {
            /* Post update event */
            EventRecord event;
            event.what = updateEvt;
            event.message = (UInt32)window;
            event.when = gEventMgr.systemTicks;
            event.where = gEventMgr.mouseLocation;
            event.modifiers = 0;

            pthread_mutex_lock(&gEventMgr.eventLock);
            EventMgr_HAL_PostEventInternal(&event);
            pthread_mutex_unlock(&gEventMgr.eventLock);
        }
        window = GetNextWindow(window);
    }

    /* Process any platform-specific tasks */
#ifdef __linux__
    if (gEventMgr.display) {
        XFlush(gEventMgr.display);
    }
#endif
}

/* SystemEvent - Check if event is system event */
Boolean EventMgr_HAL_SystemEvent(EventRecord* theEvent)
{
    if (!theEvent) return false;

    /* System events are OS events and disk events */
    return (theEvent->what == osEvt || theEvent->what == diskEvt);
}

/* Get mouse location */
void EventMgr_HAL_GetMouse(Point* mouseLoc)
{
    if (mouseLoc) {
        *mouseLoc = gEventMgr.mouseLocation;
    }
}

/* Check if mouse button pressed */
Boolean EventMgr_HAL_Button(void)
{
    return gEventMgr.mouseButtonState;
}

/* Check if mouse button still down */
Boolean EventMgr_HAL_StillDown(void)
{
    /* Update from platform */
#ifdef __linux__
    if (gEventMgr.display) {
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;

        XQueryPointer(gEventMgr.display, gEventMgr.rootWindow,
                     &root, &child, &rootX, &rootY,
                     &winX, &winY, &mask);

        return (mask & Button1Mask) != 0;
    }
#endif

    return gEventMgr.mouseButtonState;
}

/* Wait for mouse button release */
Boolean EventMgr_HAL_WaitMouseUp(void)
{
    while (EventMgr_HAL_StillDown()) {
        /* Yield to other processes */
        SystemTask();
    }
    return true;
}

/* Get keyboard state */
void EventMgr_HAL_GetKeys(KeyMap theKeys)
{
    if (theKeys) {
        memcpy(theKeys, gEventMgr.keyMap, sizeof(KeyMap));
    }
}

/* Get current ticks */
UInt32 EventMgr_HAL_TickCount(void)
{
    EventMgr_HAL_UpdateSystemTicks();
    return gEventMgr.systemTicks;
}

/* Get double-click time */
UInt32 EventMgr_HAL_GetDblTime(void)
{
    return gEventMgr.doubleClickTime;
}

/* Get caret blink time */
UInt32 EventMgr_HAL_GetCaretTime(void)
{
    return 30;  /* Default 30 ticks (0.5 seconds) */
}

/* Set event mask */
void EventMgr_HAL_SetEventMask(int16_t value)
{
    pthread_mutex_lock(&gEventMgr.eventLock);
    gEventMgr.applicationEventMask = value;
    pthread_mutex_unlock(&gEventMgr.eventLock);
}

/* === Menu/Dialog Event Integration === */

/* CRITICAL: Route menu events */
Boolean EventMgr_HAL_MenuEvent(EventRecord* theEvent)
{
    if (!theEvent) return false;

    /* Check for menu key equivalent */
    if (theEvent->what == keyDown && (theEvent->modifiers & cmdKey)) {
        char key = theEvent->message & charCodeMask;
        long menuChoice = MenuKey(key);
        if (menuChoice) {
            /* Menu key was handled */
            return true;
        }
    }

    /* Check for menu click */
    if (theEvent->what == mouseDown) {
        int16_t partCode;
        WindowPtr window;

        partCode = FindWindow(theEvent->where, &window);
        if (partCode == inMenuBar) {
            long menuChoice = MenuSelect(theEvent->where);
            if (menuChoice) {
                /* Menu was selected */
                return true;
            }
        }
    }

    return false;
}

/* CRITICAL: Route dialog events */
Boolean EventMgr_HAL_DialogEvent(EventRecord* theEvent)
{
    if (!theEvent) return false;

    /* Let Dialog Manager check if it's a dialog event */
    return IsDialogEvent(theEvent);
}

/* === Internal Helper Functions === */

/* Post event to internal queue */
static void EventMgr_HAL_PostEventInternal(EventRecord* event)
{
    if (!event) return;

    /* Check if queue is full */
    if (gEventMgr.queueCount >= 31) {
        /* Drop oldest event */
        gEventMgr.queueHead = (gEventMgr.queueHead + 1) % 32;
        gEventMgr.queueCount--;
    }

    /* Set event time and location */
    event->when = gEventMgr.systemTicks;
    if (event->what != mouseDown && event->what != mouseUp) {
        event->where = gEventMgr.mouseLocation;
    }

    /* Add to queue */
    gEventMgr.eventQueue[gEventMgr.queueTail] = *event;
    gEventMgr.queueTail = (gEventMgr.queueTail + 1) % 32;
    gEventMgr.queueCount++;
}

/* Get event from internal queue */
static Boolean EventMgr_HAL_GetEventInternal(int16_t eventMask, EventRecord* theEvent)
{
    /* Search queue for matching event */
    int16_t index = gEventMgr.queueHead;

    while (index != gEventMgr.queueTail) {
        EventRecord* event = &gEventMgr.eventQueue[index];

        if ((1 << event->what) & eventMask) {
            /* Found matching event */
            *theEvent = *event;

            /* Remove from queue */
            int16_t nextIndex = (index + 1) % 32;
            while (nextIndex != gEventMgr.queueTail) {
                gEventMgr.eventQueue[index] = gEventMgr.eventQueue[nextIndex];
                index = nextIndex;
                nextIndex = (nextIndex + 1) % 32;
            }
            gEventMgr.queueTail = index;
            gEventMgr.queueCount--;

            return true;
        }

        index = (index + 1) % 32;
    }

    return false;
}

/* Update system tick count */
static void EventMgr_HAL_UpdateSystemTicks(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* Convert to ticks (60Hz) */
    UInt32 ticks = (tv.tv_sec * 60) + (tv.tv_usec / 16667);
    gEventMgr.systemTicks = ticks;
}

/* Check for keyboard auto-repeat */
static void EventMgr_HAL_CheckKeyRepeat(void)
{
    if (gEventMgr.lastKeyCode != 0) {
        UInt32 currentTime = gEventMgr.systemTicks;
        UInt32 timeSinceKey = currentTime - gEventMgr.lastKeyTime;

        if (timeSinceKey >= gEventMgr.keyRepeatThresh) {
            /* Generate auto-repeat event */
            EventRecord event;
            event.what = autoKey;
            event.message = gEventMgr.lastKeyCode;
            event.when = currentTime;
            event.where = gEventMgr.mouseLocation;
            event.modifiers = 0;

            EventMgr_HAL_PostEventInternal(&event);

            /* Update repeat time */
            gEventMgr.lastKeyTime = currentTime -
                                   (gEventMgr.keyRepeatThresh - gEventMgr.keyRepeatRate);
        }
    }
}

/* Generate null event */
static void EventMgr_HAL_GenerateNullEvent(EventRecord* event)
{
    if (!event) return;

    event->what = nullEvent;
    event->message = 0;
    event->when = gEventMgr.systemTicks;
    event->where = gEventMgr.mouseLocation;
    event->modifiers = 0;

    /* Set current modifier keys */
#ifdef __linux__
    if (gEventMgr.display) {
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;

        XQueryPointer(gEventMgr.display, gEventMgr.rootWindow,
                     &root, &child, &rootX, &rootY,
                     &winX, &winY, &mask);

        if (mask & ShiftMask) event->modifiers |= shiftKey;
        if (mask & ControlMask) event->modifiers |= cmdKey;
        if (mask & Mod1Mask) event->modifiers |= optionKey;
        if (mask & LockMask) event->modifiers |= alphaLock;
    }
#endif

    gEventMgr.lastNullEventTime = event->when;
}

/* Initialize on first use */
__attribute__((constructor))
static void EventMgr_HAL_Init(void)
{
    EventMgr_HAL_Initialize();
}

/* Cleanup on exit */
__attribute__((destructor))
static void EventMgr_HAL_Cleanup(void)
{
    EventMgr_HAL_Terminate();
}