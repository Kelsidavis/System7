/**
 * @file ModernInput.c
 * @brief Modern Input System Integration for Event Manager
 *
 * This file provides the bridge between modern input devices (PS/2, USB)
 * and the classic Mac OS Event Manager. It handles the translation of
 * hardware events into Mac-style EventRecords.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#include "SystemTypes.h"
#include "TimeManager/TimeBase.h"
#include "EventManager/EventManagerInternal.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventGlobals.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/KeyboardEvents.h"
#include "EventManager/EventLogging.h"
#include "Platform/PS2Input.h"
#include <string.h>

#if defined(__i386__) || defined(__x86_64__)
#include "Platform/x86/xhci.h"
#endif

/* QEMU PS/2 jitter tolerance: allows tiny grace for packet arrival delays */
#define QEMU_JITTER_HACK 1

/* Global input state */
static struct {
    Boolean initialized;
    const char* platform;
    UInt8 lastButtonState;
    KeyMap lastKeyMap;
    UInt32 lastClickTime;
    Point lastClickPos;
    UInt16 clickCount;
    Boolean capsLockLatched;
#if QEMU_JITTER_HACK
    UInt32 lastDownTick;  /* Tick of last mouseDown to coalesce jitter */
#endif
} g_modernInput = {0};

static Boolean KeyMapHasKey(const KeyMap map, UInt16 scanCode)
{
    if (scanCode >= 128) {
        return false;
    }

    UInt16 arrayIndex = scanCode / 32;
    UInt16 bitIndex = scanCode % 32;
    UInt32 word;
    memcpy(&word, map + arrayIndex * sizeof(word), sizeof(word));

    return (word & (1U << bitIndex)) != 0;
}

static void KeyMapSetKey(KeyMap map, UInt16 scanCode, Boolean isDown)
{
    if (scanCode >= 128) {
        return;
    }

    UInt16 arrayIndex = scanCode / 32;
    UInt32 mask = (1U << (scanCode % 32));
    UInt32 word;
    memcpy(&word, map + arrayIndex * sizeof(word), sizeof(word));

    if (isDown) {
        word |= mask;
    } else {
        word &= ~mask;
    }
    memcpy(map + arrayIndex * sizeof(word), &word, sizeof(word));
}

static UInt16 ComputeModifiersFromKeyMap(const KeyMap map, UInt8 buttonState)
{
    UInt16 mods = 0;

    if (buttonState & 1) {
        mods |= btnState;
    }

    if (KeyMapHasKey(map, kScanCommand)) {
        mods |= cmdKey;
    }
    /* Some keyboards report right command as 0x36 */
    if (KeyMapHasKey(map, 0x36)) {
        mods |= cmdKey;
    }

    if (KeyMapHasKey(map, kScanShift)) {
        mods |= shiftKey;
    }
    if (KeyMapHasKey(map, kScanRightShift)) {
        mods |= shiftKey | rightShiftKey;
    }

    if (KeyMapHasKey(map, kScanOption)) {
        mods |= optionKey;
    }
    if (KeyMapHasKey(map, kScanRightOption)) {
        mods |= optionKey | rightOptionKey;
    }

    if (KeyMapHasKey(map, kScanControl)) {
        mods |= controlKey;
    }
    if (KeyMapHasKey(map, kScanRightControl)) {
        mods |= controlKey | rightControlKey;
    }

    if (KeyMapHasKey(map, kScanCapsLock) || g_modernInput.capsLockLatched) {
        mods |= alphaLock;
    }

    return mods;
}

/**
 * Initialize modern input system
 * @param platform Platform identifier (X11, Cocoa, Win32, PS2, etc.)
 * @return Error code
 */
SInt16 InitModernInput(const char* platform)
{
    if (g_modernInput.initialized) {
        return noErr;
    }

    /* Initialize platform-specific input */
    if (platform && strcmp(platform, "PS2") == 0) {
        /* Initialize PS/2 controller for keyboard and mouse if needed */
        if (!PS2_IsInitialized()) {
            if (!InitPS2Controller()) {
                EVT_LOG_ERROR("ModernInput failed to initialize PS/2 controller\n");
                return -1;
            }
        }
        EVT_LOG_INFO("ModernInput PS/2 controller ready\n");
    }
    /* Add other platform initializations here (USB, etc.) */

    if (InitKeyboardEvents() != noErr) {
        EVT_LOG_ERROR("ModernInput failed to initialize keyboard events\n");
        return -1;
    }

    g_modernInput.platform = platform;

    /* Initialize state */
    g_modernInput.lastButtonState = 0;
    memset(g_modernInput.lastKeyMap, 0, sizeof(KeyMap));
    g_modernInput.lastClickTime = 0;
    g_modernInput.lastClickPos.h = 0;
    g_modernInput.lastClickPos.v = 0;
    g_modernInput.clickCount = 0;
    g_modernInput.capsLockLatched = false;

    g_modernInput.initialized = true;

    return noErr;
}

