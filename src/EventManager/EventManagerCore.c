/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
/**
 * @file EventManagerCore.c
 * @brief Core Event Manager Implementation for System 7.1
 *
 * This file provides the core event queue management and fundamental
 * event processing that forms the foundation of the Mac OS Event Manager.
 * It maintains exact compatibility with System 7.1 event semantics.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>
#include <unistd.h>

#include "EventManager/EventManager.h"
#include "EventManager/EventStructs.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/KeyboardEvents.h"
#include "EventManager/SystemEvents.h"

#ifdef PLATFORM_REMOVED_WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__linux__)

#endif

/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* Event Manager global state */
static EventMgrGlobals g_eventGlobals = {0};
static Boolean g_eventMgrInitialized = false;

/* Event queue management */
static QHdr g_eventQueue = {0};
static EvQEl* g_eventBuffer = NULL;
static SInt16 g_eventBufferSize = 0;
static SInt16 g_eventBufferCount = 0;

/* Low memory globals (simulated) */
static UInt16 g_sysEvtMask = 0xFFEF;  /* All events except keyUp */
static UInt32 g_tickCount = 0;
static Point g_mousePos = {0, 0};
static UInt8 g_mouseButtonState = 0;
static KeyMap g_keyMapState = {0};
static UInt32 g_doubleTime = kDefaultDoubleClickTime;
static UInt32 g_caretTime = kDefaultCaretBlinkTime;

/* Timing */
static UInt32 g_systemStartTime = 0;
static UInt32 g_lastTickUpdate = 0;

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

static void UpdateTickCount(void);
static UInt32 GetSystemTime(void);
static EvQEl* AllocateEventElement(void);
static void FreeEventElement(EvQEl* element);
static void EnqueueEvent(EvQEl* element);
static EvQEl* DequeueEvent(void);
static EvQEl* FindEvent(SInt16 eventMask);
static void FillEventRecord(EventRecord* event);
static Boolean IsEventEnabled(SInt16 eventType);
static void ProcessAutoKey(void);

/*---------------------------------------------------------------------------
 * Platform-Specific Timing
 *---------------------------------------------------------------------------*/

/**
 * Get current system time in milliseconds
 */
static UInt32 GetSystemTime(void)
{
#ifdef PLATFORM_REMOVED_WIN32
    return GetTickCount();
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t time = mach_absolute_time();
    return (UInt32)((time * timebase.numer) / (timebase.denom * 1000000));
#elif defined(__linux__)
    #ifdef CLOCK_REALTIME
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (UInt32)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    #else
    // Fallback - just use a counter
    static UInt32 tick_counter = 0;
    return tick_counter++;
    #endif
#else
    // No platform-specific timer - use counter
    static UInt32 tick_counter = 0;
    return tick_counter++;
#endif
}

/**
 * Update tick count based on system time
 */
static void UpdateTickCount(void)
{
    UInt32 currentTime = GetSystemTime();
    if (g_systemStartTime == 0) {
        g_systemStartTime = currentTime;
        g_lastTickUpdate = currentTime;
    }

    /* Convert milliseconds to ticks (60 ticks per second) */
    UInt32 elapsedMs = currentTime - g_lastTickUpdate;
    UInt32 newTicks = (elapsedMs * 60) / 1000;

    if (newTicks > 0) {
        g_tickCount += newTicks;
        g_lastTickUpdate = currentTime;
        g_eventGlobals.Ticks = g_tickCount;
    }
}

/*---------------------------------------------------------------------------
 * Event Queue Management
 *---------------------------------------------------------------------------*/

/**
 * Allocate an event queue element
 */
static EvQEl* AllocateEventElement(void)
{
    if (!g_eventBuffer || g_eventBufferCount <= 0) {
        return NULL;
    }

    /* Find a free element (marked with qType = -1) */
    for (SInt16 i = 0; i < g_eventBufferSize; i++) {
        EvQEl* element = &g_eventBuffer[i];
        if (element->qType == -1) {
            memset(element, 0, sizeof(EvQEl));
            element->qType = 1; /* Mark as allocated */
            return element;
        }
    }

    /* No free elements - reuse oldest event */
    EvQEl* oldest = (EvQEl*)g_eventQueue.qHead;
    if (oldest) {
        /* Remove from queue */
        g_eventQueue.qHead = oldest->qLink;
        if (g_eventQueue.qTail == oldest) {
            g_eventQueue.qTail = NULL;
        }

        memset(oldest, 0, sizeof(EvQEl));
        oldest->qType = 1;
        return oldest;
    }

    return NULL;
}

