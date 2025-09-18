/**
 * @file EventTypes.h
 * @brief Event type definitions and constants for System 7.1 Event Manager
 *
 * This file contains all the event type definitions, constants, and
 * data structures used by the Event Manager. It provides exact
 * compatibility with Mac OS System 7.1 event system.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic geometric types */
typedef struct Point {
    int16_t v;  /* vertical coordinate */
    int16_t h;  /* horizontal coordinate */
} Point;

typedef struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

/* Event types - exact Mac OS System 7.1 values */
enum {
    nullEvent       = 0,    /* No event */
    mouseDown       = 1,    /* Mouse button pressed */
    mouseUp         = 2,    /* Mouse button released */
    keyDown         = 3,    /* Key pressed */
    keyUp           = 4,    /* Key released */
    autoKey         = 5,    /* Auto-repeat key */
    updateEvt       = 6,    /* Window needs update */
    diskEvt         = 7,    /* Disk inserted */
    activateEvt     = 8,    /* Window activated/deactivated */
    /* 9-14 reserved for future use */
    osEvt           = 15,   /* Operating system event */

    /* Extended event types */
    kHighLevelEvent = 23,   /* High-level events (Apple Events) */

    /* Obsolete event types (maintained for compatibility) */
    networkEvt      = 10,   /* Network event (obsolete) */
    driverEvt       = 11,   /* Driver event (obsolete) */
    app1Evt         = 12,   /* Application event 1 (obsolete) */
    app2Evt         = 13,   /* Application event 2 (obsolete) */
    app3Evt         = 14,   /* Application event 3 (obsolete) */
    app4Evt         = 15    /* Application event 4 (obsolete) */
};

/* Event masks for filtering events */
enum {
    everyEvent          = -1,       /* All events */
    mDownMask          = (1 << 1),  /* Mouse down events */
    mUpMask            = (1 << 2),  /* Mouse up events */
    keyDownMask        = (1 << 3),  /* Key down events */
    keyUpMask          = (1 << 4),  /* Key up events */
    autoKeyMask        = (1 << 5),  /* Auto-repeat events */
    updateMask         = (1 << 6),  /* Update events */
    diskMask           = (1 << 7),  /* Disk events */
    activMask          = (1 << 8),  /* Activate events */
    /* Bit 9 reserved */
    highLevelEventMask = (1 << 10), /* High-level events */
    /* Bits 11-14 reserved */
    osMask             = (1 << 15), /* OS events */

    /* Obsolete masks */
    networkMask        = (1 << 10), /* Network events (obsolete) */
    driverMask         = (1 << 11), /* Driver events (obsolete) */
    app1Mask           = (1 << 12), /* App1 events (obsolete) */
    app2Mask           = (1 << 13), /* App2 events (obsolete) */
    app3Mask           = (1 << 14), /* App3 events (obsolete) */
    app4Mask           = (1 << 15)  /* App4 events (obsolete) */
};

/* Event message field bit masks */
enum {
    charCodeMask     = 0x000000FF,  /* Character code in key events */
    keyCodeMask      = 0x0000FF00,  /* Key code in key events */
    adbAddrMask      = 0x00FF0000,  /* ADB address */
    osEvtMessageMask = 0xFF000000   /* OS event subtype */
};

/* OS event message codes (in high byte of message) */
enum {
    mouseMovedMessage    = 0xFA,    /* Mouse moved */
    suspendResumeMessage = 0x01,    /* Suspend/resume */
    clipboardChangedMessage = 0x02  /* Clipboard changed */
};

/* Suspend/resume message flags (in low byte) */
enum {
    resumeFlag           = 1,       /* Bit 0: 1=resume, 0=suspend */
    convertClipboardFlag = 2        /* Bit 1: clipboard conversion needed */
};

