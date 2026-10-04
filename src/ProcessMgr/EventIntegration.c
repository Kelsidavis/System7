/*
 * EventIntegration.c - Event Queue and Process Integration
 *
 * Implements event queue management and process-aware event APIs.
 * Provides GetNextEvent, EventAvail, and PostEvent with process
 * unblocking capabilities.
 */

#include "SystemTypes.h"
#include "SystemInternal.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventManager.h"   /* PostEventWithModifiers */
#include "EventManager/KeyboardEvents.h"
#include "ProcessMgr/ProcessMgr.h"
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManagerInternal.h"
#include "ProcessMgr/ProcessLogging.h"
#include "TimeManager/TimeBase.h"
#include "System71StdLib.h"

/* Event queue - ring buffer */
#define EVENT_QUEUE_SIZE 64
static EventRecord gEventQueue[EVENT_QUEUE_SIZE];
static UInt16 gQueueHead = 0;
static UInt16 gQueueTail = 0;
static UInt16 gQueueCount = 0;

/* Forward declarations */
static UInt16 GetModifiers(void);
static Boolean DequeueEvent(EventMask mask, EventRecord* evt);
static Boolean FindQueuedEvent(EventMask mask, EventRecord* evt);
static void NullEventNow(EventRecord* evt);
static void PumpInputEvents(void);
static Boolean CheckSystemEvents(EventMask mask, EventRecord* evt);

/* Copy fields directly so queue rotation remains defined when a full ring's
 * head and tail identify the same slot. */
static void CopyEventRecord(EventRecord* dest, const EventRecord* src) {
    dest->what = src->what;
    dest->message = src->message;
    dest->when = src->when;
    dest->where = src->where;
    dest->modifiers = src->modifiers;
}

static Boolean EventMatchesMask(const EventRecord* evt, EventMask mask) {
    return evt->what < 32 && (((EventMask)1U << evt->what) & mask) != 0;
}

/* The event queue has no input thread; event-query calls pump hardware. */
static void PumpInputEvents(void) {
    ProcessModernInput();
}

/*
 * Proc_GetNextEvent - Process-aware get next event matching mask
 *
 * This is THE cooperative multitasking point. When apps call this,
 * they're saying "I'm idle, let others run"
 *
 * NOTE: This is a process-aware version that integrates with the scheduler.
 *
 * This is the GetNextEvent that runs: the public symbol at the bottom of this
 * file routes here. EventManager/event_manager.c once had a second copy, for a
 * non-cooperative build that no longer exists; a fix written there could never
 * execute.
 */
Boolean Proc_GetNextEvent(EventMask mask, EventRecord* evt) {
    if (!evt) return false;

    /* Turn the crank on the hardware before looking for an event.
     *
     * Input used to be produced only by main.c's loop calling
     * ProcessModernInput, so a nested modal loop - which fetches its own
     * events rather than returning to main - never saw a keystroke or a
     * mouse click at all. The Standard File dialogs sat in exactly such a
     * loop: they drew correctly and then ignored every click, because no
     * mouse event was ever being generated while they ran. EventPumpYield
     * exists for the same reason and has to be remembered at each such loop;
     * asking for an event is the natural place, and is where the real
     * Toolbox pumps, which is why nested modal loops work there.
     *
     * ProcessModernInput is edge-triggered on device state, so calling it
     * from here as well as from the main loop cannot double-report anything.
     *
     * The pointer is brought up to date here for the same reason: only the
     * main loop drew it, so inside an alert's ModalDialog loop it vanished and
     * stayed gone until the alert was dismissed. */
    {
        PumpInputEvents();
        UpdateCursorDisplay();
    }

    /* Check queue first */
    if (DequeueEvent(mask, evt)) {
        /* Unblock any process waiting for this event */
        Proc_UnblockEvent(evt);
        return true;
    }

    /* Check for system-generated events */
    if (CheckSystemEvents(mask, evt)) {
        /* Unblock any process waiting for this event */
        Proc_UnblockEvent(evt);
        return true;
    }

    NullEventNow(evt);
    return false;  /* false means null event */
}

/* No event: a null one, with the mouse and modifiers as they are now. */
static void NullEventNow(EventRecord* evt) {
    evt->what = nullEvent;
    evt->message = 0;
    evt->when = TickCount();
    evt->where.h = 0;
    evt->where.v = 0;
    GetMouse(&evt->where);
    evt->modifiers = GetModifiers();
}

/*
 * GetOSEvent / OSEventAvail - the queue alone (IM I-254). What GetNextEvent
 * adds - update and activate events, desk accessories' keystrokes - is the
 * Toolbox Event Manager's, and a program calling these has asked for none
 * of it. The hardware is pumped first, as GetNextEvent does, so a program
 * polling GetOSEvent in a loop still sees the mouse.
 */
Boolean GetOSEvent(SInt16 mask, EventRecord* evt) {
    if (!evt) return false;
    PumpInputEvents();
    if (DequeueEvent((EventMask)(UInt16)mask, evt)) {
        Proc_UnblockEvent(evt);
        return true;
    }
    NullEventNow(evt);
    return false;
}

