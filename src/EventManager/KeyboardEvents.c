#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
#include "EventManager/EventManagerInternal.h"
#include <stdlib.h>
#include <string.h>
/**
 * @file KeyboardEvents.c
 * @brief Keyboard Event Processing Implementation for System 7.1
 *
 * This file provides comprehensive keyboard event handling including
 * key presses, modifier keys, auto-repeat, international layouts,
 * dead key processing, and modern keyboard features.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>

#include "EventManager/KeyboardEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventStructs.h"
#include "EventManager/EventLogging.h"
#include "EventManager/AppSwitcher.h"
/* ctype.h not available in kernel - use simple implementations */
static inline int islower(int c) { return c >= 'a' && c <= 'z'; }
static inline int isupper(int c) { return c >= 'A' && c <= 'Z'; }
static inline int isalpha(int c) { return islower(c) || isupper(c); }
static inline int tolower(int c) { return isupper(c) ? c + 32 : c; }
static inline int toupper(int c) { return islower(c) ? c - 32 : c; }


/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* Dead key type constants */
enum {
    kDeadKeyNone = 0,
    kDeadKeyAcute = 1,
    kDeadKeyGrave = 2,
    kDeadKeyCircumflex = 3,
    kDeadKeyUmlaut = 4,
    kDeadKeyTilde = 5
};


/* Keyboard state */
static KeyboardState g_keyboardState = {0};
static AutoRepeatState g_autoRepeatState = {0};
static Boolean g_keyboardInitialized = false;

/* Keyboard layouts */
static KeyboardLayout* g_keyboardLayouts = NULL;
static KeyboardLayout* g_activeLayout = NULL;

/* Dead key state */
static DeadKeyState g_deadKeyState = {0};

/* Key translation state */
static KeyTransState g_globalTransState = {0};

/* Abort detection */
static Boolean g_abortPressed = false;

/* External references */
/* PostEvent declared in EventManager.h */
extern UInt32 TickCount(void);
extern void UpdateKeyboardState(const KeyMap newKeyMap);

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
    /* 0x70-0x7F: help, home, page up, forward delete, end, page down and the
     * arrows (left, right, down, up) carry their US layout codes; they were
     * 0, so no key event was posted and arrow keys did nothing anywhere. */
    0, 0, 0x05, 0x01, 0x0B, 0x7F, 0, 0x04, 0, 0x0C, 0, 0x1C, 0x1D, 0x1F, 0x1E, 0
};

/* Shifted character table */
static const UInt8 g_usShiftedTable[128] = {
    /* 0x00-0x0F */
    'A', 'S', 'D', 'F', 'H', 'G', 'Z', 'X', 'C', 'V', 0, 'B', 'Q', 'W', 'E', 'R',
    /* 0x10-0x1F */
    'Y', 'T', '!', '@', '#', '$', '^', '%', '+', '(', '&', '_', '*', ')', '}', 'O',
    /* 0x20-0x2F */
    'U', '{', 'I', 'P', 0x0D, 'L', 'J', '"', 'K', ':', '|', '<', '?', 'N', 'M', '>',
    /* 0x30-0x3F */
    0x09, ' ', '~', 0x08, 0, 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x40-0x7F as unshifted. The comment said so but the rows were not
     * there, so a shifted keypad or arrow key gave nothing - Shift-arrow
     * could not extend a selection. */
    0, '.', 0, '*', 0, '+', 0, 0, 0, 0, 0, '/', 0x03, 0, 0, '-',
    0, '=', '0', '1', '2', '3', '4', '5', '6', '7', 0, '8', '9', 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0x05, 0x01, 0x0B, 0x7F, 0, 0x04, 0, 0x0C, 0, 0x1C, 0x1D, 0x1F, 0x1E, 0
};

/* Dead key composition table */
typedef struct DeadKeyComposition {
    SInt16 deadKeyType;
    UInt32 baseChar;
    UInt32 composedChar;
} DeadKeyComposition;

