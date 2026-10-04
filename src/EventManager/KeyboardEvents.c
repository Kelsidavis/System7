/**
 * @file KeyboardEvents.c
 * @brief Keyboard Event Processing Implementation for System 7.1
 *
 * Handles key edges, modifiers, auto-repeat, and the built-in US layout's
 * Mac Roman characters and dead-key composition.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

#include "EventManager/KeyboardEvents.h"
#include "EventManager/KeyMap.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventStructs.h"
#include "EventManager/EventLogging.h"
#include "EventManager/AppSwitcher.h"
#include "TimeManager/TimeBase.h"
/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* Keyboard state */
static KeyboardState g_keyboardState = {0};
static AutoRepeatState g_autoRepeatState = {0};
static Boolean g_keyboardInitialized = false;

/* Live input and explicit translation streams have independent dead-key state. */
static UInt32 g_eventDeadKeyState;

/* Key translation state */
static KeyTransState g_globalTransState = {0};

/*---------------------------------------------------------------------------
 * Key Translation Tables
 *---------------------------------------------------------------------------*/

/* Simple ASCII translation table for US layout */
static const UInt8 g_usKeyTransTable[128] = {
    /* 0x00-0x0F */
    'a', 's', 'd', 'f', 'h', 'g', 'z', 'x', 'c', 'v', 0, 'b', 'q', 'w', 'e', 'r',
    /* 0x10-0x1F */
    'y', 't', '1', '2', '3', '4', '6', '5', '=', '9', '7', '-', '8', '0', ']', 'o',
    /* 0x20-0x2F */
    'u', '[', 'i', 'p', 0x0D, 'l', 'j', '\'', 'k', ';', '\\', ',', '/', 'n', 'm', '.',
    /* 0x30-0x3F */
    0x09, ' ', '`', 0x08, 0, 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x40-0x4F */
    0, '.', 0, '*', 0, '+', 0, 0, 0, 0, 0, '/', 0x03, 0, 0, '-',
    /* 0x50-0x5F */
    0, '=', '0', '1', '2', '3', '4', '5', '6', '7', 0, '8', '9', 0, 0, 0,
    /* 0x60-0x6F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x70-0x7F: navigation keys and arrows */
    0, 0, 0x05, 0x01, 0x0B, 0x7F, 0, 0x04, 0, 0x0C, 0, 0x1C, 0x1D, 0x1F, 0x1E, 0
};