/**
 * Free an event queue element
 */
static void FreeEventElement(EvQEl* element)
{
    if (element) {
        element->qType = -1; /* Mark as free */
        element->qLink = NULL;
    }
}

/**
 * Add event to queue
 */
static void EnqueueEvent(EvQEl* element)
{
    if (!element) return;

    element->qLink = NULL;

    if (g_eventQueue.qTail) {
        ((EvQEl*)g_eventQueue.qTail)->qLink = element;
        g_eventQueue.qTail = element;
    } else {
        g_eventQueue.qHead = element;
        g_eventQueue.qTail = element;
    }
}

/**
 * Remove event from queue
 */
static EvQEl* DequeueEvent(void)
{
    EvQEl* element = (EvQEl*)g_eventQueue.qHead;
    if (element) {
        g_eventQueue.qHead = element->qLink;
        if (g_eventQueue.qTail == element) {
            g_eventQueue.qTail = NULL;
        }
        element->qLink = NULL;
    }
    return element;
}

/**
 * Find event matching mask (without removing from queue)
 */
static EvQEl* FindEvent(SInt16 eventMask)
{
    EvQEl* current = (EvQEl*)g_eventQueue.qHead;

    while (current) {
        if (eventMask == everyEvent || (eventMask & (1 << current->evtQWhat))) {
            return current;
        }
        current = current->qLink;
    }

    return NULL;
}

/**
 * Fill standard event record fields
 */
static void FillEventRecord(EventRecord* event)
{
    if (!event) return;

    UpdateTickCount();

    event->when = g_tickCount;
    event->where = g_mousePos;

    /* Build modifier flags from keyboard state */
    event->modifiers = 0;

    /* Mouse button state */
    if (g_mouseButtonState & 1) {
        event->modifiers |= btnState;
    }

    /* Check modifier keys in keymap */
    if (g_keyMapState[1] & (1 << (kScanCommand - 32))) {
        event->modifiers |= cmdKey;
    }
    if (g_keyMapState[1] & (1 << (kScanShift - 32))) {
        event->modifiers |= shiftKey;
    }
    if (g_keyMapState[1] & (1 << (kScanCapsLock - 32))) {
        event->modifiers |= alphaLock;
    }
    if (g_keyMapState[1] & (1 << (kScanOption - 32))) {
        event->modifiers |= optionKey;
    }
    if (g_keyMapState[1] & (1 << (kScanControl - 32))) {
        event->modifiers |= controlKey;
    }
}

/**
 * Check if event type is enabled
 */
static Boolean IsEventEnabled(SInt16 eventType)
{
    return (g_sysEvtMask & (1 << eventType)) != 0;
}

/**
 * Process auto-key generation
 */
static void ProcessAutoKey(void)
{
    if (!g_eventGlobals.KeyLast || !IsEventEnabled(autoKey)) {
        return;
    }

    UpdateTickCount();

    UInt32 elapsed = g_tickCount - g_eventGlobals.KeyTime;
    if (elapsed < g_eventGlobals.keyState.repeatDelay) {
        return;
    }

    UInt32 repeatElapsed = g_tickCount - g_eventGlobals.KeyRepTime;
    if (repeatElapsed < g_eventGlobals.keyState.repeatRate) {
        return;
    }

    /* Generate auto-key event */
    PostEvent(autoKey, g_eventGlobals.KeyLast);
    g_eventGlobals.KeyRepTime = g_tickCount;
}

/*---------------------------------------------------------------------------
 * Core Event Manager API Implementation
 *---------------------------------------------------------------------------*/

/**
 * Initialize the Event Manager
 */