Boolean OSEventAvail(SInt16 mask, EventRecord* evt) {
    if (!evt) return false;
    PumpInputEvents();
    if (FindQueuedEvent((EventMask)(UInt16)mask, evt)) {
        return true;
    }
    NullEventNow(evt);
    return false;
}

/* Find the first matching slot without changing queue order. */
static Boolean FindQueuedEventIndex(EventMask mask, UInt16* eventIndex) {
    UInt16 index = gQueueHead;
    UInt16 count = gQueueCount;

    while (count > 0) {
        if (EventMatchesMask(&gEventQueue[index], mask)) {
            *eventIndex = index;
            return true;
        }

        index = (index + 1) % EVENT_QUEUE_SIZE;
        count--;
    }

    return false;
}

/* Copy, but do not remove, the first queued event matching mask. */
static Boolean FindQueuedEvent(EventMask mask, EventRecord* evt) {
    UInt16 index;
    if (!FindQueuedEventIndex(mask, &index)) {
        return false;
    }
    CopyEventRecord(evt, &gEventQueue[index]);
    return true;
}

/*
 * Proc_EventAvail - Process-aware check if event available without removing
 *
 * The public EventAvail entry point below routes here so availability checks
 * use the same process-aware event queue as GetNextEvent.
 */
Boolean Proc_EventAvail(EventMask mask, EventRecord* evt) {
    if (!evt) return false;
    PumpInputEvents();

    if (FindQueuedEvent(mask, evt)) {
        return true;
    }

    /* Check system events without consuming */
    if (CheckSystemEvents(mask, evt)) {
        return true;
    }

    NullEventNow(evt);
    return false;
}

/*
 * Proc_PostEvent - Process-aware post event to queue
 *
 * NOTE: This is a process-aware version that unblocks waiting processes.
 * It can be called in addition to the standard PostEvent.
 */
OSErr Proc_PostEventWithModifiers(EventMask what, UInt32 message, UInt16 modifiers) {
    EventRecord evt;

    /* A full queue gives up its oldest event (Inside Macintosh: Macintosh
     * Toolbox Essentials, 2-115). Refusing the new one let events a modal
     * loop does not ask for fill it and lock out every click after them. */
    if (gQueueCount >= EVENT_QUEUE_SIZE) {
        PROCESS_LOG_DEBUG("EventMgr: Queue full, discarding the oldest\n");
        gQueueHead = (gQueueHead + 1) % EVENT_QUEUE_SIZE;
        gQueueCount--;
    }

    /* Build event record */
    evt.what = what;
    evt.message = message;
    evt.when = TickCount();
    evt.where.h = 0;
    evt.where.v = 0;
    GetMouse(&evt.where);
    evt.modifiers = modifiers;

    /* Add to queue */
    CopyEventRecord(&gEventQueue[gQueueTail], &evt);
    gQueueTail = (gQueueTail + 1) % EVENT_QUEUE_SIZE;
    gQueueCount++;

    PROCESS_LOG_DEBUG("EventMgr: Posted event %ld msg=0x%08lx\n", (long)what, (unsigned long)message);

    /* Unblock any process waiting for this event */
    Proc_UnblockEvent(&evt);

    return noErr;
}

/*
 * Post with the modifier state that was in effect when the event happened.
 *
 * Reading the live modifiers here is wrong for keys. The keyboard IRQ has
 * already processed the whole chord by the time the input layer turns it into
 * events, so a Command-N that was pressed and released between two polls
 * reports no modifiers at all and the menu equivalent never fires. The input
 * layer knows the state at each keystroke and passes it in.
 */
OSErr Proc_PostEvent(EventMask what, UInt32 message) {
    return Proc_PostEventWithModifiers(what, message, GetModifiers());
}

/*
 * Proc_FlushEvents - Remove events from queue (process-aware version)
 */
static void Proc_FlushEvents(EventMask whichMask, EventMask stopMask) {
    UInt16 readIdx = gQueueHead;
    UInt16 writeIdx = gQueueHead;
    UInt16 count = gQueueCount;
    UInt16 keptCount = 0;
    Boolean reachedStop = false;

    PROCESS_LOG_DEBUG("EventMgr: Flushing events mask=0x%04lx stop=0x%04lx\n",
                  (unsigned long)whichMask, (unsigned long)stopMask);

    while (count > 0) {
        EventRecord* evt = &gEventQueue[readIdx];
        if (EventMatchesMask(evt, stopMask)) {
            reachedStop = true;
        }

        /* Compact retained events, including the stop event and everything after it. */
        if (reachedStop || !EventMatchesMask(evt, whichMask)) {
            if (writeIdx != readIdx) {
                CopyEventRecord(&gEventQueue[writeIdx], evt);
            }
            writeIdx = (writeIdx + 1) % EVENT_QUEUE_SIZE;
            keptCount++;
        }

        readIdx = (readIdx + 1) % EVENT_QUEUE_SIZE;
        count--;
    }

    gQueueTail = writeIdx;
    gQueueCount = keptCount;
}