/* Modifier key flags in event modifiers field */
enum {
    activeFlag     = (1 << 0),      /* Window active (activate events) */
    btnState       = (1 << 7),      /* Mouse button state */
    cmdKey         = (1 << 8),      /* Command key */
    shiftKey       = (1 << 9),      /* Shift key */
    alphaLock      = (1 << 10),     /* Caps Lock */
    optionKey      = (1 << 11),     /* Option key */
    controlKey     = (1 << 12),     /* Control key */
    rightShiftKey  = (1 << 13),     /* Right Shift key (extended) */
    rightOptionKey = (1 << 14),     /* Right Option key (extended) */
    rightControlKey = (1 << 15)     /* Right Control key (extended) */
};

/* Mouse button states */
enum {
    mouseButtonUp   = 0,            /* Mouse button not pressed */
    mouseButtonDown = 1             /* Mouse button pressed */
};

/* Key codes for special keys */
enum {
    kReturnCharCode     = 0x0D,     /* Return key */
    kEnterCharCode      = 0x03,     /* Enter key */
    kBackspaceCharCode  = 0x08,     /* Backspace */
    kTabCharCode        = 0x09,     /* Tab */
    kEscapeCharCode     = 0x1B,     /* Escape */
    kSpaceCharCode      = 0x20,     /* Space */
    kDeleteCharCode     = 0x7F,     /* Delete (forward delete) */

    /* Function key codes (scan codes) */
    kF1KeyCode          = 0x7A,     /* F1 key */
    kF2KeyCode          = 0x78,     /* F2 key */
    kF3KeyCode          = 0x63,     /* F3 key */
    kF4KeyCode          = 0x76,     /* F4 key */
    kF5KeyCode          = 0x60,     /* F5 key */
    kF6KeyCode          = 0x61,     /* F6 key */
    kF7KeyCode          = 0x62,     /* F7 key */
    kF8KeyCode          = 0x64,     /* F8 key */
    kF9KeyCode          = 0x65,     /* F9 key */
    kF10KeyCode         = 0x6D,     /* F10 key */
    kF11KeyCode         = 0x67,     /* F11 key */
    kF12KeyCode         = 0x6F,     /* F12 key */

    /* Arrow key codes */
    kLeftArrowKeyCode   = 0x7B,     /* Left arrow */
    kRightArrowKeyCode  = 0x7C,     /* Right arrow */
    kUpArrowKeyCode     = 0x7E,     /* Up arrow */
    kDownArrowKeyCode   = 0x7D,     /* Down arrow */

    /* Other special keys */
    kHomeKeyCode        = 0x73,     /* Home */
    kEndKeyCode         = 0x77,     /* End */
    kPageUpKeyCode      = 0x74,     /* Page Up */
    kPageDownKeyCode    = 0x79,     /* Page Down */
    kHelpKeyCode        = 0x72      /* Help */
};

/* EventRecord - the fundamental event structure */
typedef struct EventRecord {
    int16_t  what;        /* Event type */
    int32_t  message;     /* Event-specific message */
    uint32_t when;        /* Time stamp (ticks since system startup) */
    Point    where;       /* Mouse location (global coordinates) */
    int16_t  modifiers;   /* Modifier keys and button state */
} EventRecord;

/* Event queue element structure */
typedef struct EvQEl {
    struct EvQEl* qLink;        /* Link to next queue element */
    int16_t       qType;        /* Queue element type */
    int16_t       evtQWhat;     /* Event type */
    int32_t       evtQMessage;  /* Event message */
    uint32_t      evtQWhen;     /* Event time */
    Point         evtQWhere;    /* Event location */
    int16_t       evtQModifiers; /* Event modifiers */
} EvQEl;

typedef EvQEl* EvQElPtr;

/* Queue header structure */
typedef struct QHdr {
    int16_t qFlags;         /* Queue flags */
    void*   qHead;          /* First element */
    void*   qTail;          /* Last element */
} QHdr;

typedef QHdr* QHdrPtr;