SInt16 InitEvents(SInt16 numEvents)
{
    if (g_eventMgrInitialized) {
        return noErr;
    }

    /* Initialize timing */
    g_systemStartTime = GetSystemTime();
    g_tickCount = 0;
    g_lastTickUpdate = g_systemStartTime;

    /* Allocate event buffer */
    if (numEvents <= 0) {
        numEvents = kDefaultEventQueueSize;
    }
    if (numEvents > kMaxEventQueueSize) {
        numEvents = kMaxEventQueueSize;
    }

    g_eventBuffer = (EvQEl*)calloc(numEvents, sizeof(EvQEl));
    if (!g_eventBuffer) {
        return -1; /* Memory error */
    }

    g_eventBufferSize = numEvents;
    g_eventBufferCount = numEvents;

    /* Mark all elements as free */
    for (SInt16 i = 0; i < numEvents; i++) {
        g_eventBuffer[i].qType = -1;
    }

    /* Initialize queue */
    memset(&g_eventQueue, 0, sizeof(QHdr));

    /* Initialize globals */
    memset(&g_eventGlobals, 0, sizeof(EventMgrGlobals));
    g_eventGlobals.SysEvtMask = g_sysEvtMask;
    g_eventGlobals.Ticks = g_tickCount;
    g_eventGlobals.Mouse = g_mousePos;
    g_eventGlobals.MBState = g_mouseButtonState;
    g_eventGlobals.DoubleTime = g_doubleTime;
    g_eventGlobals.CaretTime = g_caretTime;
    g_eventGlobals.KeyThresh = kDefaultKeyRepeatDelay;
    g_eventGlobals.KeyRepThresh = kDefaultKeyRepeatRate;
    g_eventGlobals.keyState.repeatDelay = kDefaultKeyRepeatDelay;
    g_eventGlobals.keyState.repeatRate = kDefaultKeyRepeatRate;
    g_eventGlobals.keyState.autoRepeatEnabled = true;
    g_eventGlobals.initialized = true;

    /* Initialize subsystems */
    InitMouseEvents();
    InitKeyboardEvents();
    InitSystemEvents();

    g_eventMgrInitialized = true;
    return noErr;
}

#if 0  /* DISABLED - GetNextEvent now provided by event_manager.c */
/**
 * Get next event matching mask
 */
Boolean GetNextEvent_DISABLED(SInt16 eventMask, EventRecord* theEvent)
{
    if (!theEvent || !g_eventMgrInitialized) {
        return false;
    }

    UpdateTickCount();
    ProcessAutoKey();

    /* Look for matching event in queue */
    EvQEl* element = FindEvent(eventMask);
    if (element) {
        /* Copy event data */
        theEvent->what = element->evtQWhat;
        theEvent->message = element->evtQMessage;
        theEvent->when = element->evtQWhen;
        theEvent->where = element->evtQWhere;
        theEvent->modifiers = element->evtQModifiers;

        /* Remove from queue */
        if (element == (EvQEl*)g_eventQueue.qHead) {
            DequeueEvent();
        } else {
            /* Remove from middle of queue */
            EvQEl* prev = (EvQEl*)g_eventQueue.qHead;
            while (prev && prev->qLink != element) {
                prev = prev->qLink;
            }
            if (prev) {
                prev->qLink = element->qLink;
                if (g_eventQueue.qTail == element) {
                    g_eventQueue.qTail = prev;
                }
            }
        }

        FreeEventElement(element);
        return true;
    }

    /* No event found - return null event */
    theEvent->what = nullEvent;
    theEvent->message = 0;
    FillEventRecord(theEvent);
    return false;
}
#endif /* DISABLED GetNextEvent */

#if 0  /* DISABLED - EventAvail now provided by event_manager.c */
/**
 * Check if event is available
 */
Boolean EventAvail_DISABLED(SInt16 eventMask, EventRecord* theEvent)
{
    if (!theEvent || !g_eventMgrInitialized) {
        return false;
    }

    UpdateTickCount();
    ProcessAutoKey();

    /* Look for matching event in queue */
    EvQEl* element = FindEvent(eventMask);
    if (element) {
        /* Copy event data without removing from queue */
        theEvent->what = element->evtQWhat;
        theEvent->message = element->evtQMessage;
        theEvent->when = element->evtQWhen;
        theEvent->where = element->evtQWhere;
        theEvent->modifiers = element->evtQModifiers;
        return true;
    }

    /* No event found - return null event */
    theEvent->what = nullEvent;
    theEvent->message = 0;
    FillEventRecord(theEvent);
    return false;
}
#endif /* DISABLED EventAvail */

