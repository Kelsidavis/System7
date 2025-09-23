/**
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

/* External serial print for debug logging */
extern void serial_printf(const char* fmt, ...);

/* Simple event queue implementation */
#define MAX_EVENTS 32
static struct {
    EventRecord events[MAX_EVENTS];
    int head;
    int tail;
    int count;
} g_eventQueue = {0};

/* Mouse and timing state */
static Point g_mousePos = {100, 100};
static UInt32 g_tickCount = 0;

/* GetMouse is provided by PS2Controller.c */
extern void GetMouse(Point* mouseLoc);

/* TickCount is provided by sys71_stubs.c */
extern UInt32 TickCount(void);

/**
 * GetNextEvent - Canonical implementation
 * Retrieves and removes the next matching event from the queue
 */
Boolean GetNextEvent(short eventMask, EventRecord* theEvent) {
    static int gne_calls = 0;
    gne_calls++;

    /* Always log first few calls and then periodically */
    if (gne_calls <= 5 || (gne_calls % 1000) == 0) {
        serial_printf("GetNextEvent: Call #%d with mask=0x%04x, queue count=%d\n",
                      gne_calls, eventMask, g_eventQueue.count);
    }

    /* Log what events we're looking for */
    if (eventMask & mDownMask) serial_printf("  Looking for: mouseDown\n");
    if (eventMask & mUpMask) serial_printf("  Looking for: mouseUp\n");
    if (eventMask & keyDownMask) serial_printf("  Looking for: keyDown\n");
    if (eventMask & keyUpMask) serial_printf("  Looking for: keyUp\n");
    if (eventMask & autoKeyMask) serial_printf("  Looking for: autoKey\n");
    if (eventMask & updateMask) serial_printf("  Looking for: update\n");
    if (eventMask & diskMask) serial_printf("  Looking for: disk\n");
    if (eventMask & activMask) serial_printf("  Looking for: activate\n");

    /* Check if queue has events */
    if (g_eventQueue.count == 0) {
        serial_printf("GetNextEvent: Queue empty, returning false\n");
        return false;
    }

    /* Find the next event matching the mask */
    int index = g_eventQueue.head;
    int checked = 0;

    while (checked < g_eventQueue.count) {
        EventRecord* evt = &g_eventQueue.events[index];

        /* Check if event matches mask */
        if ((1 << evt->what) & eventMask) {
            /* Debug: Show what we're retrieving */
            const char* eventName = "unknown";
            switch (evt->what) {
                case 0: eventName = "null"; break;
                case 1: eventName = "mouseDown"; break;
                case 2: eventName = "mouseUp"; break;
                case 3: eventName = "keyDown"; break;
                case 4: eventName = "keyUp"; break;
                case 5: eventName = "autoKey"; break;
                case 6: eventName = "update"; break;
                case 7: eventName = "disk"; break;
                case 8: eventName = "activate"; break;
                case 15: eventName = "osEvt"; break;
                case 23: eventName = "highLevel"; break;
            }
            serial_printf("GetNextEvent: Found matching event: %s (type=%d) at index=%d\n",
                         eventName, evt->what, index);
            serial_printf("GetNextEvent: Event where={x=%d,y=%d}, msg=0x%08x, modifiers=0x%04x\n",
                         evt->where.h, evt->where.v, evt->message, evt->modifiers);

            /* Copy event to caller */
            if (theEvent) {
                *theEvent = *evt;
                serial_printf("GetNextEvent: Copied to caller, where={v=%d,h=%d}\n",
                             theEvent->where.v, theEvent->where.h);
            }

            /* Remove event from queue */
            if (index == g_eventQueue.head) {
                /* Easy case - removing from head */
                g_eventQueue.head = (g_eventQueue.head + 1) % MAX_EVENTS;
                g_eventQueue.count--;
            } else {
                /* Need to shift events to fill the gap */
                int next = (index + 1) % MAX_EVENTS;
                while (next != g_eventQueue.tail) {
                    g_eventQueue.events[index] = g_eventQueue.events[next];
                    index = next;
                    next = (next + 1) % MAX_EVENTS;
                }
                g_eventQueue.tail = (g_eventQueue.tail - 1 + MAX_EVENTS) % MAX_EVENTS;
                g_eventQueue.count--;
            }

            if (evt->what == mouseDown) {
                serial_printf("GetNextEvent: Returning mouseDown at (%d,%d)\n",
                             theEvent->where.h, theEvent->where.v);
            }

            return true;
        }

        index = (index + 1) % MAX_EVENTS;
        checked++;
    }

    serial_printf("GetNextEvent: No matching event found\n");
    return false;
}

/**
 * EventAvail - Check if event is available without removing it
 * New function added for System 7.1 compatibility
 */
Boolean EventAvail(short eventMask, EventRecord* theEvent) {
    serial_printf("EventAvail: Called with mask=0x%04x, queue count=%d\n",
                  eventMask, g_eventQueue.count);

    /* Check if queue has events */
    if (g_eventQueue.count == 0) {
        serial_printf("EventAvail: Queue empty, returning false\n");
        return false;
    }

    /* Find the next event matching the mask */
    int index = g_eventQueue.head;
    int checked = 0;

    while (checked < g_eventQueue.count) {
        EventRecord* evt = &g_eventQueue.events[index];

        /* Check if event matches mask */
        if ((1 << evt->what) & eventMask) {
            const char* eventName = "unknown";
            switch (evt->what) {
                case 0: eventName = "null"; break;
                case 1: eventName = "mouseDown"; break;
                case 2: eventName = "mouseUp"; break;
                case 3: eventName = "keyDown"; break;
                case 4: eventName = "keyUp"; break;
                case 5: eventName = "autoKey"; break;
                case 6: eventName = "update"; break;
                case 7: eventName = "disk"; break;
                case 8: eventName = "activate"; break;
                case 15: eventName = "osEvt"; break;
                case 23: eventName = "highLevel"; break;
            }
            serial_printf("EventAvail: Found matching event: %s (type=%d) at index=%d\n",
                         eventName, evt->what, index);

            /* Copy event to caller WITHOUT removing from queue */
            if (theEvent) {
                *theEvent = *evt;
                serial_printf("EventAvail: Copied to caller (not removed), where={v=%d,h=%d}\n",
                             theEvent->where.v, theEvent->where.h);
            }

            return true;
        }

        index = (index + 1) % MAX_EVENTS;
        checked++;
    }

    serial_printf("EventAvail: No matching event found\n");
    return false;
}