/* KeyMap type for keyboard state - 128 bits total */
typedef uint32_t KeyMap[4];

/* Window pointer type */
typedef struct WindowRecord* WindowPtr;
typedef struct GrafPort* GrafPtr;

/* Region handle type */
typedef struct Region** RgnHandle;

/* Error codes specific to Event Manager */
enum {
    evtNotEnb    = 1,       /* Event type not enabled */
    queueFull    = -32767   /* Event queue is full */
};

/* Event queue constants */
enum {
    kDefaultEventQueueSize = 20,    /* Default number of queue elements */
    kMaxEventQueueSize     = 100,   /* Maximum queue size */
    kEventQueueElementSize = sizeof(EvQEl) /* Size of one queue element */
};

/* Timing constants */
enum {
    kDefaultDoubleClickTime = 30,   /* Default double-click time (ticks) */
    kDefaultCaretBlinkTime  = 30,   /* Default caret blink time (ticks) */
    kDefaultKeyRepeatDelay  = 30,   /* Default key repeat delay (ticks) */
    kDefaultKeyRepeatRate   = 3,    /* Default key repeat rate (ticks) */
    kMinDoubleClickTime     = 5,    /* Minimum double-click time */
    kMaxDoubleClickTime     = 120,  /* Maximum double-click time */
    kDoubleClickTolerance   = 4     /* Pixel tolerance for double-clicks */
};

/* Mouse tracking constants */
enum {
    kMouseMoveThreshold     = 1,    /* Minimum pixels for mouse move */
    kDragStartThreshold     = 4,    /* Pixels to start drag operation */
    kMaxClickCount          = 3     /* Maximum multi-click count */
};

/* Extended event information */
typedef struct ExtendedEventInfo {
    int16_t  eventClass;        /* Event class for high-level events */
    int16_t  eventKind;         /* Event kind */
    uint32_t eventSourcePSN[2]; /* Process serial number of source */
    uint32_t eventTargetPSN[2]; /* Process serial number of target */
    int32_t  eventRefCon;       /* Reference constant */
    bool     eventReplyRequired; /* Reply required flag */
} ExtendedEventInfo;

/* Click state tracking */
typedef struct ClickState {
    Point    lastClickPt;       /* Last click location */
    uint32_t lastClickTime;     /* Time of last click */
    int16_t  clickCount;        /* Current click count (1, 2, 3...) */
    int16_t  lastEventType;     /* Type of last mouse event */
    bool     trackingClick;     /* Currently tracking a click sequence */
} ClickState;

/* Keyboard auto-repeat state */
typedef struct AutoRepeatState {
    uint32_t lastKeyCode;       /* Last key code pressed */
    uint32_t lastKeyTime;       /* Time of last key press */
    uint32_t lastRepeatTime;    /* Time of last auto-repeat */
    bool     repeatActive;      /* Auto-repeat currently active */
    bool     repeatEnabled;     /* Auto-repeat enabled globally */
} AutoRepeatState;

/* Platform-specific input state */
typedef struct PlatformInputState {
    void*    platformData;      /* Platform-specific data */
    int16_t  platformType;      /* Platform type identifier */
    bool     initialized;       /* Platform input initialized */
    bool     multiTouchEnabled; /* Multi-touch support enabled */
    bool     gesturesEnabled;   /* Gesture recognition enabled */
} PlatformInputState;

/* Event filter callback function type */
typedef bool (*EventFilterProc)(EventRecord* event, void* userData);

/* Modern input event types (for internal use) */
enum {
    kModernMouseEvent    = 1000,    /* Modern mouse event */
    kModernKeyboardEvent = 1001,    /* Modern keyboard event */
    kModernTouchEvent    = 1002,    /* Touch event */
    kModernGestureEvent  = 1003,    /* Gesture event */
    kModernScrollEvent   = 1004     /* Scroll wheel event */
};

#ifdef __cplusplus
}
#endif

#endif /* EVENT_TYPES_H */