#if 0  /* DISABLED - PostEvent now provided by event_manager.c */
/**
 * Post an event to the queue
 */
SInt16 PostEvent_DISABLED(SInt16 eventNum, SInt32 eventMsg)
{
    if (!g_eventMgrInitialized) {
        return evtNotEnb;
    }

    /* Check if event type is enabled */
    if (!IsEventEnabled(eventNum)) {
        return evtNotEnb;
    }

    /* Allocate event element */
    EvQEl* element = AllocateEventElement();
    if (!element) {
        return queueFull;
    }

    /* Fill event data */
    element->evtQWhat = eventNum;
    element->evtQMessage = eventMsg;
    element->evtQWhen = g_tickCount;
    element->evtQWhere = g_mousePos;

    /* Build modifier flags */
    element->evtQModifiers = 0;
    if (g_mouseButtonState & 1) {
        element->evtQModifiers |= btnState;
    }

    /* Add modifier key states */
    if (g_keyMapState[1] & (1 << (kScanCommand - 32))) {
        element->evtQModifiers |= cmdKey;
    }
    if (g_keyMapState[1] & (1 << (kScanShift - 32))) {
        element->evtQModifiers |= shiftKey;
    }
    if (g_keyMapState[1] & (1 << (kScanCapsLock - 32))) {
        element->evtQModifiers |= alphaLock;
    }
    if (g_keyMapState[1] & (1 << (kScanOption - 32))) {
        element->evtQModifiers |= optionKey;
    }
    if (g_keyMapState[1] & (1 << (kScanControl - 32))) {
        element->evtQModifiers |= controlKey;
    }

    /* Add to queue */
    EnqueueEvent(element);

    return noErr;
}
#endif /* DISABLED PostEvent */

/**
 * Post event with queue element return
 */
SInt16 PPostEvent(SInt16 eventCode, SInt32 eventMsg, EvQEl** qEl)
{
    SInt16 result = PostEvent(eventCode, eventMsg);
    if (result == noErr && qEl) {
        *qEl = (EvQEl*)g_eventQueue.qTail;
    }
    return result;
}

/**
 * OS-level event checking
 */
Boolean OSEventAvail(SInt16 mask, EventRecord* theEvent)
{
    return EventAvail(mask, theEvent);
}

/**
 * Get OS event (removes from queue)
 */
Boolean GetOSEvent(SInt16 mask, EventRecord* theEvent)
{
    return GetNextEvent(mask, theEvent);
}

/**
 * Flush events from queue
 */
void FlushEvents(SInt16 whichMask, SInt16 stopMask)
{
    if (!g_eventMgrInitialized) {
        return;
    }

    EvQEl* current = (EvQEl*)g_eventQueue.qHead;
    EvQEl* prev = NULL;

    while (current) {
        EvQEl* next = current->qLink;

        /* Check if we should stop on this event */
        if (stopMask & (1 << current->evtQWhat)) {
            break;
        }

        /* Check if we should remove this event */
        if (whichMask & (1 << current->evtQWhat)) {
            /* Remove from queue */
            if (prev) {
                prev->qLink = next;
            } else {
                g_eventQueue.qHead = next;
            }

            if (g_eventQueue.qTail == current) {
                g_eventQueue.qTail = prev;
            }

            FreeEventElement(current);
        } else {
            prev = current;
        }

        current = next;
    }
}

#if 0  /* DISABLED - WaitNextEvent now provided by event_manager.c */
/**
 * Wait for next event with idle processing
 */
Boolean WaitNextEvent_DISABLED(SInt16 eventMask, EventRecord* theEvent,
                   UInt32 sleep, RgnHandle mouseRgn)
{
    if (!theEvent || !g_eventMgrInitialized) {
        return false;
    }

    UInt32 startTime = g_tickCount;
    UInt32 endTime = startTime + sleep;

    /* Poll for events until timeout or event found */
    while (sleep == 0 || g_tickCount < endTime) {
        UpdateTickCount();

        /* Check for available event */
        if (EventAvail(eventMask, theEvent)) {
            if (theEvent->what != nullEvent) {
                /* Remove the event from queue */
                GetNextEvent(eventMask, theEvent);
                return true;
            }
        }

        /* Process system events */
        ProcessSystemEvents();

        /* Check for mouse-moved events if region specified */
        if (mouseRgn) {
            /* TODO: Implement mouse region checking */
        }

        /* Brief sleep to avoid busy waiting */
        #ifdef PLATFORM_REMOVED_WIN32
        Sleep(1);
        #else
        /* Brief delay - would use usleep(1000) in user space */
        #endif
    }

    /* Timeout - return null event */
    theEvent->what = nullEvent;
    theEvent->message = 0;
    FillEventRecord(theEvent);
    return false;
}
#endif /* DISABLED WaitNextEvent */