/**
 * EventPumpYield - Pump input events in modal loops
 * Call this in modal tracking loops (drag, resize, etc.) to ensure
 * mouse button transitions are not missed
 */
void EventPumpYield(void)
{
    ProcessModernInput();
}

/**
 * Process modern input events
 * Should be called regularly from main event loop
 */
void ProcessModernInput(void)
{
    Point currentMousePos;
    UInt8 currentButtonState;
    KeyMap currentKeyMap;

    if (!g_modernInput.initialized) {
        return;
    }

    /* USB HID devices too: only the main loop polled them, so with a USB
     * mouse every nested tracking loop saw the pointer and button frozen. */
#if defined(__i386__) || defined(__x86_64__)
    xhci_poll_hid_x86();
#endif

    /* Poll input devices unless IRQ-driven input is enabled */
    if (!PS2_IsIRQDriven()) {
        PollPS2Input();
    }

    GetMouse(&currentMousePos);
    /* Latched variant: a press that was also released between two polls would
     * be invisible to level comparison and the click lost entirely. */
    currentButtonState = GetMouseButtonsLatched();

    /* Update global button state for Button()/StillDown() */
    if (currentButtonState != gCurrentButtons) {
        EVT_LOG_TRACE("[MI] gCurrentButtons: 0x%02x -> 0x%02x\n",
                     gCurrentButtons, currentButtonState);
    }
    gCurrentButtons = currentButtonState;

    /* Get keyboard state from PS/2 controller */
    if (!GetPS2KeyboardState(currentKeyMap)) {
        /* If no keyboard state available, clear the map */
        memset(currentKeyMap, 0, sizeof(KeyMap));
    }

    /* Check for mouse button changes */
    if (currentButtonState != g_modernInput.lastButtonState) {
        UInt32 currentTime = TickCount();

        if ((currentButtonState & 1) && !(g_modernInput.lastButtonState & 1)) {
            /* Mouse button pressed - down transition */

#if QEMU_JITTER_HACK
            /* QEMU PS/2 jitter: coalesce rapid downs in same tick AND same position */
            if (currentTime == g_modernInput.lastDownTick &&
                currentMousePos.h == g_modernInput.lastClickPos.h &&
                currentMousePos.v == g_modernInput.lastClickPos.v) {
                /* Skip duplicate down event - already posted */
                g_modernInput.lastButtonState = currentButtonState;
                return;
            }
            g_modernInput.lastDownTick = currentTime;
#endif

            /* Check for multi-click using GetDblTime() and gDoubleClickSlop */
            UInt32 threshold = GetDblTime();

            SInt16 dx = currentMousePos.h - g_modernInput.lastClickPos.h;
            SInt16 dy = currentMousePos.v - g_modernInput.lastClickPos.v;

            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            /* Handle first-ever click (lastClickTime == 0) */
            if (g_modernInput.lastClickTime == 0) {
                /* First click since boot - always single click */
                g_modernInput.clickCount = 1;
            } else {
                UInt32 dt = currentTime - g_modernInput.lastClickTime;

#if QEMU_JITTER_HACK
                /* QEMU grace: allow ≤3 tick late arrival if within slop */
                const UInt32 kJitterGrace = 3;
                UInt32 effectiveThreshold = threshold + kJitterGrace;
#else
                UInt32 effectiveThreshold = threshold;
#endif

                if (dt <= effectiveThreshold &&
                    dx <= gDoubleClickSlop && dy <= gDoubleClickSlop) {
                    /* Within time and distance - increment click count (cap at 3) */
                    g_modernInput.clickCount = (g_modernInput.clickCount < 3)
                        ? (g_modernInput.clickCount + 1) : 3;
                } else {
                    /* Outside window - reset to single click */
                    g_modernInput.clickCount = 1;
                }
            }

            /* Generate mouseDown event with classic System 7 encoding:
             * message = (clickCount << 16) | (SInt16)partCode
             * High word: click count (1, 2, or 3)
             * Low word: part code (0 for desktop by default)
             */
            if (!gInMouseTracking) {
                SInt16 partCode = 0;  /* Desktop default; FindWindow may update later */
                SInt32 message = ((SInt32)g_modernInput.clickCount << 16) | (SInt16)partCode;
                PostEvent(mouseDown, message);
            }

            /* Update last click time and position */
            g_modernInput.lastClickTime = currentTime;
            g_modernInput.lastClickPos = currentMousePos;

        } else if (!(currentButtonState & 1) && (g_modernInput.lastButtonState & 1)) {
            /* Mouse button released - up transition */
            if (!gInMouseTracking) {
                /* mouseUp: same encoding as mouseDown - high word = click count */
                SInt16 partCode = 0;
                SInt32 message = ((SInt32)g_modernInput.clickCount << 16) | (SInt16)partCode;
                PostEvent(mouseUp, message);
            }
            /* Do NOT reset clickCount on mouseUp - next mouseDown decides based on time+slop */
        }

        g_modernInput.lastButtonState = currentButtonState;
    }

    /* Drain queued key transitions.
     *
     * This used to diff currentKeyMap against the previous snapshot. A key
     * pressed and released between two polls left no trace, and two such keys
     * could cancel out entirely - typing three characters into a dialog field
     * delivered one. The scancode handler now records every transition in
     * order and this drains the ring, so nothing depends on poll timing.
     * currentKeyMap is still maintained for GetKeys and for the modifier
     * state, which is a level, not an edge. */
    {
        UInt8 macCode;
        Boolean isPressed;
        KeyMap running;

        /* Replay the transitions against a running key map rather than reading
         * the modifier state off a single sample.
         *
         * currentKeyMap is one snapshot taken at the top of this function, but
         * the ring can hold a whole chord that opened and closed since the last
         * call. Command-N arrives as four transitions - command down, N down, N
         * up, command up - and by sampling time nothing is held, so the N would
         * be reported with no modifiers and the menu equivalent never fired.
         * Rebuilding the map as each transition is applied gives each key the
         * modifier state that was actually in effect when it was pressed. */
        memcpy(running, g_modernInput.lastKeyMap, sizeof(KeyMap));

        while (PS2_DequeueKeyTransition(&macCode, &isPressed)) {
            UInt16 keyCode = macCode;

            KeyMapSetKey(running, keyCode, isPressed);

            if (isPressed && keyCode == kScanCapsLock) {
                g_modernInput.capsLockLatched = !g_modernInput.capsLockLatched;
            }

            UInt16 modifiers = ComputeModifiersFromKeyMap(running, currentButtonState);
            UInt32 timestamp = TickCount();

            SInt16 eventsGenerated = ProcessRawKeyboardEvent(keyCode, isPressed, modifiers, timestamp);

            if (eventsGenerated == 0) {
                UInt32 charCode = GetKeyCharacter(keyCode, modifiers);
                if (charCode != 0 || keyCode == kScanReturn || keyCode == kScanSpace ||
                    keyCode == kScanTab || keyCode == kScanDelete) {
                    /* Event message layout: charCode in the low byte, key code
                     * in the next. Masking the char to 16 bits let it bleed
                     * into the key-code byte. */
                    SInt32 message = (SInt32)(charCode & 0xFF) | ((SInt32)(keyCode & 0xFF) << 8);
                    PostEventWithModifiers(isPressed ? keyDown : keyUp, message, modifiers);
                }
            }
        }

        memcpy(g_modernInput.lastKeyMap, running, sizeof(KeyMap));
    }
}

/**
 * Shutdown modern input system
 */
void ShutdownModernInput(void)
{
    if (!g_modernInput.initialized) {
        return;
    }

    /* Cleanup platform-specific resources */
    /* PS/2 doesn't need explicit cleanup */

    ShutdownKeyboardEvents();
    g_modernInput.initialized = false;
    g_modernInput.platform = NULL;
}

/**
 * Report unsupported modern input feature requests
 * @param multiTouch Enable multi-touch support
 * @param gestures Enable gesture recognition
 * @param accessibility Enable accessibility features
 */
void ConfigureModernInput(Boolean multiTouch, Boolean gestures, Boolean accessibility)
{
    if (multiTouch || gestures || accessibility) {
        EVT_LOG_INFO("ModernInput options unsupported (MultiTouch:%d Gestures:%d Accessibility:%d)\n",
                     multiTouch, gestures, accessibility);
    }
}

/**
 * Get modern input configuration status
 */
Boolean IsModernInputInitialized(void)
{
    return g_modernInput.initialized;
}

/**
 * Get current platform name
 */
const char* GetModernInputPlatform(void)
{
    return g_modernInput.platform ? g_modernInput.platform : "none";
}
