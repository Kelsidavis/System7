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
#include "EventManager/EventManagerInternal.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventStructs.h"
#include "EventManager/EventGlobals.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/KeyboardEvents.h"
#include "EventManager/EventLogging.h"
#include "Platform/PS2Input.h"
#include "PS2Controller.h"
#include <string.h>

/* External functions */
/* PostEvent declared in EventManager.h */
extern UInt32 TickCount(void);

/* Tracking guard to suppress events during modal drag loops */
extern volatile Boolean gInMouseTracking;

/* QEMU PS/2 jitter tolerance: allows tiny grace for packet arrival delays */
#define QEMU_JITTER_HACK 1

/* Global mouse position from PS2Controller.c */
extern Point g_mousePos;

/* Global event manager state - simplified for kernel use */
static UInt8 g_mouseButtonState = 0;
static KeyMap g_keyMapState = {0};
static EventMgrGlobals g_eventGlobals = {0};

/**
 * Update mouse state (called by mouse input system)
 */
void UpdateMouseState(Point newPos, UInt8 buttonState)
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

/* GetDblTime() is now provided by EventGlobals.c */

/* Global input state */
static struct {
    Boolean initialized;
    const char* platform;
    Point lastMousePos;
    UInt8 lastButtonState;
    KeyMap lastKeyMap;
    UInt32 lastClickTime;
    Point lastClickPos;
    UInt16 clickCount;
    Boolean multiTouchEnabled;
    Boolean gesturesEnabled;
    Boolean accessibilityEnabled;
    Boolean capsLockLatched;
    UInt32 pollCounter;  /* PS/2 poll throttling */
#if QEMU_JITTER_HACK
    UInt32 lastDownTick;  /* Tick of last mouseDown to coalesce jitter */
    UInt16 coalescedPolls; /* Count of coalesced down events in same tick */
#endif
} g_modernInput = {
    false,
    NULL,
    {0, 0},
    0,
    {0},
    0,
    {0, 0},
    0,
    false,
    false,
    false,
    false,
    0
#if QEMU_JITTER_HACK
    , 0
    , 0
#endif
};

static Boolean KeyMapHasKey(const KeyMap map, UInt16 scanCode)
{
    if (scanCode >= 128) {
        return false;
    }

    const UInt32 *words = (const UInt32 *)map;
    UInt16 arrayIndex = scanCode / 32;
    UInt16 bitIndex = scanCode % 32;

    return (words[arrayIndex] & (1U << bitIndex)) != 0;
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

    g_modernInput.platform = platform;

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

    /* Initialize state */
    g_modernInput.lastMousePos.h = 400;
    g_modernInput.lastMousePos.v = 300;
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
    static int pumpCount = 0;
    if (++pumpCount % 200 == 0) {
        EVT_LOG_TRACE("[MI] EventPumpYield called %d times\n", pumpCount);
    }
    ProcessModernInput();
}

/**
 * Process modern input events
 * Should be called regularly from main event loop
 */
