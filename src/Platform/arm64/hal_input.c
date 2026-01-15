/*
 * ARM64 Input HAL Implementation
 * Keyboard and mouse input handling for ARM64
 * Supports VirtIO input for QEMU and stubs for Raspberry Pi USB HID
 */

#include <stdint.h>
#include <stdbool.h>
#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "Platform/PS2Input.h"

/* VirtIO input driver (QEMU) */
#ifdef QEMU_BUILD
#include "virtio_input.h"
#endif

/* Mouse state globals - referenced by EventManager and virtio_input.c */
/* MUST be volatile - modified by polling, read in tight loops */
volatile Point g_mousePos = {0, 0};
volatile uint8_t g_mouseState = 0;

/* Keyboard state */
static uint16_t g_modifiers = 0;
static bool g_input_initialized = false;
static bool g_virtio_input_available = false;

/*
 * GetMouse - Get current mouse position
 * Used extensively by the GUI
 */
void GetMouse(Point *mouseLoc) {
    if (mouseLoc) {
        mouseLoc->h = g_mousePos.h;
        mouseLoc->v = g_mousePos.v;
    }
}

/*
 * Get PS2 keyboard modifiers
 * Returns modifier key state (shift, ctrl, alt, cmd)
 */
uint16_t GetPS2Modifiers(void) {
#ifdef QEMU_BUILD
    if (g_virtio_input_available) {
        return virtio_input_get_modifiers();
    }
#endif
    return g_modifiers;
}

/*
 * Get PS2 keyboard state
 * Fills keyMap with current keyboard state, returns true on success
 */
Boolean GetPS2KeyboardState(KeyMap keyMap) {
    if (!keyMap) {
        return false;
    }
    /* Clear the keymap - no keys pressed by default */
    for (int i = 0; i < 4; i++) {
        ((uint32_t*)keyMap)[i] = 0;
    }
    /* TODO: Fill with actual key state from virtio_input */
    return true;
}

/*
 * Initialize PS2 controller
 * On ARM64, we'll use different input methods (VirtIO for QEMU, USB HID for Pi)
 * Returns true on success
 */
Boolean InitPS2Controller(void) {
    g_mousePos.h = 320;  /* Center of 640-wide display */
    g_mousePos.v = 240;  /* Center of 480-high display */
    g_mouseState = 0;
    g_modifiers = 0;

#ifdef QEMU_BUILD
    /* Try to initialize VirtIO input */
    g_virtio_input_available = virtio_input_init();
#endif

    g_input_initialized = true;
    return true;
}

/*
 * Poll PS2 input
 * On ARM64, this polls VirtIO input (QEMU) or USB HID (Raspberry Pi)
 */
void PollPS2Input(void) {
#ifdef QEMU_BUILD
    if (g_virtio_input_available) {
        virtio_input_poll();
        return;
    }
#endif
    /* TODO: Implement USB HID polling for Raspberry Pi */
}

/*
 * Set mouse position
 * Called to update mouse position from input driver
 */
void SetMousePosition(int16_t x, int16_t y) {
    g_mousePos.h = x;
    g_mousePos.v = y;
}

/*
 * Set mouse button state
 * Called to update button state from input driver
 */
void SetMouseButtons(uint8_t buttons) {
    g_mouseState = buttons;
}

/*
 * Set modifier keys
 * Called to update modifier state from input driver
 */
void SetModifiers(uint16_t mods) {
    g_modifiers = mods;
}

/*
 * GetMouseButtons - Get raw mouse button state
 * Returns button state byte (bit 0 = left, bit 1 = right, bit 2 = middle)
 */
uint8_t GetMouseButtons(void) {
    return g_mouseState;
}

/*
 * Button - Check if mouse button is down
 * Returns true if button is pressed
 */
bool Button(void) {
    return (g_mouseState & 0x01) != 0;
}

/*
 * StillDown - Check if mouse button is still down
 * Returns true if button remains pressed
 */
bool StillDown(void) {
    return (g_mouseState & 0x01) != 0;
}

/*
 * WaitMouseUp - Wait for mouse button release
 * Returns true when button is released
 */
bool WaitMouseUp(void) {
    /* In a real implementation, this would poll and wait */
    return true;
}
