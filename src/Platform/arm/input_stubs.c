/*
 * ARM Input Stubs
 * Provides minimal implementations of the PS/2 input API so that
 * shared event manager code can build on platforms that rely on USB HID.
 * TODO: Plumb these through to the real USB HID stack once it is ready.
 */

#include <stdint.h>
#include <string.h>
#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventGlobals.h"
#include "Platform/PS2Input.h"
#include "QuickDraw/QuickDraw.h"

/* Shared state expected by ModernInput.c */
Point g_mousePos = { 400, 300 };
struct {
    int16_t x;
    int16_t y;
    uint8_t buttons;
    uint8_t packet[3];
    uint8_t packet_index;
} g_mouseState = {0};
static Boolean g_input_initialized = false;

int event_post_key(uint8_t keycode, uint8_t modifiers, int key_down);
int event_post_mouse(int16_t x_delta, int16_t y_delta, uint8_t buttons);

Boolean InitPS2Controller(void) {
    /* ARM platforms use USB HID for input; return success for now. */
    g_mouseState.x = g_mousePos.h;
    g_mouseState.y = g_mousePos.v;
    g_mouseState.buttons = 0;
    g_mouseState.packet_index = 0;
    memset(g_mouseState.packet, 0, sizeof(g_mouseState.packet));
    g_input_initialized = true;
    return true;
}

void PollPS2Input(void) {
    /* USB HID pipeline will update the globals directly in the future. */
}

void GetMouse(Point* mouseLoc) {
    if (mouseLoc) {
        mouseLoc->h = g_mousePos.h;
        mouseLoc->v = g_mousePos.v;
    }
}

UInt16 GetPS2Modifiers(void) {
    return 0;
}

Boolean GetPS2KeyboardState(KeyMap keyMap) {
    if (keyMap) {
        memset(keyMap, 0, sizeof(KeyMap));
    }
    return true;
}

void SetMousePosition(SInt16 x, SInt16 y) {
    g_mousePos.h = x;
    g_mousePos.v = y;
    g_mouseState.x = x;
    g_mouseState.y = y;
}

void SetMouseButtons(UInt8 buttons) {
    g_mouseState.buttons = buttons;
    gCurrentButtons = buttons;
}

UInt8 GetMouseButtons(void) {
    return g_mouseState.buttons;
}

void GetMouseLocal(Point* mouseLoc) {
    if (!mouseLoc) return;
    GetMouse(mouseLoc);
    if (g_currentPort) {
        mouseLoc->h -= g_currentPort->portBits.bounds.left;
        mouseLoc->v -= g_currentPort->portBits.bounds.top;
    }
}

Boolean PS2_IsInitialized(void) {
    return g_input_initialized;
}

Boolean PS2_IsIRQDriven(void) {
    return false;
}

UInt8 GetMouseButtonsLatched(void) {
    return g_mouseState.buttons;
}

Boolean PS2_DequeueKeyTransition(UInt8* macCode, Boolean* isPressed) {
    (void)macCode;
    (void)isPressed;
    return false;
}

void SetModifiers(UInt16 mods) {
    (void)mods;
}

int event_post_key(uint8_t keycode, uint8_t modifiers, int key_down) {
    (void)keycode;
    (void)modifiers;
    (void)key_down;
    return 0;
}

int event_post_mouse(int16_t x_delta, int16_t y_delta, uint8_t buttons) {
    g_mousePos.h += x_delta;
    g_mousePos.v += y_delta;
    g_mouseState.x = g_mousePos.h;
    g_mouseState.y = g_mousePos.v;
    g_mouseState.buttons = buttons;
    gCurrentButtons = buttons;
    return 0;
}
