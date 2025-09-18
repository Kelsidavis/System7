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

#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/KeyboardEvents.h"
#include "EventManager/SystemEvents.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__linux__)
#include <time.h>
#include <sys/time.h>
#endif

/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* Event Manager global state */
static EventMgrGlobals g_eventGlobals = {0};
static bool g_eventMgrInitialized = false;

/* Event queue management */
static QHdr g_eventQueue = {0};
static EvQEl* g_eventBuffer = NULL;
static int16_t g_eventBufferSize = 0;
static int16_t g_eventBufferCount = 0;

/* Low memory globals (simulated) */
static uint16_t g_sysEvtMask = 0xFFEF;  /* All events except keyUp */
static uint32_t g_tickCount = 0;
static Point g_mousePos = {0, 0};
static uint8_t g_mouseButtonState = 0;
static KeyMap g_keyMapState = {0};
static uint32_t g_doubleTime = kDefaultDoubleClickTime;
static uint32_t g_caretTime = kDefaultCaretBlinkTime;

/* Timing */
static uint32_t g_systemStartTime = 0;
static uint32_t g_lastTickUpdate = 0;

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

static void UpdateTickCount(void);
static uint32_t GetSystemTime(void);
static EvQEl* AllocateEventElement(void);
static void FreeEventElement(EvQEl* element);
static void EnqueueEvent(EvQEl* element);
static EvQEl* DequeueEvent(void);
static EvQEl* FindEvent(int16_t eventMask);
static void FillEventRecord(EventRecord* event);
static bool IsEventEnabled(int16_t eventType);
static void ProcessAutoKey(void);

/*---------------------------------------------------------------------------
 * Platform-Specific Timing
 *---------------------------------------------------------------------------*/

/**
 * Get current system time in milliseconds
 */
static uint32_t GetSystemTime(void)
{
#ifdef _WIN32
    return GetTickCount();
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t time = mach_absolute_time();
    return (uint32_t)((time * timebase.numer) / (timebase.denom * 1000000));
#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    return (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
#endif
}

/**
 * Update tick count based on system time
 */