/*
 * DequeueEvent - Remove matching event from queue
 * Preserve the relative order of all events left in the queue.
 */
static Boolean DequeueEvent(EventMask mask, EventRecord* evt) {
    UInt16 index;
    if (!FindQueuedEventIndex(mask, &index)) {
        return false;
    }

    CopyEventRecord(evt, &gEventQueue[index]);
    if (index == gQueueHead) {
        gQueueHead = (gQueueHead + 1) % EVENT_QUEUE_SIZE;
    } else {
        UInt16 next = (index + 1) % EVENT_QUEUE_SIZE;
        while (next != gQueueTail) {
            CopyEventRecord(&gEventQueue[index], &gEventQueue[next]);
            index = next;
            next = (next + 1) % EVENT_QUEUE_SIZE;
        }
        gQueueTail = index;
    }
    gQueueCount--;

    PROCESS_LOG_DEBUG("EventMgr: Dequeued event %d\n", evt->what);
    return true;
}

/* Return a dirty-window update event when the caller requests update events. */
static Boolean CheckSystemEvents(EventMask mask, EventRecord* evt) {
    /* Generate updates on demand so repeated polling cannot fill the event
     * queue with duplicate events for the same dirty window. */
    if (mask & updateMask) {
        WindowPtr needy = WM_FindWindowNeedingUpdate();
        if (needy) {
            evt->what = updateEvt;
            evt->message = (SInt32)(uintptr_t)needy;
            evt->when = TickCount();
            evt->modifiers = GetModifiers();
            GetMouse(&evt->where);
            return true;
        }
    }

    return false;
}

/*
 * GetModifiers - Get current keyboard modifiers
 */
static UInt16 GetModifiers(void) {
    /* Get actual keyboard modifier state from EventManager
     * - cmdKey (0x0100)
     * - shiftKey (0x0200)
     * - alphaLock (0x0400)
     * - optionKey (0x0800)
     * - controlKey (0x1000)
     * - rightShiftKey (0x2000)
     * - rightOptionKey (0x4000)
     * - rightControlKey (0x8000)
     */
    return GetCurrentModifiers();
}

/*
 * GetCurrentModifiers - Get current keyboard modifier state
 * Delegates to the keyboard driver's tracked modifier state.
 */
UInt16 GetCurrentModifiers(void) {
    return GetModifierState();
}

/*
 * Event queue management
 */
void Event_InitQueue(void) {
    gQueueHead = 0;
    gQueueTail = 0;
    gQueueCount = 0;
    memset(gEventQueue, 0, sizeof(gEventQueue));

    PROCESS_LOG_DEBUG("EventMgr: Event queue initialized\n");
}

UInt16 Event_QueueCount(void) {
    return gQueueCount;
}

void Event_DumpQueue(void) {
    UInt16 index = gQueueHead;
    UInt16 count = gQueueCount;
    UInt16 i = 0;

    PROCESS_LOG_DEBUG("\n=== Event Queue ===\n");
    PROCESS_LOG_DEBUG("Head=%d Tail=%d Count=%d\n",
                  gQueueHead, gQueueTail, gQueueCount);

    while (count > 0) {
        EventRecord* evt = &gEventQueue[index];
        const char* typeStr = "?";

        switch (evt->what) {
            case nullEvent: typeStr = "null"; break;
            case mouseDown: typeStr = "mDown"; break;
            case mouseUp: typeStr = "mUp"; break;
            case keyDown: typeStr = "kDown"; break;
            case keyUp: typeStr = "kUp"; break;
            case autoKey: typeStr = "auto"; break;
            case updateEvt: typeStr = "updt"; break;
            case diskEvt: typeStr = "disk"; break;
            case activateEvt: typeStr = "actv"; break;
            /* High-level events */
            case osEvt: typeStr = "os"; break;
            case kHighLevelEvent: typeStr = "hlev"; break;
        }

        PROCESS_LOG_DEBUG("[%2d] %-4s msg=0x%08lx time=%lu pos=(%d,%d)\n",
                     i, typeStr, (unsigned long)evt->message, (unsigned long)evt->when,
                     evt->where.h, evt->where.v);

        index = (index + 1) % EVENT_QUEUE_SIZE;
        count--;
        i++;
    }
    PROCESS_LOG_DEBUG("==================\n\n");
}

/* The Event Manager's public calls, routed to the process-aware versions */

Boolean GetNextEvent(EventMask mask, EventRecord* evt) {
    return Proc_GetNextEvent(mask, evt);
}

Boolean EventAvail(EventMask mask, EventRecord* evt) {
    return Proc_EventAvail(mask, evt);
}

OSErr PostEvent(EventMask what, UInt32 message) {
    return Proc_PostEvent(what, message);
}

OSErr PostEventWithModifiers(EventMask what, UInt32 message, UInt16 modifiers) {
    return Proc_PostEventWithModifiers(what, message, modifiers);
}

void FlushEvents(EventMask whichMask, EventMask stopMask) {
    Proc_FlushEvents(whichMask, stopMask);
}
