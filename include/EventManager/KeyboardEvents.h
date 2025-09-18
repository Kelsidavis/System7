/**
 * @file KeyboardEvents.h
 * @brief Keyboard Event Processing for System 7.1 Event Manager
 *
 * This file provides comprehensive keyboard event handling including
 * key presses, modifier keys, auto-repeat, international layouts,
 * and modern keyboard features.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef KEYBOARD_EVENTS_H
#define KEYBOARD_EVENTS_H

#include "EventTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Key state constants */
enum {
    kKeyStateUp     = 0,    /* Key is not pressed */
    kKeyStateDown   = 1,    /* Key is pressed */
    kKeyStateRepeat = 2     /* Key is auto-repeating */
};

/* Keyboard layout types */
enum {
    kLayoutUS           = 0,    /* US English */
    kLayoutUK           = 1,    /* UK English */
    kLayoutFrench       = 2,    /* French */
    kLayoutGerman       = 3,    /* German */
    kLayoutItalian      = 4,    /* Italian */
    kLayoutSpanish      = 5,    /* Spanish */
    kLayoutSwedish      = 6,    /* Swedish */
    kLayoutJapanese     = 7,    /* Japanese */
    kLayoutCustom       = 100   /* Custom layout */
};

/* Dead key types for international layouts */
enum {
    kDeadKeyNone        = 0,    /* No dead key */
    kDeadKeyAcute       = 1,    /* Acute accent (´) */
    kDeadKeyGrave       = 2,    /* Grave accent (`) */
    kDeadKeyCircumflex  = 3,    /* Circumflex (^) */
    kDeadKeyTilde       = 4,    /* Tilde (~) */
    kDeadKeyUmlaut      = 5,    /* Umlaut/diaeresis (¨) */
    kDeadKeyRing        = 6,    /* Ring above (°) */
    kDeadKeyCedilla     = 7     /* Cedilla (¸) */
};

/* Special key codes (scan codes) */
enum {
    /* Function keys */
    kScanF1         = 0x7A,     /* F1 */
    kScanF2         = 0x78,     /* F2 */
    kScanF3         = 0x63,     /* F3 */
    kScanF4         = 0x76,     /* F4 */
    kScanF5         = 0x60,     /* F5 */
    kScanF6         = 0x61,     /* F6 */
    kScanF7         = 0x62,     /* F7 */
    kScanF8         = 0x64,     /* F8 */
    kScanF9         = 0x65,     /* F9 */
    kScanF10        = 0x6D,     /* F10 */
    kScanF11        = 0x67,     /* F11 */
    kScanF12        = 0x6F,     /* F12 */

    /* Arrow keys */
    kScanLeftArrow  = 0x7B,     /* Left arrow */
    kScanRightArrow = 0x7C,     /* Right arrow */
    kScanUpArrow    = 0x7E,     /* Up arrow */
    kScanDownArrow  = 0x7D,     /* Down arrow */

    /* Navigation keys */
    kScanHome       = 0x73,     /* Home */
    kScanEnd        = 0x77,     /* End */
    kScanPageUp     = 0x74,     /* Page Up */
    kScanPageDown   = 0x79,     /* Page Down */
    kScanHelp       = 0x72,     /* Help */
    kScanForwardDel = 0x75,     /* Forward Delete */

    /* Modifier keys */
    kScanCommand    = 0x37,     /* Command (left) */
    kScanShift      = 0x38,     /* Shift (left) */
    kScanCapsLock   = 0x39,     /* Caps Lock */
    kScanOption     = 0x3A,     /* Option (left) */
    kScanControl    = 0x3B,     /* Control (left) */
    kScanRightShift = 0x3C,     /* Shift (right) */
    kScanRightOption = 0x3D,    /* Option (right) */
    kScanRightControl = 0x3E,   /* Control (right) */
    kScanFunction   = 0x3F      /* Function key */
};

/* KeyMap indices for modifier keys */
enum {
    kKeyMapIndexCommand     = 55,   /* Command key bit */
    kKeyMapIndexShift       = 56,   /* Shift key bit */
    kKeyMapIndexCapsLock    = 57,   /* Caps Lock bit */
    kKeyMapIndexOption      = 58,   /* Option key bit */
    kKeyMapIndexControl     = 59    /* Control key bit */
};

