/**
 * @file EventManager.h
 * @brief Complete Event Manager Implementation for System 7.1
 *
 * This file provides the main Event Manager API that maintains exact
 * compatibility with Mac OS System 7.1 event handling semantics while
 * providing modern cross-platform abstractions.
 *
 * The Event Manager is the core of all user interaction in System 7.1,
 * handling mouse events, keyboard input, system events, and providing
 * the foundation for all Mac applications.
 *
 * Features:
 * - Complete Mac OS Event Manager API compatibility
 * - Mouse event handling (clicks, drags, movement)
 * - Keyboard event processing (keys, modifiers, auto-repeat)
 * - System events (update, activate, suspend/resume)
 * - Event queue management with priority and filtering
 * - Double-click timing and multi-click detection
 * - Modern input abstraction for cross-platform support
 * - ADB Manager integration for low-level input
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct EventRecord EventRecord;
typedef struct EvQEl EvQEl;
typedef struct Region* RgnHandle;
typedef struct GrafPort* GrafPtr;
typedef struct WindowRecord* WindowPtr;

/* Point structure for mouse coordinates */
typedef struct Point {
    int16_t v;  /* vertical coordinate */
    int16_t h;  /* horizontal coordinate */
} Point;

/* Event types - exact Mac OS values */
enum {
    nullEvent       = 0,
    mouseDown       = 1,
    mouseUp         = 2,
    keyDown         = 3,
    keyUp           = 4,
    autoKey         = 5,
    updateEvt       = 6,
    diskEvt         = 7,
    activateEvt     = 8,
    /* 9-14 reserved */
    osEvt           = 15,
    /* High-level events */
    kHighLevelEvent = 23
};

/* Event masks - for filtering events */
enum {
    everyEvent          = -1,
    mDownMask          = 1 << 1,
    mUpMask            = 1 << 2,
    keyDownMask        = 1 << 3,
    keyUpMask          = 1 << 4,
    autoKeyMask        = 1 << 5,
    updateMask         = 1 << 6,
    diskMask           = 1 << 7,
    activMask          = 1 << 8,
    highLevelEventMask = 1 << 10,
    osMask             = 1 << 15
};

/* Event message masks */
enum {
    charCodeMask     = 0x000000FF,
    keyCodeMask      = 0x0000FF00,
    adbAddrMask      = 0x00FF0000,
    osEvtMessageMask = 0xFF000000
};

/* OS event message codes */
enum {
    mouseMovedMessage    = 0xFA,
    suspendResumeMessage = 0x01,
    /* Suspend/resume flags */
    resumeFlag           = 1,     /* Bit 0: 1=resume, 0=suspend */
    convertClipboardFlag = 2      /* Bit 1: clipboard conversion needed */
};

/* Modifier key flags */
enum {
    activeFlag   = 1,      /* Bit 0: window active flag */
    btnState     = 128,    /* Bit 7: mouse button state */
    cmdKey       = 256,    /* Bit 8: Command key */
    shiftKey     = 512,    /* Bit 9: Shift key */
    alphaLock    = 1024,   /* Bit 10: Caps Lock */
    optionKey    = 2048,   /* Bit 11: Option key */
    controlKey   = 4096,   /* Bit 12: Control key */
    rightShiftKey = 8192,  /* Bit 13: Right Shift key */
    rightOptionKey = 16384,/* Bit 14: Right Option key */
    rightControlKey = 32768/* Bit 15: Right Control key */
};

/* Obsolete event types (kept for compatibility) */
enum {
    networkEvt = 10,
    driverEvt  = 11,
    app1Evt    = 12,
    app2Evt    = 13,
    app3Evt    = 14,
    app4Evt    = 15
};

/* EventRecord - the fundamental event structure */
struct EventRecord {
    int16_t  what;        /* event type */
    int32_t  message;     /* event message/details */
    uint32_t when;        /* ticks since system startup */
    Point    where;       /* mouse location */
    int16_t  modifiers;   /* modifier keys and flags */
};

/* Event queue element */
struct EvQEl {
    struct EvQEl* qLink;     /* next element in queue */
    int16_t       qType;     /* queue element type */
    int16_t       evtQWhat;  /* event type */
    int32_t       evtQMessage; /* event message */
    uint32_t      evtQWhen;    /* event time */
    Point         evtQWhere;   /* event location */
    int16_t       evtQModifiers; /* event modifiers */
};