static void UpdateTickCount(void)
{
    uint32_t currentTime = GetSystemTime();
    if (g_systemStartTime == 0) {
        g_systemStartTime = currentTime;
        g_lastTickUpdate = currentTime;
    }

    /* Convert milliseconds to ticks (60 ticks per second) */
    uint32_t elapsedMs = currentTime - g_lastTickUpdate;
    uint32_t newTicks = (elapsedMs * 60) / 1000;

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
    for (int16_t i = 0; i < g_eventBufferSize; i++) {
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
static EvQEl* FindEvent(int16_t eventMask)
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
static bool IsEventEnabled(int16_t eventType)
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

    uint32_t elapsed = g_tickCount - g_eventGlobals.KeyTime;
    if (elapsed < g_eventGlobals.keyState.repeatDelay) {
        return;
    }

    uint32_t repeatElapsed = g_tickCount - g_eventGlobals.KeyRepTime;
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
int16_t InitEvents(int16_t numEvents)
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
        return noMemErr;
    }

    g_eventBufferSize = numEvents;
    g_eventBufferCount = numEvents;

    /* Mark all elements as free */
    for (int16_t i = 0; i < numEvents; i++) {
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

/**
 * Get next event matching mask
 */
bool GetNextEvent(int16_t eventMask, EventRecord* theEvent)
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

/**
 * Check if event is available
 */
bool EventAvail(int16_t eventMask, EventRecord* theEvent)
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

/**
 * Post an event to the queue
 */
int16_t PostEvent(int16_t eventNum, int32_t eventMsg)
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

/**
 * Post event with queue element return
 */
int16_t PPostEvent(int16_t eventCode, int32_t eventMsg, EvQEl** qEl)
{
    int16_t result = PostEvent(eventCode, eventMsg);
    if (result == noErr && qEl) {
        *qEl = (EvQEl*)g_eventQueue.qTail;
    }
    return result;
}

/**
 * OS-level event checking
 */
bool OSEventAvail(int16_t mask, EventRecord* theEvent)
{
    return EventAvail(mask, theEvent);
}

/**
 * Get OS event (removes from queue)
 */
bool GetOSEvent(int16_t mask, EventRecord* theEvent)
{
    return GetNextEvent(mask, theEvent);
}

/**
 * Flush events from queue
 */
void FlushEvents(int16_t whichMask, int16_t stopMask)
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

/**
 * Wait for next event with idle processing
 */
bool WaitNextEvent(int16_t eventMask, EventRecord* theEvent,
                   uint32_t sleep, RgnHandle mouseRgn)
{
    if (!theEvent || !g_eventMgrInitialized) {
        return false;
    }

    uint32_t startTime = g_tickCount;
    uint32_t endTime = startTime + sleep;

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
        #ifdef _WIN32
        Sleep(1);
        #else
        usleep(1000);
        #endif
    }

    /* Timeout - return null event */
    theEvent->what = nullEvent;
    theEvent->message = 0;
    FillEventRecord(theEvent);
    return false;
}

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

/**
 * Check if mouse button is down
 */
bool Button(void)
{
    return (g_mouseButtonState & 1) != 0;
}

/**
 * Check if mouse still down
 */
bool StillDown(void)
{
    /* TODO: Track button state changes */
    return Button();
}

/**
 * Wait for mouse button release
 */
bool WaitMouseUp(void)
{
    while (Button()) {
        UpdateTickCount();
        /* Brief sleep */
        #ifdef _WIN32
        Sleep(1);
        #else
        usleep(1000);
        #endif
    }
    return true;
}

/*---------------------------------------------------------------------------
 * Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current keyboard state
 */
void GetKeys(KeyMap theKeys)
{
    if (theKeys) {
        memcpy(theKeys, g_keyMapState, sizeof(KeyMap));
    }
}

/**
 * Key translation (simplified)
 */
int32_t KeyTranslate(const void* transData, uint16_t keycode, uint32_t* state)
{
    /* TODO: Implement full KCHR resource translation */
    /* For now, return simple ASCII mapping */

    uint16_t scanCode = keycode & 0xFF;
    uint16_t modifiers = (keycode >> 8) & 0xFF;

    /* Simple ASCII mapping for common keys */
    if (scanCode >= 0x00 && scanCode <= 0x0B) { /* 1234567890-= */
        const char keys[] = "1234567890-=";
        return keys[scanCode];
    }
    if (scanCode >= 0x0C && scanCode <= 0x19) { /* qwertyuiop[] */
        const char keys[] = "qwertyuiop[]";
        return keys[scanCode - 0x0C];
    }
    if (scanCode >= 0x1C && scanCode <= 0x28) { /* asdfghjkl;' */
        const char keys[] = "asdfghjkl;'";
        return keys[scanCode - 0x1C];
    }
    if (scanCode >= 0x2A && scanCode <= 0x35) { /* zxcvbnm,./ */
        const char keys[] = "zxcvbnm,./";
        return keys[scanCode - 0x2A];
    }

    /* Special keys */
    switch (scanCode) {
        case 0x31: return ' ';  /* Space */
        case 0x24: return '\r'; /* Return */
        case 0x30: return '\t'; /* Tab */
        case 0x33: return '\b'; /* Backspace */
        case 0x35: return 0x1B; /* Escape */
    }

    return 0;
}

/*---------------------------------------------------------------------------
 * Timing API
 *---------------------------------------------------------------------------*/

/**
 * Get system tick count
 */
uint32_t TickCount(void)
{
    UpdateTickCount();
    return g_tickCount;
}

/**
 * Get double-click time
 */
uint32_t GetDblTime(void)
{
    return g_doubleTime;
}

/**
 * Get caret time
 */
uint32_t GetCaretTime(void)
{
    return g_caretTime;
}

/**
 * Set system event mask
 */
void SetEventMask(int16_t mask)
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
void SetKeyRepeat(uint16_t delay, uint16_t rate)
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
void GenerateSystemEvent(int16_t eventType, int32_t message, Point where, int16_t modifiers)
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
bool CheckAbort(void)
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
void UpdateMouseState(Point newPos, uint8_t buttonState)
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
void SetTimingParameters(uint32_t doubleTime, uint32_t caretTime)
{
    g_doubleTime = doubleTime;
    g_caretTime = caretTime;
    g_eventGlobals.DoubleTime = doubleTime;
    g_eventGlobals.CaretTime = caretTime;
}