/*---------------------------------------------------------------------------
 * Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current mouse position
 */
void GetMouse(Point* mouseLoc)
{
    if (mouseLoc) {
        UpdateTickCount();
        *mouseLoc = g_mousePos;
    }
}

/* Button is implemented in PS2Controller.c */
/* StillDown is implemented in control_stubs.c */

/* WaitMouseUp is implemented in MouseEvents.c */

/*---------------------------------------------------------------------------
 * Keyboard Event API
 *---------------------------------------------------------------------------*/

/* GetKeys is implemented in KeyboardEvents.c */

/* KeyTranslate is implemented in KeyboardEvents.c */

/*---------------------------------------------------------------------------
 * Timing API
 *---------------------------------------------------------------------------*/

/* TickCount is implemented in sys71_stubs.c */

/**
 * Get double-click time
 */
UInt32 GetDblTime(void)
{
    return g_doubleTime;
}

/**
 * Get caret time
 */
UInt32 GetCaretTime(void)
{
    return g_caretTime;
}

/**
 * Set system event mask
 */
void SetEventMask(SInt16 mask)
{
    g_sysEvtMask = mask;
    g_eventGlobals.SysEvtMask = mask;
}

/*---------------------------------------------------------------------------
 * Extended API
 *---------------------------------------------------------------------------*/

/**
 * Set key repeat thresholds
 */
void SetKeyRepeat(UInt16 delay, UInt16 rate)
{
    g_eventGlobals.KeyThresh = delay;
    g_eventGlobals.KeyRepThresh = rate;
    g_eventGlobals.keyState.repeatDelay = delay;
    g_eventGlobals.keyState.repeatRate = rate;
}

/**
 * Get global event manager state
 */
EventMgrGlobals* GetEventMgrGlobals(void)
{
    return &g_eventGlobals;
}

/**
 * Generate system event
 */
void GenerateSystemEvent(SInt16 eventType, SInt32 message, Point where, SInt16 modifiers)
{
    EvQEl* element = AllocateEventElement();
    if (element) {
        element->evtQWhat = eventType;
        element->evtQMessage = message;
        element->evtQWhen = g_tickCount;
        element->evtQWhere = where;
        element->evtQModifiers = modifiers;
        EnqueueEvent(element);
    }
}

/**
 * Process null event
 */
void ProcessNullEvent(void)
{
    UpdateTickCount();
    ProcessAutoKey();
    ProcessSystemEvents();
}

/**
 * Check for command-period abort
 */
Boolean CheckAbort(void)
{
    /* Check if Command+Period is pressed */
    return (g_keyMapState[1] & (1 << (kScanCommand - 32))) &&
           (g_keyMapState[0] & (1 << 0x2F)); /* Period key */
}

/*---------------------------------------------------------------------------
 * Public State Access
 *---------------------------------------------------------------------------*/

/**
 * Update mouse state (called by mouse input system)
 */
void UpdateMouseState(Point newPos, UInt8 buttonState)
{
    g_mousePos = newPos;
    g_mouseButtonState = buttonState;
    g_eventGlobals.Mouse = newPos;
    g_eventGlobals.MBState = buttonState;
}

/**
 * Update keyboard state (called by keyboard input system)
 */
void UpdateKeyboardState(const KeyMap newKeyMap)
{
    memcpy(g_keyMapState, newKeyMap, sizeof(KeyMap));
    memcpy(g_eventGlobals.KeyMapState, newKeyMap, sizeof(KeyMap));
}

/**
 * Set timing parameters
 */
void SetTimingParameters(UInt32 doubleTime, UInt32 caretTime)
{
    g_doubleTime = doubleTime;
    g_caretTime = caretTime;
    g_eventGlobals.DoubleTime = doubleTime;
    g_eventGlobals.CaretTime = caretTime;
}