/* Shifted character table */
static const UInt8 g_usShiftedTable[64] = {
    /* 0x00-0x0F */
    'A', 'S', 'D', 'F', 'H', 'G', 'Z', 'X', 'C', 'V', 0, 'B', 'Q', 'W', 'E', 'R',
    /* 0x10-0x1F */
    'Y', 'T', '!', '@', '#', '$', '^', '%', '+', '(', '&', '_', '*', ')', '}', 'O',
    /* 0x20-0x2F */
    'U', '{', 'I', 'P', 0x0D, 'L', 'J', '"', 'K', ':', '|', '<', '?', 'N', 'M', '>',
    /* 0x30-0x3F */
    0x09, ' ', '~', 0x08, 0, 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Dead key composition table */
typedef struct DeadKeyComposition {
    SInt16 deadKeyType;
    UInt32 baseChar;
    UInt32 composedChar;
} DeadKeyComposition;

/* Character bytes use Mac Roman, not Unicode/Latin-1 code points. */
static const DeadKeyComposition g_deadKeyTable[] = {
    /* Acute accent */
    {kDeadKeyAcute, 'a', 0x87}, /* á */
    {kDeadKeyAcute, 'e', 0x8E}, /* é */
    {kDeadKeyAcute, 'i', 0x92}, /* í */
    {kDeadKeyAcute, 'o', 0x97}, /* ó */
    {kDeadKeyAcute, 'u', 0x9C}, /* ú */
    {kDeadKeyAcute, 'A', 0xE7}, /* Á */
    {kDeadKeyAcute, 'E', 0x83}, /* É */
    {kDeadKeyAcute, 'I', 0xEA}, /* Í */
    {kDeadKeyAcute, 'O', 0xEE}, /* Ó */
    {kDeadKeyAcute, 'U', 0xF2}, /* Ú */

    /* Grave accent */
    {kDeadKeyGrave, 'a', 0x88}, /* à */
    {kDeadKeyGrave, 'e', 0x8F}, /* è */
    {kDeadKeyGrave, 'i', 0x93}, /* ì */
    {kDeadKeyGrave, 'o', 0x98}, /* ò */
    {kDeadKeyGrave, 'u', 0x9D}, /* ù */
    {kDeadKeyGrave, 'A', 0xCB}, /* À */
    {kDeadKeyGrave, 'E', 0xE9}, /* È */
    {kDeadKeyGrave, 'I', 0xED}, /* Ì */
    {kDeadKeyGrave, 'O', 0xF1}, /* Ò */
    {kDeadKeyGrave, 'U', 0xF4}, /* Ù */

    /* Circumflex */
    {kDeadKeyCircumflex, 'a', 0x89}, /* â */
    {kDeadKeyCircumflex, 'e', 0x90}, /* ê */
    {kDeadKeyCircumflex, 'i', 0x94}, /* î */
    {kDeadKeyCircumflex, 'o', 0x99}, /* ô */
    {kDeadKeyCircumflex, 'u', 0x9E}, /* û */
    {kDeadKeyCircumflex, 'A', 0xE5}, /* Â */
    {kDeadKeyCircumflex, 'E', 0xE6}, /* Ê */
    {kDeadKeyCircumflex, 'I', 0xEB}, /* Î */
    {kDeadKeyCircumflex, 'O', 0xEF}, /* Ô */
    {kDeadKeyCircumflex, 'U', 0xF3}, /* Û */

    /* Umlaut */
    {kDeadKeyUmlaut, 'a', 0x8A}, /* ä */
    {kDeadKeyUmlaut, 'e', 0x91}, /* ë */
    {kDeadKeyUmlaut, 'i', 0x95}, /* ï */
    {kDeadKeyUmlaut, 'o', 0x9A}, /* ö */
    {kDeadKeyUmlaut, 'u', 0x9F}, /* ü */
    {kDeadKeyUmlaut, 'y', 0xD8}, /* ÿ */
    {kDeadKeyUmlaut, 'A', 0x80}, /* Ä */
    {kDeadKeyUmlaut, 'E', 0xE8}, /* Ë */
    {kDeadKeyUmlaut, 'I', 0xEC}, /* Ï */
    {kDeadKeyUmlaut, 'O', 0x85}, /* Ö */
    {kDeadKeyUmlaut, 'U', 0x86}, /* Ü */
    {kDeadKeyUmlaut, 'Y', 0xD9}, /* Ÿ */

    {kDeadKeyTilde, 'a', 0x8B}, /* ã */
    {kDeadKeyTilde, 'n', 0x96}, /* ñ */
    {kDeadKeyTilde, 'o', 0x9B}, /* õ */
    {kDeadKeyTilde, 'A', 0xCC}, /* Ã */
    {kDeadKeyTilde, 'N', 0x84}, /* Ñ */
    {kDeadKeyTilde, 'O', 0xCD}, /* Õ */

    {kDeadKeyNone, 0, 0} /* Terminator */
};

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

static UInt32 TranslateKeyToASCII(UInt16 scanCode, UInt16 modifiers);
static Boolean IsModifierKey(UInt16 scanCode);
static void UpdateModifierState(UInt16 scanCode, Boolean isPressed);
static void CheckForAutoRepeat(void);
static void StartAutoRepeatForKey(UInt16 scanCode, UInt32 charCode);
static void StopCurrentAutoRepeat(void);
static SInt16 GetDeadKeyTypeForScanCode(UInt16 scanCode, UInt16 modifiers);
static UInt32 LookupDeadKeyComposition(SInt16 deadKeyType, UInt32 baseChar);
static UInt32 TranslateUSKey(UInt16 scanCode, UInt16 modifiers, Boolean isKeyUp, UInt32* state);

static UInt32 KeyMessage(UInt16 scanCode, UInt32 charCode)
{
    return (charCode & charCodeMask) | (((UInt32)scanCode << 8) & keyCodeMask);
}

/*---------------------------------------------------------------------------
 * Key Translation Functions
 *---------------------------------------------------------------------------*/

/**
 * Translate scan code to ASCII using simple lookup
 */
static UInt32 TranslateKeyToASCII(UInt16 scanCode, UInt16 modifiers)
{
    if (scanCode >= 128) {
        return 0; /* Invalid scan code */
    }

    Boolean shifted = (modifiers & shiftKey) != 0;
    Boolean capsLock = (modifiers & alphaLock) != 0;

    UInt32 baseChar;
    if (shifted && scanCode < sizeof(g_usShiftedTable)) {
        baseChar = g_usShiftedTable[scanCode];
    } else {
        baseChar = g_usKeyTransTable[scanCode];
    }

    /* Apply caps lock to letters */
    if (capsLock && isalpha(baseChar)) {
        if (shifted) {
            baseChar = tolower(baseChar);
        } else {
            baseChar = toupper(baseChar);
        }
    }

    return baseChar;
}

/**
 * Check if scan code is a modifier key
 */
static Boolean IsModifierKey(UInt16 scanCode)
{
    switch (scanCode) {
        case kScanCommand:
        case kScanRightCommand:
        case kScanShift:
        case kScanCapsLock:
        case kScanOption:
        case kScanControl:
        case kScanRightShift:
        case kScanRightOption:
        case kScanRightControl:
        case kScanFunction:
            return true;
        default:
            return false;
    }
}

/**
 * Update modifier state for key press/release
 */
static void UpdateModifierState(UInt16 scanCode, Boolean isPressed)
{
    if (scanCode == kScanCapsLock && isPressed) {
        g_keyboardState.capsLockState = !g_keyboardState.capsLockState;
    }
    g_keyboardState.modifierState = KeyMapModifiers(g_keyboardState.currentKeyMap) & ~alphaLock;
    if (g_keyboardState.capsLockState) g_keyboardState.modifierState |= alphaLock;
}

/*---------------------------------------------------------------------------
 * Auto-Repeat Management
 *---------------------------------------------------------------------------*/

/**
 * Check for auto-repeat timing
 */
static void CheckForAutoRepeat(void)
{
    if (!g_keyboardInitialized || !g_autoRepeatState.active || !g_autoRepeatState.enabled) {
        return;
    }

    UInt32 currentTime = TickCount();
    UInt32 elapsed = currentTime - g_autoRepeatState.lastRepeatTime;
    UInt32 delay = g_autoRepeatState.repeating
        ? g_autoRepeatState.repeatRate : g_autoRepeatState.initialDelay;

    if (elapsed >= delay) {
        UInt32 message = KeyMessage(g_autoRepeatState.keyCode, g_autoRepeatState.charCode);
        PostEvent(autoKey, message);
        g_autoRepeatState.lastRepeatTime = currentTime;
        g_autoRepeatState.repeating = true;
    }
}

/**
 * Start auto-repeat for a key
 */
static void StartAutoRepeatForKey(UInt16 scanCode, UInt32 charCode)
{
    if (!g_autoRepeatState.enabled || IsModifierKey(scanCode)) {
        return;
    }

    g_autoRepeatState.keyCode = scanCode;
    g_autoRepeatState.charCode = charCode;
    g_autoRepeatState.startTime = TickCount();
    g_autoRepeatState.lastRepeatTime = g_autoRepeatState.startTime;
    g_autoRepeatState.repeating = false;
    g_autoRepeatState.active = true;
}

/**
 * Stop current auto-repeat
 */
static void StopCurrentAutoRepeat(void)
{
    g_autoRepeatState.active = false;
    g_autoRepeatState.repeating = false;
    g_autoRepeatState.keyCode = 0;
    g_autoRepeatState.charCode = 0;
}

/*---------------------------------------------------------------------------
 * Dead Key Processing
 *---------------------------------------------------------------------------*/

/**
 * Get dead key type for scan code
 */
static SInt16 GetDeadKeyTypeForScanCode(UInt16 scanCode, UInt16 modifiers)
{
    if (scanCode < 128 && (modifiers & optionKey)) {
        switch (g_usKeyTransTable[scanCode]) {
            case 'e': return kDeadKeyAcute;
            case '`': return kDeadKeyGrave;
            case 'i': return kDeadKeyCircumflex;
            case 'u': return kDeadKeyUmlaut;
            case 'n': return kDeadKeyTilde;
        }
    }

    return kDeadKeyNone;
}

/**
 * Look up dead key composition
 */
static UInt32 LookupDeadKeyComposition(SInt16 deadKeyType, UInt32 baseChar)
{
    const DeadKeyComposition* comp = g_deadKeyTable;

    while (comp->deadKeyType != kDeadKeyNone) {
        if (comp->deadKeyType == deadKeyType && comp->baseChar == baseChar) {
            return comp->composedChar;
        }
        comp++;
    }

    return baseChar; /* No composition found */
}

static UInt32 DeadKeyAccent(SInt16 type)
{
    static const UInt8 accents[] = {0, 0xAB, '`', 0xF6, 0xAC, 0xF7};
    return type > kDeadKeyNone && type <= kDeadKeyTilde ? accents[type] : 0;
}

/* Returns one Mac Roman byte, or the preceding accent in bits 16-23 as well. */
static UInt32 TranslateUSKey(UInt16 scanCode, UInt16 modifiers, Boolean isKeyUp, UInt32* state)
{
    if (scanCode >= 128) return 0;
    UInt32 character = TranslateKeyToASCII(scanCode, modifiers);
    SInt16 deadKey = GetDeadKeyTypeForScanCode(scanCode, modifiers);
    if (isKeyUp) return deadKey ? 0 : character;

    if (*state > kDeadKeyTilde) *state = 0;
    if (*state && (character || deadKey)) {
        SInt16 previous = (SInt16)*state;
        *state = 0;
        UInt32 accent = DeadKeyAccent(previous);
        if (deadKey) {
            return deadKey == previous ? accent : (accent << 16) | DeadKeyAccent(deadKey);
        }
        if (character == ' ') return accent;
        UInt32 composed = LookupDeadKeyComposition(previous, character);
        return composed != character ? composed : (accent << 16) | character;
    }
    if (deadKey) {
        *state = (UInt32)deadKey;
        return 0;
    }
    return character;
}

/*---------------------------------------------------------------------------
 * Core Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize keyboard event system
 */
SInt16 InitKeyboardEvents(void)
{
    if (g_keyboardInitialized) {
        return noErr;
    }

    /* Initialize keyboard state */
    memset(&g_keyboardState, 0, sizeof(KeyboardState));
    memset(&g_autoRepeatState, 0, sizeof(AutoRepeatState));
    g_eventDeadKeyState = 0;

    /* Set default auto-repeat parameters */
    g_autoRepeatState.initialDelay = kDefaultKeyRepeatDelay;
    g_autoRepeatState.repeatRate = kDefaultKeyRepeatRate;
    g_autoRepeatState.enabled = true;

    /* Initialize translation state */
    memset(&g_globalTransState, 0, sizeof(KeyTransState));

    g_keyboardInitialized = true;
    return noErr;
}

/**
 * Shutdown keyboard event system
 */
void ShutdownKeyboardEvents(void)
{
    if (!g_keyboardInitialized) {
        return;
    }

    StopCurrentAutoRepeat();
    g_keyboardInitialized = false;
}

/**
 * Process raw keyboard event
 */
SInt16 ProcessRawKeyboardEvent(UInt16 scanCode, Boolean isKeyDown,
                               UInt16 modifiers, UInt32 timestamp)
{
    if (!g_keyboardInitialized || scanCode >= 128) {
        return 0;
    }

    SInt16 eventsGenerated = 0;

    /* Update key map */
    KeyMapSetKey(g_keyboardState.currentKeyMap, scanCode, isKeyDown);

    /* Handle modifier keys */
    if (IsModifierKey(scanCode)) {
        UpdateModifierState(scanCode, isKeyDown);
        /* Modifier keys don't generate key events in Mac OS */
        return 0;
    }

    if (isKeyDown) {
        /* Key pressed */
        /* Check for Command-Tab application switcher */
        if (scanCode == kScanTab && (modifiers & cmdKey)) {
            if (modifiers & shiftKey) {
                /* Shift-Command-Tab: cycle backward */
                AppSwitcher_CycleBackward();
            } else {
                /* Command-Tab: cycle forward */
                AppSwitcher_CycleForward();
            }
            /* Don't post the normal Tab event when Command is held */
            return eventsGenerated;
        }
        UInt32 charCode = TranslateUSKey(scanCode, modifiers, false, &g_eventDeadKeyState);
        if (charCode == 0 && GetDeadKeyTypeForScanCode(scanCode, modifiers)) {
            StopCurrentAutoRepeat();
            return 0;
        }
        if (charCode & 0x00FF0000U) {
            PostEventWithModifiers(keyDown, KeyMessage(scanCode, charCode >> 16), modifiers);
            eventsGenerated++;
        }

        /* Generate key down event */
        UInt32 message = KeyMessage(scanCode, charCode);
        PostEventWithModifiers(keyDown, message, modifiers);
        eventsGenerated++;

        /* Start auto-repeat */
        if (charCode != 0) {
            StartAutoRepeatForKey(scanCode, charCode & charCodeMask);
        }

    } else {
        /* Key released */

        /* Check for Tab key release when command-tab switcher is active */
        if (scanCode == kScanTab && AppSwitcher_IsActive()) {
            AppSwitcher_HandleKeyUp();
            /* Don't post normal Tab keyUp event */
            return eventsGenerated;
        }

        /* Stop auto-repeat if this was the repeating key */
        if (g_autoRepeatState.active && g_autoRepeatState.keyCode == scanCode) {
            StopCurrentAutoRepeat();
        }

        /* Generate key up event */
        UInt32 charCode = TranslateUSKey(scanCode, modifiers, true, &g_eventDeadKeyState);
        UInt32 message = KeyMessage(scanCode, charCode);
        PostEventWithModifiers(keyUp, message, modifiers);
        eventsGenerated++;
    }

    /* Update timing */
    g_keyboardState.lastEventTime = timestamp;

    return eventsGenerated;
}

/**
 * Get current keyboard state
 */
void GetKeys(KeyMap theKeys)
{
    if (theKeys) {
        memcpy(theKeys, g_keyboardState.currentKeyMap, sizeof(KeyMap));
    }
}

/**
 * Check if specific key is pressed
 */
Boolean IsKeyDown(UInt16 scanCode)
{
    return KeyMapHasKey(g_keyboardState.currentKeyMap, scanCode);
}

/**
 * Get current modifier key state
 */
UInt16 GetModifierState(void)
{
    return g_keyboardState.modifierState;
}

/**
 * Check if specific modifier is pressed
 */
Boolean IsModifierDown(UInt16 modifier)
{
    return (g_keyboardState.modifierState & modifier) != 0;
}

/*---------------------------------------------------------------------------
 * Key Translation API
 *---------------------------------------------------------------------------*/

/**
 * Built-in US translation; native KCHR resource parsing is not implemented.
 */
SInt32 KeyTranslate(const void* transData, UInt16 keyCode, UInt32* state)
{
    (void)transData;
    if (!state) {
        return 0;
    }

    return (SInt32)TranslateUSKey(keyCode & 0x7F, keyCode & 0xFF00,
                                  (keyCode & 0x80) != 0, state);
}

/**
 * Translate scan code to character
 */
UInt32 TranslateScanCode(UInt16 scanCode, UInt16 modifiers, KeyTransState* transState)
{
    if (scanCode >= 128) return 0;
    if (!transState) {
        transState = &g_globalTransState;
    }

    return TranslateUSKey(scanCode, modifiers, false, &transState->state);
}

/**
 * Get character for key combination
 */
UInt32 GetKeyCharacter(UInt16 scanCode, UInt16 modifiers)
{
    return TranslateKeyToASCII(scanCode, modifiers);
}

/**
 * Reset key translation state
 */
void ResetKeyTransState(KeyTransState* transState)
{
    if (transState) {
        memset(transState, 0, sizeof(KeyTransState));
    }
}

/*---------------------------------------------------------------------------
 * Auto-Repeat Management
 *---------------------------------------------------------------------------*/

/**
 * Initialize auto-repeat system
 */
void InitAutoRepeat(UInt32 initialDelay, UInt32 repeatRate)
{
    g_autoRepeatState.initialDelay = initialDelay;
    g_autoRepeatState.repeatRate = repeatRate;
    g_autoRepeatState.enabled = true;
}

/**
 * Set auto-repeat parameters
 */
void SetAutoRepeat(UInt32 initialDelay, UInt32 repeatRate)
{
    g_autoRepeatState.initialDelay = initialDelay;
    g_autoRepeatState.repeatRate = repeatRate;
}

/**
 * Get auto-repeat parameters
 */
void GetAutoRepeat(UInt32* initialDelay, UInt32* repeatRate)
{
    if (initialDelay) *initialDelay = g_autoRepeatState.initialDelay;
    if (repeatRate) *repeatRate = g_autoRepeatState.repeatRate;
}

/**
 * Enable or disable auto-repeat
 */
void SetAutoRepeatEnabled(Boolean enabled)
{
    g_autoRepeatState.enabled = enabled;
    if (!enabled) {
        StopCurrentAutoRepeat();
    }
}

/**
 * Check if auto-repeat is enabled
 */
Boolean IsAutoRepeatEnabled(void)
{
    return g_autoRepeatState.enabled;
}

/**
 * Process auto-repeat timing
 */
void ProcessAutoRepeat(void)
{
    CheckForAutoRepeat();
}

/**
 * Start auto-repeat for a key
 */
void StartAutoRepeat(UInt16 scanCode, UInt32 charCode)
{
    StartAutoRepeatForKey(scanCode, charCode);
}

/**
 * Stop auto-repeat
 */
void StopAutoRepeat(void)
{
    StopCurrentAutoRepeat();
}

/*---------------------------------------------------------------------------
 * International Input Support
 *---------------------------------------------------------------------------*/

/**
 * Process dead key input
 */
UInt32 ProcessDeadKey(UInt16 deadKeyCode, UInt32 nextChar)
{
    SInt16 deadKeyType = GetDeadKeyTypeForScanCode(deadKeyCode, optionKey);
    if (deadKeyType == kDeadKeyNone) {
        return nextChar;
    }

    return LookupDeadKeyComposition(deadKeyType, nextChar);
}

/**
 * Check if scan code is a dead key
 */
SInt16 GetDeadKeyType(UInt16 scanCode, UInt16 modifiers)
{
    return GetDeadKeyTypeForScanCode(scanCode, modifiers);
}

/**
 * Compose character with accent
 */
UInt32 ComposeCharacter(UInt32 baseChar, SInt16 accentType)
{
    return LookupDeadKeyComposition(accentType, baseChar);
}

/**
 * Reset dead key state
 */
void ResetDeadKeyState(void)
{
    g_eventDeadKeyState = 0;
    ResetKeyTransState(&g_globalTransState);
}

/*---------------------------------------------------------------------------
 * Event Generation
 *---------------------------------------------------------------------------*/

static EventRecord GenerateKeyboardEvent(EventMask what, UInt16 scanCode,
                                         UInt32 charCode, UInt16 modifiers)
{
    EventRecord event = {0};
    event.what = what;
    event.message = KeyMessage(scanCode, charCode);
    event.when = TickCount();
    GetMouse(&event.where);
    event.modifiers = modifiers;
    return event;
}

/**
 * Generate key down event
 */
EventRecord GenerateKeyDownEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    return GenerateKeyboardEvent(keyDown, scanCode, charCode, modifiers);
}

