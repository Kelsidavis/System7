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
#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventStructs.h"
#include "EventManager/EventGlobals.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/KeyboardEvents.h"
#include <string.h>

/* External functions */
extern SInt16 PostEvent(SInt16 eventNum, SInt32 eventMsg);
extern UInt32 TickCount(void);
extern void serial_printf(const char* fmt, ...);

/* Tracking guard to suppress events during modal drag loops */
extern volatile Boolean gInMouseTracking;

/* PS/2 Controller integration */
extern Boolean InitPS2Controller(void);
extern void PollPS2Input(void);

/* Global event manager state - simplified for kernel use */
static Point g_mousePos = {0, 0};
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
    UInt32 pollCounter;  /* PS/2 poll throttling */
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
    0
};

/**
 * Check if two points are within double-click distance
 * Uses global gDoubleClickSlop setting
 */
static Boolean PointsNearby(Point p1, Point p2)
{
    SInt16 dx = p1.h - p2.h;
    SInt16 dy = p1.v - p2.v;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    return (dx <= gDoubleClickSlop && dy <= gDoubleClickSlop);
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
        /* Initialize PS/2 controller for keyboard and mouse */
        if (!InitPS2Controller()) {
            serial_printf("ModernInput: Failed to initialize PS/2 controller\n");
            return -1;
        }
        serial_printf("ModernInput: PS/2 controller initialized\n");
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
        serial_printf("[EventPumpYield] Called %d times\n", pumpCount);
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
        serial_printf("[MI] ProcessModernInput ENTRY #%d\n", entryCount);
    }

    Point currentMousePos;
    UInt8 currentButtonState;
    KeyMap currentKeyMap;

    if (!g_modernInput.initialized) {
        serial_printf("[MI] NOT INITIALIZED, returning early!\n");
        return;
    }

    /* Poll PS/2 devices if that's our platform */
    if (g_modernInput.platform && strcmp(g_modernInput.platform, "PS2") == 0) {
        PollPS2Input();
    }

    /* Get current input state from hardware abstraction layer */
    /* For now, we'll use the global mouse state from PS2Controller */
    extern Point g_mousePos;
    extern struct {
        int16_t x;
        int16_t y;
        uint8_t buttons;
        uint8_t packet[3];
        uint8_t packet_index;
    } g_mouseState;

    currentMousePos = g_mousePos;
    currentButtonState = g_mouseState.buttons;

    /* Update global button state for Button()/StillDown() */
    extern volatile UInt8 gCurrentButtons;
    static int updateCount = 0;
    if (currentButtonState != gCurrentButtons) {
        serial_printf("[MI] gCurrentButtons: 0x%02x -> 0x%02x (update #%d)\n",
                     gCurrentButtons, currentButtonState, ++updateCount);
    }
    gCurrentButtons = currentButtonState;

    /* Debug: Log button states on every call */
    static int pollCount = 0;
    pollCount++;
    if (pollCount % 60 == 0) {  /* Every ~1 second at 60Hz */
        serial_printf("[MI] Poll #%d: curr=%d, last=%d\n",
                     pollCount, currentButtonState, g_modernInput.lastButtonState);
    }

    /* Get keyboard state from PS2Controller */
    extern Boolean GetPS2KeyboardState(KeyMap keyMap);
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
    static int btnChangeCount = 0;
    if (currentButtonState != g_modernInput.lastButtonState) {
        btnChangeCount++;
        serial_printf("[MI] Button change #%d: curr=%d, last=%d\n",
                     btnChangeCount, currentButtonState, g_modernInput.lastButtonState);

        UInt32 currentTime = TickCount();

        if ((currentButtonState & 1) && !(g_modernInput.lastButtonState & 1)) {
            /* Mouse button pressed - down transition */

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
                serial_printf("[MI] First click since boot\n");
            } else {
                UInt32 dt = currentTime - g_modernInput.lastClickTime;
                serial_printf("[MI] Click timing: dt=%u, thresh=%u, dx=%d, dy=%d, slop=%d\n",
                             (unsigned)dt, (unsigned)threshold, dx, dy, gDoubleClickSlop);
                if (dt <= threshold && dx <= gDoubleClickSlop && dy <= gDoubleClickSlop) {
                    /* Within time and distance - increment click count (cap at 3) */
                    g_modernInput.clickCount = (g_modernInput.clickCount < 3)
                        ? (g_modernInput.clickCount + 1) : 3;
                    serial_printf("[MI] Multi-click detected! count=%d\n", g_modernInput.clickCount);
                } else {
                    /* Outside window - reset to single click */
                    g_modernInput.clickCount = 1;
                    if (dt > threshold) {
                        serial_printf("[MI] Reset: dt=%u > thresh=%u\n", (unsigned)dt, (unsigned)threshold);
                    } else {
                        serial_printf("[MI] Reset: dx=%d or dy=%d > slop=%d\n", dx, dy, gDoubleClickSlop);
                    }
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
                serial_printf("[MI] PostEvent mouseDown: clickCount=%d, msg=0x%08x\n",
                             g_modernInput.clickCount, message);
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
        /* Update Event Manager keyboard state */
        UpdateKeyboardState(currentKeyMap);

        /* Detect key presses and releases */
        for (int i = 0; i < 16; i++) {
            UInt8 oldByte = g_modernInput.lastKeyMap[i];
            UInt8 newByte = currentKeyMap[i];
            UInt8 changed = oldByte ^ newByte;

            if (changed) {
                for (int bit = 0; bit < 8; bit++) {
                    if (changed & (1 << bit)) {
                        UInt16 keyCode = (i * 8) + bit;

                        if (newByte & (1 << bit)) {
                            /* Key pressed */
                            PostEvent(keyDown, keyCode);
                        } else {
                            /* Key released */
                            PostEvent(keyUp, keyCode);
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
        serial_printf("ModernInput: Features configured - MultiTouch:%d Gestures:%d Accessibility:%d\n",
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