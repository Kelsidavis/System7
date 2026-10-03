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

#include "SystemTypes.h"

/* Forward declarations */


#ifdef __cplusplus
extern "C" {
#endif

/* Basic geometric types */
/* Point type defined in MacTypes.h */

/* Rect type defined in MacTypes.h */

/* Pack/unpack Point into/from a 32-bit message */
#define PACK_POINT(h, v)   (((UInt32)(h) << 16) | ((UInt16)(v)))
#define UNPACK_H(msg)      ((SInt16)((msg) >> 16))
#define UNPACK_V(msg)      ((SInt16)((msg) & 0xFFFF))

/* Event types - exact Mac OS System 7.1 values */
/* These are already defined in SystemTypes.h
enum {
    nullEvent           = 0,
    mouseDown           = 1,
    mouseUp             = 2,
    keyDown             = 3,
    keyUp               = 4,
    autoKey             = 5,
    updateEvt           = 6,
    diskEvt             = 7,
    activateEvt         = 8,
    osEvt               = 15,
    kHighLevelEvent     = 23
};
*/
#ifndef kHighLevelEvent
#define kHighLevelEvent 23
#endif

/* Event masks for filtering events */
#ifndef mDownMask
enum {
    mDownMask           = 1 << 1,   /* mouseDown */
    mUpMask             = 1 << 2,   /* mouseUp */
    keyDownMask         = 1 << 3,   /* keyDown */
    keyUpMask           = 1 << 4,   /* keyUp */
    autoKeyMask         = 1 << 5,   /* autoKey */
    updateMask          = 1 << 6,   /* updateEvt */
    diskMask            = 1 << 7,   /* diskEvt */
    activMask           = 1 << 8,   /* activateEvt */
    highLevelEventMask  = 1 << 23,  /* kHighLevelEvent */
    osMask              = 1 << 15   /* osEvt */
    /* everyEvent already defined in SystemTypes.h */
};
#endif

/* Event message field bit masks */
enum {
    charCodeMask        = 0x000000FF,
    keyCodeMask         = 0x0000FF00
};

/* OS event message codes (in high byte of message) */
enum {
    suspendResumeMessage = 0x01,
    mouseMovedMessage   = 0xFA
};

/* Suspend/resume message flags (in low byte) */
enum {
    resumeFlag          = 1,
    convertClipboardFlag = 2
};

/* Modifier key flags in event modifiers field */
enum {
    activeFlag          = 0x0001,  /* For activate events */
    btnState            = 0x0080,  /* Mouse button state */
    cmdKey              = 0x0100,  /* Command key */
    shiftKey            = 0x0200,  /* Shift key */
    alphaLock           = 0x0400,  /* Caps lock */
    optionKey           = 0x0800,  /* Option key */
    controlKey          = 0x1000,  /* Control key */
    rightShiftKey       = 0x2000,  /* Right shift key */
    rightOptionKey      = 0x4000,  /* Right option key */
    rightControlKey     = 0x8000   /* Right control key */
};

/* Mouse button states */
enum {
    btnUp               = 0,
    btnDown             = 1
};

/* Key codes for special keys */
enum {
    kScanReturn         = 0x24,
    kScanTab            = 0x30,
    kScanSpace          = 0x31,
    kScanDelete         = 0x33,
    kScanEscape         = 0x35,
    kScanCommand        = 0x37,
    kScanShift          = 0x38,
    kScanCapsLock       = 0x39,
    kScanOption         = 0x3A,
    kScanControl        = 0x3B,
    kScanRightShift     = 0x3C,
    kScanRightOption    = 0x3D,
    kScanRightControl   = 0x3E,
    kScanFunction       = 0x3F,

    /* Function keys */
    kScanF1             = 0x7A,
    kScanF2             = 0x78,
    kScanF3             = 0x63,
    kScanF4             = 0x76,
    kScanF5             = 0x60,
    kScanF6             = 0x61,
    kScanF7             = 0x62,
    kScanF8             = 0x64,
    kScanF9             = 0x65,
    kScanF10            = 0x6D,
    kScanF11            = 0x67,
    kScanF12            = 0x6F,

    /* Arrow keys */
    kScanLeftArrow      = 0x7B,
    kScanRightArrow     = 0x7C,
    kScanDownArrow      = 0x7D,
    kScanUpArrow        = 0x7E
};

/* EventRecord - the fundamental event structure */
/* EventRecord is in EventManager/EventTypes.h */

/* Event queue element structure */
typedef struct EvQEl {
    QElemPtr    qLink;         /* Next element in queue */
    SInt16      qType;         /* Queue type */
    SInt16      evtQWhat;      /* Event type */
    SInt32      evtQMessage;   /* Event message */
    UInt32      evtQWhen;      /* Event time */
    Point       evtQWhere;     /* Event location */
    SInt16      evtQModifiers; /* Event modifiers */
    EventRecord eventRecord;   /* Full event record */
} EvQEl;

/* KeyMap type */

/* Ptr is defined in MacTypes.h */

/* Queue header structure */
// 

/* Ptr is defined in MacTypes.h */

/* KeyMap type for keyboard state - 128 bits total */

/* Window pointer type */
/* Ptr is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */

/* Region handle type */
/* Handle is defined in MacTypes.h */

/* Error codes specific to Event Manager */
enum {
    evtNotEnb           = 1,    /* Event not enabled */
    queueFull           = -1    /* Event queue is full */
};

/* Event queue constants */
enum {
    kDefaultEventQueueSize = 20,
    kMaxEventQueueSize = 100,
    kEventQueueType = 1
};

/* Timing constants */
enum {
    kDefaultDoubleClickTime = 30,  /* 0.5 seconds at 60 ticks/sec */
    kDefaultCaretBlinkTime = 30,   /* 0.5 seconds */
    kDefaultKeyRepeatDelay = 18,   /* 0.3 seconds */
    kDefaultKeyRepeatRate = 3      /* 0.05 seconds */
};

/* Mouse tracking constants */
enum {
    kDoubleClickDistance = 5       /* Pixels for double-click radius */
};

/* Extended event information */

/* Click state tracking */

/* Keyboard auto-repeat state */

/* Platform-specific input state */

/* Event filter callback function type */

/* Modern input event types (for internal use) */

#ifdef __cplusplus
}
#endif

#endif /* EVENT_TYPES_H */