/* KeyMap type - 128 bits for all keys */
typedef uint32_t KeyMap[4];

/* Error codes */
enum {
    noErr        = 0,
    evtNotEnb    = 1,     /* event not enabled */
    noMemErr     = -108,  /* not enough memory */
    queueFull    = -109   /* event queue full */
};

/* Double-click detection */
typedef struct ClickInfo {
    Point    lastClickPt;    /* location of last click */
    uint32_t lastClickTime;  /* time of last click */
    int16_t  clickCount;     /* current click count */
    int16_t  clickTolerance; /* pixel tolerance for double-clicks */
} ClickInfo;

/* Mouse tracking state */
typedef struct MouseState {
    Point    currentPos;     /* current mouse position */
    Point    lastPos;        /* previous mouse position */
    bool     buttonDown;     /* current button state */
    bool     buttonWasDown;  /* previous button state */
    uint32_t lastMoveTime;   /* time of last movement */
    ClickInfo clickInfo;     /* double-click tracking */
} MouseState;

/* Keyboard state and auto-repeat */
typedef struct KeyboardState {
    KeyMap   currentKeys;    /* current key state */
    KeyMap   lastKeys;       /* previous key state */
    uint32_t lastKeyTime;    /* time of last key event */
    uint32_t repeatTime;     /* time of last repeat */
    uint16_t repeatDelay;    /* initial repeat delay */
    uint16_t repeatRate;     /* repeat rate */
    uint32_t lastKeyCode;    /* last key for auto-repeat */
    bool     autoRepeatEnabled; /* auto-repeat state */
} KeyboardState;

/* System globals (would normally be at low memory addresses) */
typedef struct EventMgrGlobals {
    uint16_t      SysEvtMask;    /* System event mask */
    uint32_t      Ticks;         /* System tick count */
    Point         Mouse;         /* Current mouse position */
    uint8_t       MBState;       /* Mouse button state */
    KeyMap        KeyMapState;   /* Current keyboard state */
    uint32_t      DoubleTime;    /* Double-click interval */
    uint32_t      CaretTime;     /* Caret blink interval */
    uint16_t      KeyThresh;     /* Key repeat delay */
    uint16_t      KeyRepThresh;  /* Key repeat rate */
    /* Auto-repeat state */
    uint32_t      KeyLast;       /* Last key pressed */
    uint32_t      KeyTime;       /* Time of last key press */
    uint32_t      KeyRepTime;    /* Time of last repeat */
    /* Extended state */
    MouseState    mouseState;    /* Mouse tracking */
    KeyboardState keyState;      /* Keyboard state */
    WindowPtr     frontWindow;   /* Front window for events */
    bool          initialized;   /* Event manager initialized */
} EventMgrGlobals;

/*---------------------------------------------------------------------------
 * Core Event Manager API
 *---------------------------------------------------------------------------*/

/**
 * Initialize the Event Manager
 * @param numEvents Number of event queue elements to allocate
 * @return Error code (0 = success)
 */
int16_t InitEvents(int16_t numEvents);

/**
 * Get next event matching mask (removes from queue)
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @return true if event found, false if null event
 */
bool GetNextEvent(int16_t eventMask, EventRecord* theEvent);

/**
 * Wait for next event with idle processing
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @param sleep Maximum ticks to sleep
 * @param mouseRgn Region for mouse-moved events
 * @return true if event found, false if null event
 */
bool WaitNextEvent(int16_t eventMask, EventRecord* theEvent,
                   uint32_t sleep, RgnHandle mouseRgn);

/**
 * Check if event is available (doesn't remove from queue)
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @return true if event found, false if null event
 */
bool EventAvail(int16_t eventMask, EventRecord* theEvent);

/**
 * Post an event to the queue
 * @param eventNum Event type
 * @param eventMsg Event message
 * @return Error code (0 = success, 1 = not enabled)
 */
int16_t PostEvent(int16_t eventNum, int32_t eventMsg);

/**
 * Post an event with queue element return
 * @param eventCode Event type
 * @param eventMsg Event message
 * @param qEl Pointer to receive queue element pointer
 * @return Error code
 */
int16_t PPostEvent(int16_t eventCode, int32_t eventMsg, EvQEl** qEl);

/**
 * OS-level event checking (low-level)
 * @param mask Event mask
 * @param theEvent Event record to fill
 * @return true if event found
 */
bool OSEventAvail(int16_t mask, EventRecord* theEvent);