/**
 * Generate key up event
 */
EventRecord GenerateKeyUpEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    return GenerateKeyboardEvent(keyUp, scanCode, charCode, modifiers);
}

/**
 * Generate auto-key event
 */
EventRecord GenerateAutoKeyEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    return GenerateKeyboardEvent(autoKey, scanCode, charCode, modifiers);
}

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

Boolean CheckAbort(void)
{
    return IsModifierDown(cmdKey) && IsKeyDown(0x2F); /* Command-Period */
}

/**
 * Convert scan code to virtual key code
 */
UInt16 ScanCodeToVirtualKey(UInt16 scanCode)
{
    /* For Mac OS, scan codes are virtual key codes */
    return scanCode;
}

/**
 * Convert virtual key code to scan code
 */
UInt16 VirtualKeyToScanCode(UInt16 virtualKey)
{
    /* For Mac OS, virtual key codes are scan codes */
    return virtualKey;
}

/**
 * Get key name string
 */
SInt16 GetKeyName(UInt16 scanCode, UInt16 modifiers, char* buffer, SInt16 bufferSize)
{
    if (!buffer || bufferSize <= 0) {
        return 0;
    }

    /* Simple key name lookup */
    const char* keyName = "Unknown";

    switch (scanCode) {
        case kScanReturn: keyName = "Return"; break;
        case kScanTab: keyName = "Tab"; break;
        case kScanSpace: keyName = "Space"; break;
        case kScanDelete: keyName = "Delete"; break;
        case kScanEscape: keyName = "Escape"; break;
        case kScanCommand: keyName = "Command"; break;
        case kScanShift: keyName = "Shift"; break;
        case kScanCapsLock: keyName = "Caps Lock"; break;
        case kScanOption: keyName = "Option"; break;
        case kScanControl: keyName = "Control"; break;
        case kScanRightCommand: keyName = "Right Command"; break;
        case kScanRightShift: keyName = "Right Shift"; break;
        case kScanRightOption: keyName = "Right Option"; break;
        case kScanRightControl: keyName = "Right Control"; break;
        case kScanF1: keyName = "F1"; break;
        case kScanF2: keyName = "F2"; break;
        case kScanF3: keyName = "F3"; break;
        case kScanF4: keyName = "F4"; break;
        case kScanF5: keyName = "F5"; break;
        case kScanF6: keyName = "F6"; break;
        case kScanF7: keyName = "F7"; break;
        case kScanF8: keyName = "F8"; break;
        case kScanF9: keyName = "F9"; break;
        case kScanF10: keyName = "F10"; break;
        case kScanF11: keyName = "F11"; break;
        case kScanF12: keyName = "F12"; break;
        case kScanLeftArrow: keyName = "Left Arrow"; break;
        case kScanRightArrow: keyName = "Right Arrow"; break;
        case kScanUpArrow: keyName = "Up Arrow"; break;
        case kScanDownArrow: keyName = "Down Arrow"; break;
        default: {
            UInt32 charCode = TranslateKeyToASCII(scanCode, modifiers);
            if (charCode >= 32 && charCode <= 126) {
                if (bufferSize == 1) {
                    buffer[0] = '\0';
                    return 0;
                }
                buffer[0] = (char)charCode;
                buffer[1] = '\0';
                return 1;
            }
            break;
        }
    }

    SInt16 len = strlen(keyName);
    if (len >= bufferSize) {
        len = bufferSize - 1;
    }
    strncpy(buffer, keyName, len);
    buffer[len] = '\0';

    return len;
}

/**
 * Check if character is printable
 */
Boolean IsCharacterPrintable(UInt32 charCode)
{
    return charCode >= 32 && charCode <= 255 && charCode != 127;
}

/**
 * Get keyboard state structure
 */
KeyboardState* GetKeyboardState(void)
{
    return &g_keyboardState;
}

/**
 * Reset keyboard state
 */
void ResetKeyboardState(void)
{
    memset(&g_keyboardState, 0, sizeof(KeyboardState));
    StopCurrentAutoRepeat();
    ResetDeadKeyState();
}

/**
 * Get auto-repeat state
 */
AutoRepeatState* GetAutoRepeatState(void)
{
    return &g_autoRepeatState;
}
