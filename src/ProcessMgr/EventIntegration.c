/*
 * EventIntegration.c - Event Queue and Process Integration
 *
 * Implements event queue management and process-aware event APIs.
 * Provides GetNextEvent, EventAvail, and PostEvent with process
 * unblocking capabilities.
 */

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventManager.h"   /* PostEventWithModifiers */
#include "EventManager/KeyboardEvents.h"
#include "ProcessMgr/ProcessMgr.h"
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
 * execute (see REDRAW-004).
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
        extern void UpdateCursorDisplay(void);
        ProcessModernInput();
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

    /* No event - generate null event */
    evt->what = nullEvent;
    evt->message = 0;
    evt->when = TickCount();
    evt->where.h = 0;
    evt->where.v = 0;
    GetMouse(&evt->where);
    evt->modifiers = GetModifiers();

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
    ProcessModernInput();
    if (DequeueEvent((EventMask)(UInt16)mask, evt)) {
        Proc_UnblockEvent(evt);
        return true;
    }
    NullEventNow(evt);
    return false;
}

Boolean OSEventAvail(SInt16 mask, EventRecord* evt) {
    if (!evt) return false;
    UInt16 index = gQueueHead;
    for (UInt16 count = gQueueCount; count > 0; count--) {
        if ((1 << gEventQueue[index].what) & (UInt16)mask) {
            CopyEventRecord(evt, &gEventQueue[index]);
            return true;
        }
        index = (index + 1) % EVENT_QUEUE_SIZE;
    }
    NullEventNow(evt);
    return false;
}

/*
 * Proc_EventAvail - Process-aware check if event available without removing
 *
 * The public EventAvail entry point below routes here so availability checks
 * use the same process-aware event queue as GetNextEvent.
 */
Boolean Proc_EventAvail(EventMask mask, EventRecord* evt) {
    UInt16 index;
    UInt16 count;

    if (!evt) return false;

    /* Scan queue for matching event */
    index = gQueueHead;
    count = gQueueCount;

    while (count > 0) {
        EventRecord* qEvt = &gEventQueue[index];

        if ((1 << qEvt->what) & mask) {
            /* Found matching event - copy but don't remove */
            /* Use memcpy to avoid struct assignment on ARM64 */
            CopyEventRecord(evt, qEvt);
            return true;
        }

        index = (index + 1) % EVENT_QUEUE_SIZE;
        count--;
    }

    /* Check system events without consuming */
    if (CheckSystemEvents(mask, evt)) {
        return true;
    }

    /* No event available */
    evt->what = nullEvent;
    evt->message = 0;
    evt->when = TickCount();
    evt->where.h = 0;
    evt->where.v = 0;
    GetMouse(&evt->where);
    evt->modifiers = GetModifiers();

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

    /* Add to queue - use memcpy to avoid struct assignment on ARM64 */
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

    PROCESS_LOG_DEBUG("EventMgr: Flushing events mask=0x%04lx stop=0x%04lx\n",
                  (unsigned long)whichMask, (unsigned long)stopMask);

    while (count > 0) {
        EventRecord* evt = &gEventQueue[readIdx];
        EventMask evtBit = (1 << evt->what);

        /* Stop if we hit stop event */
        if (evtBit & stopMask) {
            break;
        }

        /* Keep event if not in flush mask */
        if (!(evtBit & whichMask)) {
            if (writeIdx != readIdx) {
                CopyEventRecord(&gEventQueue[writeIdx], evt);
            }
            writeIdx = (writeIdx + 1) % EVENT_QUEUE_SIZE;
        } else {
            gQueueCount--;  /* Removing this event */
        }

        readIdx = (readIdx + 1) % EVENT_QUEUE_SIZE;
        count--;
    }

    /* Update tail if we removed events */
    if (writeIdx != readIdx) {
        gQueueTail = writeIdx;
    }
}

/*
 * DequeueEvent - Remove matching event from queue
 * Strategy: Only pop from head - rotate non-matches to back
 */