/* Keyboard state structure */
typedef struct KeyboardState {
    KeyMap      currentKeyMap;      /* Current key state (128 bits) */
    KeyMap      lastKeyMap;         /* Previous key state */
    uint16_t    modifierState;      /* Current modifier state */
    uint16_t    lastModifierState;  /* Previous modifier state */
    uint32_t    lastEventTime;      /* Time of last keyboard event */
    bool        numLockState;       /* Num Lock state */
    bool        capsLockState;      /* Caps Lock state */
    bool        scrollLockState;    /* Scroll Lock state */
} KeyboardState;

/* Auto-repeat state */
typedef struct AutoRepeatState {
    uint16_t    keyCode;            /* Key being repeated */
    uint32_t    charCode;           /* Character being repeated */
    uint32_t    startTime;          /* Time repeat started */
    uint32_t    lastRepeatTime;     /* Time of last repeat */
    uint32_t    initialDelay;       /* Initial delay before repeat */
    uint32_t    repeatRate;         /* Rate of repeat */
    bool        active;             /* Auto-repeat is active */
    bool        enabled;            /* Auto-repeat is enabled */
} AutoRepeatState;

/* Dead key state for international input */
typedef struct DeadKeyState {
    int16_t     deadKeyType;        /* Type of dead key */
    uint16_t    deadKeyScanCode;    /* Scan code of dead key */
    uint32_t    deadKeyTime;        /* Time dead key was pressed */
    bool        waitingForNext;     /* Waiting for next character */
} DeadKeyState;

/* Keyboard layout information */
typedef struct KeyboardLayout {
    int16_t     layoutType;         /* Layout type identifier */
    const char* layoutName;         /* Human-readable name */
    void*       kchrResource;       /* KCHR resource data */
    uint32_t    kchrSize;          /* Size of KCHR resource */
    bool        isActive;          /* Currently active layout */
    DeadKeyState deadKeyState;     /* Dead key processing state */
} KeyboardLayout;

/* Key translation state */
typedef struct KeyTransState {
    uint32_t    state;             /* Translation state */
    int16_t     layoutID;          /* Current layout ID */
    bool        hasDeadKey;        /* Dead key in progress */
} KeyTransState;

/* Keyboard event context */
typedef struct KeyboardEventContext {
    EventRecord*    baseEvent;      /* Base event record */
    uint16_t        scanCode;       /* Hardware scan code */
    uint32_t        charCode;       /* Translated character */
    uint16_t        modifiers;      /* Modifier state */
    uint32_t        timestamp;      /* Event timestamp */
    bool            isAutoRepeat;   /* Is auto-repeat event */
    bool            consumed;       /* Event has been consumed */
    KeyTransState*  transState;     /* Translation state */
} KeyboardEventContext;

/* Callback function types */
typedef bool (*KeyboardEventCallback)(KeyboardEventContext* context, void* userData);
typedef void (*ModifierChangeCallback)(uint16_t oldState, uint16_t newState, void* userData);
typedef bool (*KeyFilterCallback)(uint16_t scanCode, uint32_t charCode, uint16_t modifiers, void* userData);

/*---------------------------------------------------------------------------
 * Core Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize keyboard event system
 * @return Error code (0 = success)
 */
int16_t InitKeyboardEvents(void);

/**
 * Shutdown keyboard event system
 */
void ShutdownKeyboardEvents(void);

/**
 * Process raw keyboard event
 * @param scanCode Hardware scan code
 * @param isKeyDown true for key down, false for key up
 * @param modifiers Modifier key state
 * @param timestamp Event timestamp
 * @return Number of events generated
 */
int16_t ProcessRawKeyboardEvent(uint16_t scanCode, bool isKeyDown,
                               uint16_t modifiers, uint32_t timestamp);

/**
 * Get current keyboard state map
 * @param theKeys 128-bit keymap to fill
 */
void GetKeys(KeyMap theKeys);

