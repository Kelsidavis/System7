/**
#include "EventManager/EventManagerInternal.h"
 * @file event_manager.c
 * @brief Canonical Event Manager Implementation for System 7.1
 *
 * This is the single authoritative implementation of GetNextEvent and EventAvail.
 * All other files should call these functions, not reimplement them.
 *
 * This file consolidates the working queue-based implementation from sys71_stubs.c
 * with proper Event Manager structure and debug logging.
 */

#include <string.h>
#include "../../include/MacTypes.h"
#include "../../include/EventManager/EventTypes.h"
#include "../../include/EventManager/EventManager.h"
#include "../../include/ProcessMgr/ProcessMgr.h"
#include "../../include/QuickDraw/QDRegions.h"
#include "EventManager/EventLogging.h"

/* External serial print for debug logging */

/* Simple event queue implementation */
#define MAX_EVENTS 32
static struct {
    EventRecord events[MAX_EVENTS];
    int head;
    int tail;
    int count;
} g_eventQueue __attribute__((unused)) = {0};

/* Mouse and timing state */
static Point g_mousePos = {100, 100};

/* GetMouse is provided by PS2Controller.c */
extern void GetMouse(Point* mouseLoc);

/* GetPS2Modifiers is provided by PS2Controller.c */
extern UInt16 GetPS2Modifiers(void);

/* TickCount is in TimeManager/TimeBase.c */
extern UInt32 TickCount(void);

/**
 * EventAvail - Check if event is available without removing it
 * New function added for System 7.1 compatibility
 */

/**
 * PostEvent - Post an event to the queue
 * Core function for adding events to the system
 */

/*
 * InitEvents - empty the event queue.
 *
 * The queue is a fixed MAX_EVENTS entries, so the size asked for is not
 * honoured. This was a stub in sys71_stubs.c that did nothing and said the
 * Event Manager was initialized elsewhere; nothing else emptied the queue.
 */
SInt16 InitEvents(SInt16 numEvents);
SInt16 InitEvents(SInt16 numEvents) {
    (void)numEvents;
    g_eventQueue.head = 0;
    g_eventQueue.tail = 0;
    g_eventQueue.count = 0;
    return 0;
}

/**
 * WaitNextEvent - Core of cooperative multitasking
 * Applications call this to yield control and allow other processes to run
 *
 * This is the heart of System 7's cooperative multitasking. Applications
 * call WaitNextEvent in their event loop, which allows the Process Manager
 * to switch to other processes while waiting for events.
 *
 * @param eventMask Mask of events to retrieve
 * @param theEvent Event record to fill
 * @param sleep Maximum ticks to wait for an event
 * @param mouseRgn Region where mouse can move without generating null events.
 *                 If the mouse moves outside this region, a null event is
 *                 generated immediately to wake the application.
 */
Boolean WaitNextEvent(short eventMask, EventRecord* theEvent, UInt32 sleep, RgnHandle mouseRgn) {
    Boolean eventAvailable = false;
    UInt32 startTime = TickCount();
    ProcessControlBlock* nextProcess;
    Point initialMousePos;
    Point currentMousePos;

    /* Save initial mouse position for mouseRgn tracking */
    GetMouse(&initialMousePos);

    /* Check for immediate events */
    eventAvailable = GetNextEvent(eventMask, theEvent);
    if (eventAvailable) {
        return true;
    }

    /* Cooperative yield - give other processes a chance to run */
    if (gMultiFinderActive) {
        OSErr err = Scheduler_GetNextProcess(&nextProcess);
        if (err == noErr && nextProcess != gCurrentProcess) {
            Context_Switch(nextProcess);
        }
    }

    /* Wait for events or timeout */
    do {
        /* Check if mouse has moved outside the mouseRgn */
        if (mouseRgn != NULL) {
            GetMouse(&currentMousePos);
            /* If mouse moved outside the region, generate null event immediately */
            if (!PtInRgn(currentMousePos, mouseRgn)) {
                EVT_LOG_TRACE("WaitNextEvent: Mouse left region at (%d,%d), generating null event\n",
                             currentMousePos.h, currentMousePos.v);
                theEvent->what = nullEvent;
                theEvent->message = 0;
                theEvent->when = TickCount();
                theEvent->modifiers = GetPS2Modifiers();
                theEvent->where = currentMousePos;
                return true;
            }
        }

        eventAvailable = GetNextEvent(eventMask, theEvent);
        if (eventAvailable) {
            break;
        }

        /* Run system tasks (clock update, DA idle) during wait */
        {
            extern void SystemTask(void);
            SystemTask();
        }

        /* Yield to other processes during wait */
        if (gMultiFinderActive) {
            Scheduler_GetNextProcess(&nextProcess);
            if (nextProcess != gCurrentProcess) {
                Context_Switch(nextProcess);
            }
        }

    } while (sleep > 0 && (TickCount() - startTime) < sleep);

    /* Generate null event if no real event occurred */
    if (!eventAvailable) {
        theEvent->what = nullEvent;
        theEvent->message = 0;
        theEvent->when = TickCount();
        theEvent->modifiers = GetPS2Modifiers();
        GetMouse(&theEvent->where);
        eventAvailable = true;
    }

    return eventAvailable;
}

/**
 * FlushEvents - Remove events from the queue
 * Used to clear unwanted events
 */

/* Button is provided by PS2Controller.c */
extern Boolean Button(void);

/* StillDown is provided by control_stubs.c */
extern Boolean StillDown(void);

/* GetKeys is provided by KeyboardEvents.c */
extern void GetKeys(KeyMap theKeys);

/* UpdateMouseState is provided by ModernInput.c */
extern void UpdateMouseState(Point newPos, UInt8 buttonState);

/**
 * GenerateSystemEvent - Internal function to generate system events
 * Used by other system components to post events
 */
void GenerateSystemEvent(SInt16 eventType, SInt32 message, Point where, SInt16 modifiers) {
    EVT_LOG_DEBUG("GenerateSystemEvent: type=%d, msg=0x%x, where=(%d,%d), mod=0x%04x\n",
                  eventType, message, where.h, where.v, modifiers);

    /* Update cached mouse position if provided */
    if (where.h != 0 || where.v != 0) {
        g_mousePos = where;
    } else {
        /* Get current mouse position */
        GetMouse(&g_mousePos);
    }

    /* Post the event */
    PostEvent(eventType, message);
}