static const DeadKeyComposition g_deadKeyTable[] = {
    /* Acute accent */
    {kDeadKeyAcute, 'a', 0xE1}, /* á */
    {kDeadKeyAcute, 'e', 0xE9}, /* é */
    {kDeadKeyAcute, 'i', 0xED}, /* í */
    {kDeadKeyAcute, 'o', 0xF3}, /* ó */
    {kDeadKeyAcute, 'u', 0xFA}, /* ú */
    {kDeadKeyAcute, 'A', 0xC1}, /* Á */
    {kDeadKeyAcute, 'E', 0xC9}, /* É */
    {kDeadKeyAcute, 'I', 0xCD}, /* Í */
    {kDeadKeyAcute, 'O', 0xD3}, /* Ó */
    {kDeadKeyAcute, 'U', 0xDA}, /* Ú */

    /* Grave accent */
    {kDeadKeyGrave, 'a', 0xE0}, /* à */
    {kDeadKeyGrave, 'e', 0xE8}, /* è */
    {kDeadKeyGrave, 'i', 0xEC}, /* ì */
    {kDeadKeyGrave, 'o', 0xF2}, /* ò */
    {kDeadKeyGrave, 'u', 0xF9}, /* ù */
    {kDeadKeyGrave, 'A', 0xC0}, /* À */
    {kDeadKeyGrave, 'E', 0xC8}, /* È */
    {kDeadKeyGrave, 'I', 0xCC}, /* Ì */
    {kDeadKeyGrave, 'O', 0xD2}, /* Ò */
    {kDeadKeyGrave, 'U', 0xD9}, /* Ù */

    /* Circumflex */
    {kDeadKeyCircumflex, 'a', 0xE2}, /* â */
    {kDeadKeyCircumflex, 'e', 0xEA}, /* ê */
    {kDeadKeyCircumflex, 'i', 0xEE}, /* î */
    {kDeadKeyCircumflex, 'o', 0xF4}, /* ô */
    {kDeadKeyCircumflex, 'u', 0xFB}, /* û */
    {kDeadKeyCircumflex, 'A', 0xC2}, /* Â */
    {kDeadKeyCircumflex, 'E', 0xCA}, /* Ê */
    {kDeadKeyCircumflex, 'I', 0xCE}, /* Î */
    {kDeadKeyCircumflex, 'O', 0xD4}, /* Ô */
    {kDeadKeyCircumflex, 'U', 0xDB}, /* Û */

    /* Umlaut */
    {kDeadKeyUmlaut, 'a', 0xE4}, /* ä */
    {kDeadKeyUmlaut, 'e', 0xEB}, /* ë */
    {kDeadKeyUmlaut, 'i', 0xEF}, /* ï */
    {kDeadKeyUmlaut, 'o', 0xF6}, /* ö */
    {kDeadKeyUmlaut, 'u', 0xFC}, /* ü */
    {kDeadKeyUmlaut, 'A', 0xC4}, /* Ä */
    {kDeadKeyUmlaut, 'E', 0xCB}, /* Ë */
    {kDeadKeyUmlaut, 'I', 0xCF}, /* Ï */
    {kDeadKeyUmlaut, 'O', 0xD6}, /* Ö */
    {kDeadKeyUmlaut, 'U', 0xDC}, /* Ü */

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
static void UpdateKeyMapForScanCode(UInt16 scanCode, Boolean isPressed);

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
    if (shifted) {
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
    UInt16 modifierBit = 0;

    switch (scanCode) {
        case kScanCommand:
            modifierBit = cmdKey;
            break;
        case kScanShift:
            modifierBit = shiftKey;
            break;
        case kScanCapsLock:
            modifierBit = alphaLock;
            if (isPressed) {
                /* Caps lock toggles */
                g_keyboardState.modifierState ^= modifierBit;
                g_keyboardState.capsLockState = !g_keyboardState.capsLockState;
                return;
            }
            break;
        case kScanOption:
            modifierBit = optionKey;
            break;
        case kScanControl:
            modifierBit = controlKey;
            break;
        case kScanRightShift:
            modifierBit = rightShiftKey;
            break;
        case kScanRightOption:
            modifierBit = rightOptionKey;
            break;
        case kScanRightControl:
            modifierBit = rightControlKey;
            break;
    }

    if (modifierBit) {
        if (isPressed) {
            g_keyboardState.modifierState |= modifierBit;
        } else {
            g_keyboardState.modifierState &= ~modifierBit;
        }
    }
}

/**
 * Update KeyMap for scan code
 */
static void UpdateKeyMapForScanCode(UInt16 scanCode, Boolean isPressed)
{
    if (scanCode >= 128) return;

    UInt16 arrayIndex = scanCode / 32;
    UInt16 bitIndex = scanCode % 32;

    if (arrayIndex < 4) {
        if (isPressed) {
            g_keyboardState.currentKeyMap[arrayIndex] |= (1U << bitIndex);
        } else {
            g_keyboardState.currentKeyMap[arrayIndex] &= ~(1U << bitIndex);
        }
    }

    /* Update global keymap */
    UpdateKeyboardState(g_keyboardState.currentKeyMap);
}

/*---------------------------------------------------------------------------
 * Auto-Repeat Management
 *---------------------------------------------------------------------------*/

/**
 * Check for auto-repeat timing
 */
static void CheckForAutoRepeat(void)
{
    if (!g_autoRepeatState.active || !g_autoRepeatState.enabled) {
        return;
    }

    UInt32 currentTime = TickCount();
    UInt32 elapsed = currentTime - g_autoRepeatState.lastRepeatTime;

    if (elapsed >= g_autoRepeatState.repeatRate) {
        /* Generate auto-repeat event */
        PostEvent(autoKey, g_autoRepeatState.charCode);
        g_autoRepeatState.lastRepeatTime = currentTime;
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
    g_autoRepeatState.lastRepeatTime = g_autoRepeatState.startTime + g_autoRepeatState.initialDelay;
    g_autoRepeatState.active = true;
}

/**
 * Stop current auto-repeat
 */
static void StopCurrentAutoRepeat(void)
{
    g_autoRepeatState.active = false;
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
    /* Simple mapping - in real implementation this would use KCHR resource */
    if (modifiers & optionKey) {
        switch (scanCode) {
            case 0x18: return kDeadKeyAcute;    /* Option+E */
            case 0x32: return kDeadKeyGrave;    /* Option+` */
            case 0x1A: return kDeadKeyCircumflex; /* Option+I */
            case 0x1C: return kDeadKeyUmlaut;   /* Option+U */
            case 0x1D: return kDeadKeyTilde;    /* Option+N */
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
    memset(&g_deadKeyState, 0, sizeof(DeadKeyState));

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

    /* Free keyboard layouts */
    KeyboardLayout* layout = g_keyboardLayouts;
    while (layout) {
        KeyboardLayout* next = (KeyboardLayout*)layout->keyMapData; /* Use keyMapData as next pointer */
        if (layout->keyMapData) {
            /* Note: keyMapData is Handle, would need proper disposal */
        }
        DisposePtr((Ptr)layout);
        layout = next;
    }
    g_keyboardLayouts = NULL;
    g_activeLayout = NULL;

    g_keyboardInitialized = false;
}

/**
 * Process raw keyboard event
 */
SInt16 ProcessRawKeyboardEvent(UInt16 scanCode, Boolean isKeyDown,
                               UInt16 modifiers, UInt32 timestamp)
{
    if (!g_keyboardInitialized) {
        return 0;
    }

    SInt16 eventsGenerated = 0;

    /* Update key map */
    UpdateKeyMapForScanCode(scanCode, isKeyDown);

    /* Handle modifier keys */
    if (IsModifierKey(scanCode)) {
        UpdateModifierState(scanCode, isKeyDown);
        /* Modifier keys don't generate key events in Mac OS */
        return 0;
    }

    if (isKeyDown) {
        /* Key pressed */
        UInt32 charCode = 0;

        /* Check for dead key */
        SInt16 deadKeyType = GetDeadKeyTypeForScanCode(scanCode, modifiers);
        if (deadKeyType != kDeadKeyNone) {
            g_deadKeyState.deadKeyType = deadKeyType;
            g_deadKeyState.deadKeyScanCode = scanCode;
            g_deadKeyState.deadKeyTime = timestamp;
            g_deadKeyState.waitingForNext = true;
            /* Dead keys don't generate immediate character events */
            return 0;
        }

        /* Translate to character */
        charCode = TranslateKeyToASCII(scanCode, modifiers);

        /* Handle dead key composition */
        if (g_deadKeyState.waitingForNext && charCode != 0) {
            UInt32 composedChar = LookupDeadKeyComposition(g_deadKeyState.deadKeyType, charCode);
            if (composedChar != charCode) {
                charCode = composedChar;
            }
            g_deadKeyState.waitingForNext = false;
            g_deadKeyState.deadKeyType = kDeadKeyNone;
        }

        /* Check for Command-Tab application switcher */
        if (scanCode == 0x30 && (modifiers & cmdKey)) { /* Tab key with Command */
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

        /* Generate key down event */
        SInt32 message = charCode | (scanCode << 8);
        PostEvent(keyDown, message);
        eventsGenerated++;

        /* Start auto-repeat */
        if (charCode != 0) {
            StartAutoRepeatForKey(scanCode, charCode);
        }

        /* Check for abort sequence (Cmd+Period) */
        if (scanCode == 0x2F && (modifiers & cmdKey)) { /* Period key */
            g_abortPressed = true;
        }

    } else {
        /* Key released */

        /* Check for Tab key release when command-tab switcher is active */
        if (scanCode == 0x30 && AppSwitcher_IsActive()) {
            AppSwitcher_HandleKeyUp();
            /* Don't post normal Tab keyUp event */
            return eventsGenerated;
        }

        /* Stop auto-repeat if this was the repeating key */
        if (g_autoRepeatState.active && g_autoRepeatState.keyCode == scanCode) {
            StopCurrentAutoRepeat();
        }

        /* Generate key up event */
        UInt32 charCode = TranslateKeyToASCII(scanCode, modifiers);
        SInt32 message = charCode | (scanCode << 8);
        PostEvent(keyUp, message);
        eventsGenerated++;

        /* Clear abort if Command or Period released */
        if (scanCode == kScanCommand || scanCode == 0x2F) {
            g_abortPressed = false;
        }
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
    if (scanCode >= 128) return false;

    UInt16 arrayIndex = scanCode / 32;
    UInt16 bitIndex = scanCode % 32;

    if (arrayIndex < 4) {
        return (g_keyboardState.currentKeyMap[arrayIndex] & (1U << bitIndex)) != 0;
    }

    return false;
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
 * Key translation using KCHR resource
 */
SInt32 KeyTranslate(const void* transData, UInt16 keyCode, UInt32* state)
{
    (void)transData;
    if (!state) {
        return 0;
    }

    /* Extract scan code and modifiers */
    UInt16 scanCode = keyCode & 0xFF;
    UInt16 modifiers = (keyCode >> 8) & 0xFF;

    /* For now, use simple ASCII translation */
    /* Real implementation would process KCHR resource */
    UInt32 charCode = TranslateKeyToASCII(scanCode, modifiers);

    /* Handle dead key state */
    if (g_deadKeyState.waitingForNext && charCode != 0) {
        UInt32 composedChar = LookupDeadKeyComposition(g_deadKeyState.deadKeyType, charCode);
        if (composedChar != charCode) {
            *state = 0; /* Reset state after composition */
            g_deadKeyState.waitingForNext = false;
            return composedChar;
        }
    }

    /* Check if this is a dead key */
    SInt16 deadKeyType = GetDeadKeyTypeForScanCode(scanCode, modifiers);
    if (deadKeyType != kDeadKeyNone) {
        *state = deadKeyType; /* Store dead key type in state */
        return 0; /* Dead key produces no immediate character */
    }

    return charCode;
}

/**
 * Translate scan code to character
 */
UInt32 TranslateScanCode(UInt16 scanCode, UInt16 modifiers, KeyTransState* transState)
{
    if (!transState) {
        transState = &g_globalTransState;
    }

    UInt32 state = transState->state;
    UInt16 keyCode = scanCode | (modifiers << 8);
    SInt32 result = KeyTranslate(NULL, keyCode, &state);
    transState->state = state;

    return (UInt32)result;
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
    SInt16 deadKeyType = GetDeadKeyTypeForScanCode(deadKeyCode, 0);
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
    memset(&g_deadKeyState, 0, sizeof(DeadKeyState));
}

/*---------------------------------------------------------------------------
 * Event Generation
 *---------------------------------------------------------------------------*/

/**
 * Generate key down event
 */
EventRecord GenerateKeyDownEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    EventRecord event = {0};

    event.what = keyDown;
    event.message = charCode | (scanCode << 8);
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = modifiers;

    return event;
}

/**
 * Generate key up event
 */
EventRecord GenerateKeyUpEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    EventRecord event = {0};

    event.what = keyUp;
    event.message = charCode | (scanCode << 8);
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = modifiers;

    return event;
}

/**
 * Generate auto-key event
 */
EventRecord GenerateAutoKeyEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers)
{
    EventRecord event = {0};

    event.what = autoKey;
    event.message = charCode | (scanCode << 8);
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = modifiers;

    return event;
}

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/* CheckAbort is implemented in EventManagerCore.c */

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
        case 0x24: keyName = "Return"; break;
        case 0x30: keyName = "Tab"; break;
        case 0x31: keyName = "Space"; break;
        case 0x33: keyName = "Delete"; break;
        case 0x35: keyName = "Escape"; break;
        case 0x37: keyName = "Command"; break;
        case 0x38: keyName = "Shift"; break;
        case 0x39: keyName = "Caps Lock"; break;
        case 0x3A: keyName = "Option"; break;
        case 0x3B: keyName = "Control"; break;
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
    return (charCode >= 32 && charCode <= 126) || (charCode >= 160 && charCode <= 255);
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
