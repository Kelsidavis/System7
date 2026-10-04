/*
 * Shared PS/2-compatible input stubs for ARM32 and PowerPC backends.
 */

#include <stdint.h>
#include <string.h>
#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventGlobals.h"
#include "Platform/PS2Input.h"

static Point g_mousePos = {.h = 400, .v = 300};
static UInt8 g_mouseButtons = 0;

static Boolean g_input_initialized = false;

int event_post_key(uint8_t keycode, uint8_t modifiers, int key_down);
int event_post_mouse(int16_t x_delta, int16_t y_delta, uint8_t buttons);

Boolean InitPS2Controller(void) {
    g_mouseButtons = 0;
    g_input_initialized = true;
    return true;
}

void PollPS2Input(void) {
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
}

void SetMouseButtons(UInt8 buttons) {
    g_mouseButtons = buttons;
    gCurrentButtons = buttons;
}

UInt8 GetMouseButtons(void) {
    return g_mouseButtons;
}

UInt8 GetMouseButtonsLatched(void) {
    return g_mouseButtons;
}

Boolean PS2_IsInitialized(void) {
    return g_input_initialized;
}

Boolean PS2_IsIRQDriven(void) {
    return false;
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
    SetMouseButtons(buttons);
    return 0;
}