void ProcessModernInput(void)
{
    static int entryCount = 0;
    entryCount++;

    /* ALWAYS log first 5 calls and every 60th */
    if (entryCount <= 5 || entryCount % 60 == 0) {
        EVT_LOG_TRACE("[MI] ProcessModernInput entry #%d\n", entryCount);
    }

    Point currentMousePos;
    UInt8 currentButtonState;
    KeyMap currentKeyMap;

    if (!g_modernInput.initialized) {
        EVT_LOG_TRACE("[MI] not initialized, returning early\n");
        return;
    }

    /* Poll input devices unless IRQ-driven input is enabled */
    if (!PS2_IsIRQDriven()) {
        PollPS2Input();
    }

    /* Get current input state from hardware abstraction layer */
    /* For now, we'll use the global mouse state from PS2Controller */
    /* Use platform-independent GetMouseButtons() */
    extern uint8_t GetMouseButtons(void);

    currentMousePos = g_mousePos;  /* Use file-scope extern */
    currentButtonState = GetMouseButtons();

    /* DEBUG: Check if g_mousePos is actually being read */
    static int posReadCount = 0;
    if (++posReadCount % 60 == 1) {
        EVT_LOG_TRACE("[MI] g_mousePos read: (%d,%d)\n",
                     (int)g_mousePos.h, (int)g_mousePos.v);
    }

    /* Update global button state for Button()/StillDown() */
    extern volatile UInt8 gCurrentButtons;
    static int updateCount = 0;
    if (currentButtonState != gCurrentButtons) {
        EVT_LOG_TRACE("[MI] gCurrentButtons: 0x%02x -> 0x%02x (update #%d)\n",
                     gCurrentButtons, currentButtonState, ++updateCount);
    }
    gCurrentButtons = currentButtonState;

    /* Debug: Log button states on every call */
    static int pollCount = 0;
    pollCount++;
    if (pollCount % 60 == 0) {  /* Every ~1 second at 60Hz */
        EVT_LOG_TRACE("[MI] Poll #%d: curr=%d, last=%d\n",
                     pollCount, currentButtonState, g_modernInput.lastButtonState);
    }

    /* Get keyboard state from PS/2 controller */
    if (!GetPS2KeyboardState(currentKeyMap)) {
        /* If no keyboard state available, clear the map */
        memset(currentKeyMap, 0, sizeof(KeyMap));
    }

    /* Check for mouse movement */
    if (currentMousePos.h != g_modernInput.lastMousePos.h ||
        currentMousePos.v != g_modernInput.lastMousePos.v) {

        /* Update Event Manager mouse state */
        UpdateMouseState(currentMousePos, currentButtonState);

        g_modernInput.lastMousePos = currentMousePos;
    }

    /* Check for mouse button changes */
    if (currentButtonState != g_modernInput.lastButtonState) {
        extern void uart_flush(void);
        extern void serial_puts(const char*);
        /* Simple debug: log button state changes */
        if (currentButtonState & 0x01) {
            serial_puts("[CLICK] Button DOWN\n");
        } else {
            serial_puts("[CLICK] Button UP\n");
        }

        UInt32 currentTime = TickCount();

        if ((currentButtonState & 1) && !(g_modernInput.lastButtonState & 1)) {
            /* Mouse button pressed - down transition */
            extern volatile Boolean gInMouseTracking;

#if QEMU_JITTER_HACK
            /* QEMU PS/2 jitter: coalesce rapid downs in same tick AND same position */
            if (currentTime == g_modernInput.lastDownTick &&
                currentMousePos.h == g_modernInput.lastClickPos.h &&
                currentMousePos.v == g_modernInput.lastClickPos.v) {
                g_modernInput.coalescedPolls++;
                /* EVT_LOG_TRACE removed - can hang on ARM64 */
                /* Skip duplicate down event - already posted */
                g_modernInput.lastButtonState = currentButtonState;
                return;
            }
            g_modernInput.lastDownTick = currentTime;
            g_modernInput.coalescedPolls = 1;
#endif

            /* Check for multi-click using GetDblTime() and gDoubleClickSlop */
            UInt32 threshold = GetDblTime();

            /* Hardcoded slop value - gDoubleClickSlop global not working */
            const UInt16 kClickSlop = 6;

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

                if (dt <= effectiveThreshold && dx <= kClickSlop && dy <= kClickSlop) {
                    /* Within time and distance - increment click count (cap at 3) */
                    g_modernInput.clickCount = (g_modernInput.clickCount < 3)
                        ? (g_modernInput.clickCount + 1) : 3;
                } else {
                    /* Outside window - reset to single click */
                    g_modernInput.clickCount = 1;
                }
            }

            /* Update Event Manager mouse position BEFORE posting event */
            UpdateMouseState(currentMousePos, currentButtonState);

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
            /* Update position before posting event */
            UpdateMouseState(currentMousePos, currentButtonState);
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

    /* Check for keyboard state changes */
    if (memcmp(currentKeyMap, g_modernInput.lastKeyMap, sizeof(KeyMap)) != 0) {
        UInt16 newModifiers = ComputeModifiersFromKeyMap(currentKeyMap, currentButtonState);
        UInt16 oldModifiers = ComputeModifiersFromKeyMap(g_modernInput.lastKeyMap, g_modernInput.lastButtonState);

        for (int i = 0; i < 16; i++) {
            UInt8 oldByte = g_modernInput.lastKeyMap[i];
            UInt8 newByte = currentKeyMap[i];
            UInt8 changed = oldByte ^ newByte;

            if (changed) {
                for (int bit = 0; bit < 8; bit++) {
                    if (changed & (1 << bit)) {
                        UInt16 keyCode = (i * 8) + bit;
                        Boolean isPressed = (newByte & (1 << bit)) != 0;

                        if (isPressed && keyCode == kScanCapsLock) {
                            g_modernInput.capsLockLatched = !g_modernInput.capsLockLatched;
                            newModifiers = ComputeModifiersFromKeyMap(currentKeyMap, currentButtonState);
                        }

                        UInt16 modifiers = isPressed ? newModifiers : oldModifiers;
                        UInt32 timestamp = TickCount();

                        SInt16 eventsGenerated = ProcessRawKeyboardEvent(keyCode, isPressed, modifiers, timestamp);

                        if (eventsGenerated == 0) {
                            UInt32 charCode = GetKeyCharacter(keyCode, modifiers);
                            if (charCode != 0 || keyCode == kScanReturn || keyCode == kScanSpace ||
                                keyCode == kScanTab || keyCode == kScanDelete) {
                                SInt32 message = (charCode & 0xFFFF) | ((SInt32)keyCode << 8);
                                PostEvent(isPressed ? keyDown : keyUp, message);
                            }
                        }
                    }
                }
            }
        }

        memcpy(g_modernInput.lastKeyMap, currentKeyMap, sizeof(KeyMap));
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

    g_modernInput.initialized = false;
    g_modernInput.platform = NULL;
}

/**
 * Enable/disable modern input features
 * @param multiTouch Enable multi-touch support
 * @param gestures Enable gesture recognition
 * @param accessibility Enable accessibility features
 */
void ConfigureModernInput(Boolean multiTouch, Boolean gestures, Boolean accessibility)
{
    g_modernInput.multiTouchEnabled = multiTouch;
    g_modernInput.gesturesEnabled = gestures;
    g_modernInput.accessibilityEnabled = accessibility;

    /* Configure platform-specific features */
    if (g_modernInput.platform) {
        EVT_LOG_INFO("ModernInput features configured MultiTouch:%d Gestures:%d Accessibility:%d\n",
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