/**
 * PostEvent - Post an event to the queue
 * Core function for adding events to the system
 */
SInt16 PostEvent(SInt16 eventNum, SInt32 eventMsg) {
    /* Debug: log post with event name */
    const char* eventName = "unknown";
    switch (eventNum) {
        case 0: eventName = "null"; break;
        case 1: eventName = "mouseDown"; break;
        case 2: eventName = "mouseUp"; break;
        case 3: eventName = "keyDown"; break;
        case 4: eventName = "keyUp"; break;
        case 5: eventName = "autoKey"; break;
        case 6: eventName = "update"; break;
        case 7: eventName = "disk"; break;
        case 8: eventName = "activate"; break;
        case 15: eventName = "osEvt"; break;
        case 23: eventName = "highLevel"; break;
    }
    serial_printf("PostEvent: Posting %s (type=%d), msg=0x%08x, queue count=%d\n",
                  eventName, eventNum, eventMsg, g_eventQueue.count);

    /* Check if queue is full */
    if (g_eventQueue.count >= MAX_EVENTS) {
        serial_printf("PostEvent: Event queue full!\n");
        return -1; /* queueFull error */
    }

    /* Add event to queue */
    EventRecord* evt = &g_eventQueue.events[g_eventQueue.tail];
    evt->what = eventNum;
    evt->message = eventMsg;
    evt->when = TickCount();

    /* Get current mouse position for all events */
    GetMouse(&evt->where);
    g_mousePos = evt->where;  /* Update our cached position */

    /* For mouse events, message contains additional data like click count */
    if (eventNum == mouseDown || eventNum == mouseUp) {
        serial_printf("PostEvent: Mouse event with message=0x%08x at (%d,%d)\n",
                     eventMsg, evt->where.h, evt->where.v);
    }

    evt->modifiers = 0;  /* TODO: Get keyboard modifiers */

    /* Debug: Print actual coordinates we're storing */
    serial_printf("PostEvent: Successfully posted %s at position %d, queue now has %d events\n",
                 eventName, g_eventQueue.tail, g_eventQueue.count + 1);

    g_eventQueue.tail = (g_eventQueue.tail + 1) % MAX_EVENTS;
    g_eventQueue.count++;

    if (eventNum == mouseDown) {
        serial_printf("PostEvent: Added mouseDown at (%d,%d) to queue (count=%d)\n",
                     evt->where.h, evt->where.v, g_eventQueue.count);
    }

    return 0; /* noErr */
}

/* InitEvents is provided by sys71_stubs.c - we just use the queue here */
extern SInt16 InitEvents(SInt16 numEvents);

/* WaitNextEvent is provided by sys71_stubs.c - it can call our GetNextEvent */
extern Boolean WaitNextEvent(short eventMask, EventRecord* theEvent,
                     UInt32 sleep, RgnHandle mouseRgn);

/**
 * FlushEvents - Remove events from the queue
 * Used to clear unwanted events
 */
void FlushEvents(short whichMask, short stopMask) {
    serial_printf("FlushEvents: Flushing events with mask=0x%04x, stop=0x%04x\n",
                  whichMask, stopMask);

    int index = g_eventQueue.head;
    int checked = 0;

    while (checked < g_eventQueue.count) {
        EventRecord* evt = &g_eventQueue.events[index];

        /* Check if we should stop */
        if ((1 << evt->what) & stopMask) {
            serial_printf("FlushEvents: Stopping at event type %d\n", evt->what);
            break;
        }

        /* Check if this event should be flushed */
        if ((1 << evt->what) & whichMask) {
            serial_printf("FlushEvents: Removing event type %d\n", evt->what);

            /* Remove this event */
            if (index == g_eventQueue.head) {
                g_eventQueue.head = (g_eventQueue.head + 1) % MAX_EVENTS;
                g_eventQueue.count--;
                /* Don't increment index since head moved */
            } else {
                /* Shift events to fill gap */
                int next = (index + 1) % MAX_EVENTS;
                while (next != g_eventQueue.tail) {
                    g_eventQueue.events[index] = g_eventQueue.events[next];
                    index = next;
                    next = (next + 1) % MAX_EVENTS;
                }
                g_eventQueue.tail = (g_eventQueue.tail - 1 + MAX_EVENTS) % MAX_EVENTS;
                g_eventQueue.count--;
            }
        } else {
            index = (index + 1) % MAX_EVENTS;
            checked++;
        }
    }

    serial_printf("FlushEvents: Complete, queue now has %d events\n", g_eventQueue.count);
}

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
void GenerateSystemEvent(short eventType, int message, Point where, short modifiers) {
    serial_printf("GenerateSystemEvent: type=%d, msg=0x%x, where=(%d,%d), mod=0x%04x\n",
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