/**
 * Get OS event (removes from queue, low-level)
 * @param mask Event mask
 * @param theEvent Event record to fill
 * @return true if event found
 */
bool GetOSEvent(int16_t mask, EventRecord* theEvent);

/**
 * Flush events from queue
 * @param whichMask Mask of events to remove
 * @param stopMask Mask of events that stop flushing
 */
void FlushEvents(int16_t whichMask, int16_t stopMask);

/*---------------------------------------------------------------------------
 * Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current mouse position
 * @param mouseLoc Pointer to Point to fill with position
 */
void GetMouse(Point* mouseLoc);

/**
 * Check if mouse button is currently down
 * @return true if button is down
 */
bool Button(void);

/**
 * Check if mouse button is still down (since last check)
 * @return true if button is still down
 */
bool StillDown(void);

/**
 * Wait for mouse button release
 * @return true if button was released
 */
bool WaitMouseUp(void);

/**
 * Track mouse dragging
 * @param startPt Starting point of drag
 * @param limitRect Rectangle to limit dragging
 * @param slopRect Rectangle for initial slop
 * @param axis Constraint axis (0=none, 1=horizontal, 2=vertical)
 * @return Final mouse position offset
 */
int32_t DragTheRgn(Point startPt, const struct Rect* limitRect,
                   const struct Rect* slopRect, int16_t axis);

/*---------------------------------------------------------------------------
 * Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current keyboard state
 * @param theKeys 128-bit keymap to fill
 */
void GetKeys(KeyMap theKeys);

/**
 * Translate key code using KCHR resource
 * @param transData Pointer to KCHR resource data
 * @param keycode Key code and modifier flags
 * @param state Pointer to translation state
 * @return Character code or function key code
 */
int32_t KeyTranslate(const void* transData, uint16_t keycode, uint32_t* state);

/**
 * Compatibility name for KeyTranslate
 */
#define KeyTrans KeyTranslate

/**
 * Check for Command-Period abort
 * @return true if user pressed Cmd-Period
 */
bool CheckAbort(void);

/*---------------------------------------------------------------------------
 * Timing API
 *---------------------------------------------------------------------------*/

/**
 * Get system tick count
 * @return Ticks since system startup
 */
uint32_t TickCount(void);

/**
 * Get double-click time threshold
 * @return Ticks for double-click timing
 */
uint32_t GetDblTime(void);

/**
 * Get caret blink time
 * @return Ticks for caret blink rate
 */
uint32_t GetCaretTime(void);

/**
 * Set system event mask
 * @param mask New event mask
 */
void SetEventMask(int16_t mask);

/*---------------------------------------------------------------------------
 * Event Manager Extended API
 *---------------------------------------------------------------------------*/

/**
 * Set key repeat thresholds
 * @param delay Initial delay before repeat (ticks)
 * @param rate Rate of repeat (ticks between repeats)
 */
void SetKeyRepeat(uint16_t delay, uint16_t rate);

/**
 * Get global event manager state
 * @return Pointer to global state structure
 */
EventMgrGlobals* GetEventMgrGlobals(void);

/**
 * Generate system event (for internal use)
 * @param eventType Type of event to generate
 * @param message Event message
 * @param where Event location
 * @param modifiers Event modifiers
 */
void GenerateSystemEvent(int16_t eventType, int32_t message, Point where, int16_t modifiers);

/**
 * Process null event (idle processing)
 */
void ProcessNullEvent(void);

/**
 * Set the front window for event targeting
 * @param window Window to receive events
 */
void SetEventWindow(WindowPtr window);

/**
 * Get the front window for event targeting
 * @return Current front window
 */
WindowPtr GetEventWindow(void);

/*---------------------------------------------------------------------------
 * Modern Input Integration API
 *---------------------------------------------------------------------------*/

/**
 * Initialize modern input system
 * @param platform Platform identifier (X11, Cocoa, Win32, etc.)
 * @return Error code
 */
int16_t InitModernInput(const char* platform);

/**
 * Process modern input events
 * Should be called regularly from main event loop
 */
void ProcessModernInput(void);

/**
 * Shutdown modern input system
 */
void ShutdownModernInput(void);

/**
 * Enable/disable modern input features
 * @param multiTouch Enable multi-touch support
 * @param gestures Enable gesture recognition
 * @param accessibility Enable accessibility features
 */
void ConfigureModernInput(bool multiTouch, bool gestures, bool accessibility);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_MANAGER_H */