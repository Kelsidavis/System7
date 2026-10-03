/**
 * @file EventStructs.h
 * @brief Central location for Event Manager structure definitions
 *
 * This file defines all the shared structures used by the Event Manager
 * subsystem to avoid duplicate definitions.
 */

#ifndef EVENT_STRUCTS_H
#define EVENT_STRUCTS_H

#include "SystemTypes.h"


#ifdef __cplusplus
extern "C" {
#endif

/* KeyMap type - 128 bits for all keys */
#ifndef KEYMAP_DEFINED
#define KEYMAP_DEFINED
typedef UInt8 KeyMap[16];  /* 16 bytes = 128 bits */
#endif

/* Click information for double-click detection */
#ifndef CLICKINFO_DEFINED
#define CLICKINFO_DEFINED
typedef struct ClickInfo {
    Point       where;          /* Location of last click */
    UInt32      when;           /* Time of last click */
    UInt16      clickCount;     /* Number of clicks (1-3) */
    UInt16      reserved;
} ClickInfo;
#endif

/* Mouse tracking state */
#ifndef MOUSESTATE_DEFINED
#define MOUSESTATE_DEFINED
typedef struct MouseState {
    Point       position;       /* Current mouse position */
    UInt8       buttonState;    /* Current button state */
    UInt8       reserved;
    UInt16      reserved2;
} MouseState;
#endif

/* Keyboard state structure */
#ifndef KEYBOARDSTATE_DEFINED
#define KEYBOARDSTATE_DEFINED
typedef struct KeyboardState {
    KeyMap      keyMap;         /* Current key state */
    KeyMap      currentKeyMap;  /* Alias for keyMap */
    UInt16      modifiers;      /* Current modifier state */
    UInt16      modifierState;  /* Alias for modifiers */
    UInt16      lastKeyCode;    /* Last key pressed */
    UInt32      lastKeyTime;    /* Time of last key press */
    UInt32      lastEventTime;  /* Time of last event */
    Boolean     autoRepeatEnabled;
    Boolean     capsLockState;  /* Caps lock state */
    UInt16      repeatDelay;    /* Initial delay before repeat */
    UInt16      repeatRate;     /* Rate of repeat */
} KeyboardState;
#endif

/* Auto-repeat state */
#ifndef AUTOREPEATSTATE_DEFINED
#define AUTOREPEATSTATE_DEFINED
typedef struct AutoRepeatState {
    UInt16      keyCode;        /* Key being repeated */
    UInt16      charCode;       /* Character code */
    UInt32      startTime;      /* When key was first pressed */
    UInt32      lastRepeatTime; /* When last repeat was generated */
    UInt32      initialDelay;   /* Initial delay before repeat */
    UInt32      repeatRate;     /* Repeat rate */
    Boolean     repeating;      /* Currently repeating */
    Boolean     active;         /* Auto-repeat is active */
    Boolean     enabled;        /* Auto-repeat is enabled */
    UInt8       reserved;
} AutoRepeatState;
#endif

/* Dead key state for international input */
#ifndef DEADKEYSTATE_DEFINED
#define DEADKEYSTATE_DEFINED
typedef struct DeadKeyState {
    UInt16      deadKey;        /* Current dead key */
    UInt16      deadKeyType;    /* Type of dead key */
    UInt16      deadKeyScanCode; /* Scan code of dead key */
    UInt32      deadKeyTime;    /* Time of dead key press */
    Boolean     active;         /* Dead key is active */
    Boolean     waitingForNext; /* Waiting for next key */
    UInt16      reserved;
} DeadKeyState;
#endif

/* Key translation state */
#ifndef KEYTRANSSTATE_DEFINED
#define KEYTRANSSTATE_DEFINED
typedef struct KeyTransState {
    UInt32      state;          /* Translation state machine */
    Handle      kchrHandle;     /* Handle to KCHR resource */
} KeyTransState;
#endif

/* Event Manager global state structure */
#ifndef EVENTMGRGLOBALS_DEFINED
#define EVENTMGRGLOBALS_DEFINED
typedef struct EventMgrGlobals {
    /* System globals */
    UInt16      SysEvtMask;     /* System event mask */
    UInt32      Ticks;          /* Current tick count */
    Point       Mouse;          /* Current mouse position */
    UInt8       MBState;        /* Mouse button state */
    UInt8       reserved1;

    /* Timing parameters */
    UInt32      DoubleTime;     /* Double-click time */
    UInt32      CaretTime;      /* Caret blink time */

    /* Keyboard state */
    KeyMap      KeyMapState;    /* Current keyboard state */
    UInt32      KeyTime;        /* Time of last key event */
    UInt32      KeyRepTime;     /* Time of last repeat */
    UInt32      KeyThresh;      /* Key repeat threshold */
    UInt32      KeyRepThresh;   /* Key repeat rate */
    UInt16      KeyLast;        /* Last key code */
    UInt16      KeyMods;        /* Last key modifiers */

    /* Mouse state */
    ClickInfo   clickInfo;      /* Click detection info */
    MouseState  mouseState;     /* Current mouse state */

    /* Keyboard state */
    KeyboardState keyState;     /* Full keyboard state */
    AutoRepeatState autoRepeat; /* Auto-repeat state */
    DeadKeyState deadKey;       /* Dead key state */
    KeyTransState keyTrans;     /* Key translation state */

    /* Initialization flag */
    Boolean     initialized;
    UInt8       reserved[3];
} EventMgrGlobals;
#endif

#ifdef __cplusplus
}
#endif

#endif /* EVENT_STRUCTS_H */