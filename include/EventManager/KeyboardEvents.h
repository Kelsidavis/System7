/**
 * @file KeyboardEvents.h
 * @brief Keyboard Event Processing for System 7.1 Event Manager
 *
 * Keyboard edges, modifier state, auto-repeat, and built-in US translation.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef KEYBOARD_EVENTS_H
#define KEYBOARD_EVENTS_H

#include "SystemTypes.h"

#include "EventManager/EventStructs.h"

#include "EventTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * Core Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize keyboard event system
 * @return Error code (0 = success)
 */
SInt16 InitKeyboardEvents(void);

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
SInt16 ProcessRawKeyboardEvent(UInt16 scanCode, Boolean isKeyDown,
                               UInt16 modifiers, UInt32 timestamp);

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
Boolean IsKeyDown(UInt16 scanCode);

/**
 * Get current modifier key state
 * @return Modifier key bitmask
 */
UInt16 GetModifierState(void);

/**
 * Check if specific modifier is pressed
 * @param modifier Modifier flag to check
 * @return true if modifier is pressed
 */
Boolean IsModifierDown(UInt16 modifier);

/*---------------------------------------------------------------------------
 * Key Translation and Character Generation
 *---------------------------------------------------------------------------*/

/**
 * Built-in US translation using the KeyTranslate calling convention.
 * @param transData Reserved; native KCHR parsing is not implemented
 * @param keyCode Virtual key in bits 0-6, key-up in bit 7, modifiers in bits 8-15
 * @param state Caller-owned dead-key state, initially zero
 * @return Mac Roman byte in bits 0-7, optionally a preceding accent in bits 16-23
 */
SInt32 KeyTranslate(const void* transData, UInt16 keyCode, UInt32* state);

/* Legacy spelling retained for source compatibility. */
#define KeyTrans KeyTranslate

/**
 * Translate a virtual key using the built-in US layout
 * @param scanCode Hardware scan code
 * @param modifiers Modifier key state
 * @param transState Translation state
 * @return Character code (0 if no character)
 */
UInt32 TranslateScanCode(UInt16 scanCode, UInt16 modifiers, KeyTransState* transState);

/**
 * Get character for key combination
 * @param scanCode Scan code
 * @param modifiers Modifier state
 * @return Character code
 */
UInt32 GetKeyCharacter(UInt16 scanCode, UInt16 modifiers);

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
void InitAutoRepeat(UInt32 initialDelay, UInt32 repeatRate);

/**
 * Set auto-repeat parameters
 * @param initialDelay Delay before repeat starts
 * @param repeatRate Rate of repeat
 */
void SetAutoRepeat(UInt32 initialDelay, UInt32 repeatRate);

/**
 * Get auto-repeat parameters
 * @param initialDelay Pointer to receive initial delay
 * @param repeatRate Pointer to receive repeat rate
 */
void GetAutoRepeat(UInt32* initialDelay, UInt32* repeatRate);

/**
 * Enable or disable auto-repeat
 * @param enabled true to enable auto-repeat
 */
void SetAutoRepeatEnabled(Boolean enabled);

/**
 * Check if auto-repeat is enabled
 * @return true if auto-repeat is enabled
 */
Boolean IsAutoRepeatEnabled(void);

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
void StartAutoRepeat(UInt16 scanCode, UInt32 charCode);

/**
 * Stop auto-repeat
 */
void StopAutoRepeat(void);

/*---------------------------------------------------------------------------
 * International Input Support
 *---------------------------------------------------------------------------*/

enum {
    kDeadKeyNone = 0,
    kDeadKeyAcute = 1,
    kDeadKeyGrave = 2,
    kDeadKeyCircumflex = 3,
    kDeadKeyUmlaut = 4,
    kDeadKeyTilde = 5
};

/**
 * Process dead key input
 * @param deadKeyCode Dead key scan code
 * @param nextChar Next character input
 * @return Composed Mac Roman byte, or nextChar if no composition exists
 */
UInt32 ProcessDeadKey(UInt16 deadKeyCode, UInt32 nextChar);

/**
 * Check if scan code is a dead key
 * @param scanCode Scan code to check
 * @param modifiers Modifier state
 * @return Dead key type or kDeadKeyNone
 */
SInt16 GetDeadKeyType(UInt16 scanCode, UInt16 modifiers);

/**
 * Compose character with accent
 * @param baseChar Base character
 * @param accentType Accent type
 * @return Composed character or base character
 */
UInt32 ComposeCharacter(UInt32 baseChar, SInt16 accentType);

/**
 * Reset dead key state
 */
void ResetDeadKeyState(void);

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
EventRecord GenerateKeyDownEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers);

/**
 * Generate key up event
 * @param scanCode Key scan code
 * @param charCode Character code
 * @param modifiers Modifier state
 * @return Generated event
 */
EventRecord GenerateKeyUpEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers);

/**
 * Generate auto-key event
 * @param scanCode Key scan code
 * @param charCode Character code
 * @param modifiers Modifier state
 * @return Generated event
 */
EventRecord GenerateAutoKeyEvent(UInt16 scanCode, UInt32 charCode, UInt16 modifiers);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Check for Command-Period abort sequence
 * @return true if Cmd-Period was pressed
 */
Boolean CheckAbort(void);

/**
 * Convert scan code to virtual key code
 * @param scanCode Hardware scan code
 * @return Virtual key code
 */
UInt16 ScanCodeToVirtualKey(UInt16 scanCode);

/**
 * Convert virtual key code to scan code
 * @param virtualKey Virtual key code
 * @return Hardware scan code
 */
UInt16 VirtualKeyToScanCode(UInt16 virtualKey);

/**
 * Get key name string
 * @param scanCode Scan code
 * @param modifiers Modifier state
 * @param buffer Buffer for key name
 * @param bufferSize Size of buffer
 * @return Length of key name
 */
SInt16 GetKeyName(UInt16 scanCode, UInt16 modifiers, char* buffer, SInt16 bufferSize);

/**
 * Check if character is printable
 * @param charCode Character code to check
 * @return true if character is printable
 */
Boolean IsCharacterPrintable(UInt32 charCode);

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