/**
 * Check if specific key is pressed
 * @param scanCode Scan code to check
 * @return true if key is pressed
 */
bool IsKeyDown(uint16_t scanCode);

/**
 * Get current modifier key state
 * @return Modifier key bitmask
 */
uint16_t GetModifierState(void);

/**
 * Check if specific modifier is pressed
 * @param modifier Modifier flag to check
 * @return true if modifier is pressed
 */
bool IsModifierDown(uint16_t modifier);

/*---------------------------------------------------------------------------
 * Key Translation and Character Generation
 *---------------------------------------------------------------------------*/

/**
 * Translate key using KCHR resource
 * @param transData Pointer to KCHR resource
 * @param keyCode Key code and modifier information
 * @param state Pointer to translation state
 * @return Character code or function key code
 */
int32_t KeyTranslate(const void* transData, uint16_t keyCode, uint32_t* state);

/**
 * Translate scan code to character using current layout
 * @param scanCode Hardware scan code
 * @param modifiers Modifier key state
 * @param transState Translation state
 * @return Character code (0 if no character)
 */
uint32_t TranslateScanCode(uint16_t scanCode, uint16_t modifiers, KeyTransState* transState);

/**
 * Get character for key combination
 * @param scanCode Scan code
 * @param modifiers Modifier state
 * @return Character code
 */
uint32_t GetKeyCharacter(uint16_t scanCode, uint16_t modifiers);

/**
 * Reset key translation state
 * @param transState Translation state to reset
 */
void ResetKeyTransState(KeyTransState* transState);

/*---------------------------------------------------------------------------
 * Auto-Repeat Management
 *---------------------------------------------------------------------------*/

/**
 * Initialize auto-repeat system
 * @param initialDelay Delay before repeat starts (ticks)
 * @param repeatRate Rate of repeat (ticks between repeats)
 */
void InitAutoRepeat(uint32_t initialDelay, uint32_t repeatRate);

/**
 * Set auto-repeat parameters
 * @param initialDelay Delay before repeat starts
 * @param repeatRate Rate of repeat
 */
void SetAutoRepeat(uint32_t initialDelay, uint32_t repeatRate);

/**
 * Get auto-repeat parameters
 * @param initialDelay Pointer to receive initial delay
 * @param repeatRate Pointer to receive repeat rate
 */
void GetAutoRepeat(uint32_t* initialDelay, uint32_t* repeatRate);

/**
 * Enable or disable auto-repeat
 * @param enabled true to enable auto-repeat
 */
void SetAutoRepeatEnabled(bool enabled);

/**
 * Check if auto-repeat is enabled
 * @return true if auto-repeat is enabled
 */
bool IsAutoRepeatEnabled(void);

/**
 * Process auto-repeat timing
 * Called regularly to generate auto-repeat events
 */
void ProcessAutoRepeat(void);

/**
 * Start auto-repeat for a key
 * @param scanCode Scan code of key
 * @param charCode Character code of key
 */
void StartAutoRepeat(uint16_t scanCode, uint32_t charCode);

/**
 * Stop auto-repeat
 */
void StopAutoRepeat(void);

/*---------------------------------------------------------------------------
 * Keyboard Layout Management
 *---------------------------------------------------------------------------*/

/**
 * Initialize keyboard layouts
 * @return Error code
 */
int16_t InitKeyboardLayouts(void);

/**
 * Load keyboard layout
 * @param layoutType Layout type identifier
 * @param layoutData KCHR resource data
 * @param dataSize Size of resource data
 * @return Layout handle
 */
KeyboardLayout* LoadKeyboardLayout(int16_t layoutType, const void* layoutData, uint32_t dataSize);

/**
 * Set active keyboard layout
 * @param layout Layout to activate
 * @return Error code
 */
int16_t SetActiveKeyboardLayout(KeyboardLayout* layout);

/**
 * Get active keyboard layout
 * @return Current active layout
 */
KeyboardLayout* GetActiveKeyboardLayout(void);

/**
 * Get layout by type
 * @param layoutType Layout type to find
 * @return Layout handle or NULL
 */
