/**
 * @file event_manager.c
 * @brief Event-loop integration and WaitNextEvent for System 7.1
 *
 * GetNextEvent and EventAvail use the process-aware queue in
 * ProcessMgr/EventIntegration.c. This file owns WaitNextEvent and initializes
 * the active queue through InitEvents.
 *
 */

#include "SystemTypes.h"
#include "DeskManager/DeskManager.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventManagerInternal.h"
#include "Platform/PS2Input.h"
#include "ProcessMgr/ProcessMgr.h"
#include "QuickDraw/QDRegions.h"
#include "EventManager/EventLogging.h"
#include "TimeManager/TimeBase.h"

/* Mouse and timing state */
static Point g_mousePos = {100, 100};

/*
 * InitEvents - initialize the process-aware Event Manager queue.
 */
SInt16 InitEvents(SInt16 numEvents) {
    (void)numEvents;
    Event_InitQueue();
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

/* UpdateMouseState is provided by ModernInput.c */
extern void UpdateMouseState(Point newPos, UInt8 buttonState);

/**
 * GenerateSystemEvent - Internal function to generate system events
 * Used by other system components to post events
 */
void GenerateSystemEvent(SInt16 eventType, SInt32 message, Point where, SInt16 modifiers) {
    /* All five are 32-bit: %d/%x would pass 4-byte ints where
     * the printf expects longs. */
    EVT_LOG_DEBUG("GenerateSystemEvent: type=%ld, msg=0x%lx, where=(%ld,%ld), mod=0x%04lx\n",
                  (long)eventType, (unsigned long)message,
                  (long)where.h, (long)where.v, (unsigned long)modifiers);

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