static Boolean DequeueEvent(EventMask mask, EventRecord* evt) {
    UInt16 rotations = 0;

    /* Rotate queue until matching event at head or full rotation */
    while (rotations < gQueueCount) {
        if (gQueueCount == 0) {
            return false;
        }

        EventRecord* headEvt = &gEventQueue[gQueueHead];

        /* Check if head matches mask */
        if ((1 << headEvt->what) & mask) {
            /* Found match at head - dequeue it */
            CopyEventRecord(evt, headEvt);
            gQueueHead = (gQueueHead + 1) % EVENT_QUEUE_SIZE;
            gQueueCount--;

            PROCESS_LOG_DEBUG("EventMgr: Dequeued event %d\n", evt->what);
            return true;
        }

        /* No match - rotate this event to back */
        CopyEventRecord(&gEventQueue[gQueueTail], headEvt);
        gQueueHead = (gQueueHead + 1) % EVENT_QUEUE_SIZE;
        gQueueTail = (gQueueTail + 1) % EVENT_QUEUE_SIZE;
        rotations++;
    }

    return false;
}

/*
 * CheckSystemEvents - Check for system-generated events
 *
 * NOTE: Mouse events are now generated by ModernInput.c (ProcessModernInput).
 * This function should NOT synthesize mouse events to avoid duplicates.
 * Only generate events when the queue is empty AND ModernInput hasn't run.
 */
static Boolean CheckSystemEvents(EventMask mask, EventRecord* evt) {
    /* DO NOT generate mouse events here - ModernInput is the authoritative source.
     * Generating events here would create duplicates and break click counting.
     *
     * This function can be used for:
     * - Idle/null events
     * - Timeout events
     * - System notifications
     *
     * But NOT for mouse/keyboard hardware polling.
     */

    /* Synthesise an update event for a window with a dirty updateRgn.
     *
     * Update events were once generated only in a second GetNextEvent, in
     * EventManager/event_manager.c, that never linked - so none were generated
     * at all.
     *
     * Synthesising rather than PostEvent()ing matters: GetNextEvent is called
     * continuously and PostEvent does not deduplicate, so posting would fill
     * the 32-entry queue in microseconds and every subsequent mouse and key
     * event would be rejected as queueFull. Generating on demand can never
     * accumulate, and it matches how Classic Mac OS reports update events.
     *
     * Checked before the null event below, because a null event is returned
     * unconditionally whenever nullEvent is in the mask and would starve the
     * update forever. */
    if (mask & updateMask) {
        extern WindowPtr WM_FindWindowNeedingUpdate(void);
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

    /* Nothing pending: a null event, and false (Inside Macintosh: Toolbox
     * Essentials, 2-80). The caller does its idle work on it. This used to
     * test mask & nullEvent - and nullEvent is 0, so it never ran and no
     * null event was ever seen: no blinking caret, no ticking clock. */
    {
        evt->what = nullEvent;
        evt->message = 0;
        evt->when = TickCount();
        evt->modifiers = GetModifiers();
        GetMouse(&evt->where);
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

/* Override the canonical GetNextEvent */
Boolean GetNextEvent(EventMask mask, EventRecord* evt) {
    return Proc_GetNextEvent(mask, evt);
}

/* Override the canonical EventAvail */
Boolean EventAvail(EventMask mask, EventRecord* evt) {
    return Proc_EventAvail(mask, evt);
}

/* Override the canonical PostEvent */
OSErr PostEvent(EventMask what, UInt32 message) {
    return Proc_PostEvent(what, message);
}

OSErr PostEventWithModifiers(EventMask what, UInt32 message, UInt16 modifiers) {
    return Proc_PostEventWithModifiers(what, message, modifiers);
}

/* Override the canonical FlushEvents */
void FlushEvents(EventMask whichMask, EventMask stopMask) {
    Proc_FlushEvents(whichMask, stopMask);
}