KeyboardLayout* GetKeyboardLayoutByType(int16_t layoutType);

/*---------------------------------------------------------------------------
 * International Input Support
 *---------------------------------------------------------------------------*/

/**
 * Process dead key input
 * @param deadKeyCode Dead key scan code
 * @param nextChar Next character input
 * @return Composed character or 0
 */
uint32_t ProcessDeadKey(uint16_t deadKeyCode, uint32_t nextChar);

/**
 * Check if scan code is a dead key
 * @param scanCode Scan code to check
 * @param modifiers Modifier state
 * @return Dead key type or kDeadKeyNone
 */
int16_t GetDeadKeyType(uint16_t scanCode, uint16_t modifiers);

/**
 * Compose character with accent
 * @param baseChar Base character
 * @param accentType Accent type
 * @return Composed character or base character
 */
uint32_t ComposeCharacter(uint32_t baseChar, int16_t accentType);

/**
 * Reset dead key state
 */
void ResetDeadKeyState(void);

/*---------------------------------------------------------------------------
 * Modern Keyboard Features
 *---------------------------------------------------------------------------*/

/**
 * Enable extended key support
 * @param enabled true to enable extended keys
 */
void SetExtendedKeysEnabled(bool enabled);

/**
 * Process media key event
 * @param mediaKey Media key identifier
 * @param isPressed true if pressed, false if released
 * @return true if event was handled
 */
bool ProcessMediaKeyEvent(int16_t mediaKey, bool isPressed);

/**
 * Set keyboard backlight level
 * @param level Backlight level (0.0 to 1.0)
 */
void SetKeyboardBacklight(float level);

/**
 * Get keyboard backlight level
 * @return Current backlight level
 */
float GetKeyboardBacklight(void);

/*---------------------------------------------------------------------------
 * Event Generation
 *---------------------------------------------------------------------------*/

/**
 * Generate key down event
 * @param scanCode Key scan code
 * @param charCode Character code
 * @param modifiers Modifier state
 * @return Generated event
 */
EventRecord GenerateKeyDownEvent(uint16_t scanCode, uint32_t charCode, uint16_t modifiers);

/**
 * Generate key up event
 * @param scanCode Key scan code
 * @param charCode Character code
 * @param modifiers Modifier state
 * @return Generated event
 */
EventRecord GenerateKeyUpEvent(uint16_t scanCode, uint32_t charCode, uint16_t modifiers);

/**
 * Generate auto-key event
 * @param scanCode Key scan code
 * @param charCode Character code
 * @param modifiers Modifier state
 * @return Generated event
 */
EventRecord GenerateAutoKeyEvent(uint16_t scanCode, uint32_t charCode, uint16_t modifiers);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Check for Command-Period abort sequence
 * @return true if Cmd-Period was pressed
 */
bool CheckAbort(void);

/**
 * Convert scan code to virtual key code
 * @param scanCode Hardware scan code
 * @return Virtual key code
 */
uint16_t ScanCodeToVirtualKey(uint16_t scanCode);

/**
 * Convert virtual key code to scan code
 * @param virtualKey Virtual key code
 * @return Hardware scan code
 */
uint16_t VirtualKeyToScanCode(uint16_t virtualKey);

/**
 * Get key name string
 * @param scanCode Scan code
 * @param modifiers Modifier state
 * @param buffer Buffer for key name
 * @param bufferSize Size of buffer
 * @return Length of key name
 */
int16_t GetKeyName(uint16_t scanCode, uint16_t modifiers, char* buffer, int16_t bufferSize);

/**
 * Check if character is printable
 * @param charCode Character code to check
 * @return true if character is printable
 */
bool IsCharacterPrintable(uint32_t charCode);

/**
 * Get keyboard state structure
 * @return Pointer to keyboard state
 */
KeyboardState* GetKeyboardState(void);

/**
 * Reset keyboard state
 */
void ResetKeyboardState(void);

/**
 * Get auto-repeat state
 * @return Pointer to auto-repeat state
 */
AutoRepeatState* GetAutoRepeatState(void);

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_EVENTS_H */