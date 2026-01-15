/*
 * EventIntegration.c - Event Queue and Process Integration
 *
 * Implements event queue management and process-aware event APIs.
 * Provides GetNextEvent, EventAvail, and PostEvent with process
 * unblocking capabilities.
 */

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "ProcessMgr/ProcessMgr.h"
#include "ProcessMgr/ProcessLogging.h"

/* Event queue - ring buffer */
#define EVENT_QUEUE_SIZE 64
static EventRecord gEventQueue[EVENT_QUEUE_SIZE];
static UInt16 gQueueHead = 0;
static UInt16 gQueueTail = 0;
static UInt16 gQueueCount = 0;

/* External functions */
extern void Proc_UnblockEvent(EventRecord* evt);
extern UInt32 TickCount(void);
extern void GetMouse(Point* pt);
extern Boolean Button(void);

/* String function from System71StdLib */
extern void* memset(void* s, int c, size_t n);

/* Forward declarations */
static UInt16 GetModifiers(void);
static Boolean DequeueEvent(EventMask mask, EventRecord* evt);
static Boolean CheckSystemEvents(EventMask mask, EventRecord* evt);

/*
 * Proc_GetNextEvent - Process-aware get next event matching mask
 *
 * This is THE cooperative multitasking point. When apps call this,
 * they're saying "I'm idle, let others run"
 *
 * NOTE: This is a process-aware version that integrates with the scheduler.
 * The canonical GetNextEvent is in EventManager/event_manager.c
 */
Boolean Proc_GetNextEvent(EventMask mask, EventRecord* evt) {
    if (!evt) return false;

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

/*
 * Proc_EventAvail - Process-aware check if event available without removing
 *
 * NOTE: This is a process-aware version. The canonical EventAvail
 * is in EventManager/event_manager.c
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
            extern void* memcpy(void* dest, const void* src, size_t n);
            memcpy(evt, qEvt, sizeof(EventRecord));
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
OSErr Proc_PostEvent(EventMask what, UInt32 message) {
    EventRecord evt;

    if (gQueueCount >= EVENT_QUEUE_SIZE) {
        PROCESS_LOG_DEBUG("EventMgr: Queue full, dropping event %d\n", what);
        return evtNotEnb;  /* Event queue full */
    }

    /* Build event record */
    evt.what = what;
    evt.message = message;
    evt.when = TickCount();
    evt.where.h = 0;
    evt.where.v = 0;
    GetMouse(&evt.where);
    evt.modifiers = GetModifiers();

    /* Add to queue - use memcpy to avoid struct assignment on ARM64 */
    extern void* memcpy(void* dest, const void* src, size_t n);
    memcpy(&gEventQueue[gQueueTail], &evt, sizeof(EventRecord));
    gQueueTail = (gQueueTail + 1) % EVENT_QUEUE_SIZE;
    gQueueCount++;

    PROCESS_LOG_DEBUG("EventMgr: Posted event %d msg=0x%08x\n", what, message);

    /* Unblock any process waiting for this event */
    Proc_UnblockEvent(&evt);

    return noErr;
}

/*
 * Proc_FlushEvents - Remove events from queue (process-aware version)
 */
static void Proc_FlushEvents(EventMask whichMask, EventMask stopMask) {
    UInt16 readIdx = gQueueHead;
    UInt16 writeIdx = gQueueHead;
    UInt16 count = gQueueCount;

    PROCESS_LOG_DEBUG("EventMgr: Flushing events mask=0x%04x stop=0x%04x\n",
                  whichMask, stopMask);

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
                /* Use memcpy to avoid struct assignment on ARM64 */
                extern void* memcpy(void* dest, const void* src, size_t n);
                memcpy(&gEventQueue[writeIdx], evt, sizeof(EventRecord));
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
            /* Use memcpy to avoid struct assignment on ARM64 */
            extern void* memcpy(void* dest, const void* src, size_t n);
            memcpy(evt, headEvt, sizeof(EventRecord));
            gQueueHead = (gQueueHead + 1) % EVENT_QUEUE_SIZE;
            gQueueCount--;

            PROCESS_LOG_DEBUG("EventMgr: Dequeued event %d\n", evt->what);
            return true;
        }

        /* No match - rotate this event to back */
        /* Use memcpy to avoid struct assignment on ARM64 */
        extern void* memcpy(void* dest, const void* src, size_t n);
        memcpy(&gEventQueue[gQueueTail], headEvt, sizeof(EventRecord));
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

    /* Generate null/idle events when no other events are pending */
    /* Null events allow applications to perform background tasks */
    if (mask & nullEvent) {
        extern void GetMouse(Point* mouseLoc);
        extern UInt32 TickCount(void);

        /* Fill in null event */
        evt->what = nullEvent;
        evt->message = 0;
        evt->when = TickCount();
        evt->modifiers = GetModifiers();

        /* Get current mouse position */
        Point mousePt;
        GetMouse(&mousePt);
        evt->where = mousePt;

        return true;
    }

    return false;  /* No events generated */
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
 * Stub implementation for bare-metal environment
 */
UInt16 GetCurrentModifiers(void) {
    /* TODO: Integrate with actual keyboard driver when available
     * For now, return no modifiers (0) as we don't have keyboard input in bare-metal */
    return 0;
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

        PROCESS_LOG_DEBUG("[%2d] %-4s msg=0x%08x time=%u pos=(%d,%d)\n",
                     i, typeStr, evt->message, evt->when,
                     evt->where.h, evt->where.v);

        index = (index + 1) % EVENT_QUEUE_SIZE;
        count--;
        i++;
    }
    PROCESS_LOG_DEBUG("==================\n\n");
}

/*
 * Route canonical Event Manager APIs to process-aware versions
 * when ENABLE_PROCESS_COOP is defined
 */
#ifdef ENABLE_PROCESS_COOP

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

/* Override the canonical FlushEvents */
void FlushEvents(EventMask whichMask, EventMask stopMask) {
    Proc_FlushEvents(whichMask, stopMask);
}

#endif /* ENABLE_PROCESS_COOP */