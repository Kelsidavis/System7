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
#else
/* USB HID driver (Raspberry Pi) */
#include "usb_hid.h"
#include "usb_core.h"
#include "dwc2.h"
#endif

/* Mouse state globals - referenced by EventManager and virtio_input.c */
/* MUST be volatile - modified by polling, read in tight loops */
volatile Point g_mousePos = {0, 0};
volatile uint8_t g_mouseState = 0;

/* Keyboard state */
static uint16_t g_modifiers = 0;
static bool g_input_initialized = false;
static bool g_virtio_input_available = false;
#ifndef QEMU_BUILD
static bool g_usb_hid_available = false;
#endif

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
#else
    if (g_usb_hid_available && usb_hid_keyboard_connected()) {
        /* Convert USB HID modifiers to Mac modifier format */
        uint8_t hid_mods = usb_hid_get_modifiers();
        uint16_t mac_mods = 0;

        if (hid_mods & (HID_MOD_LEFT_SHIFT | HID_MOD_RIGHT_SHIFT)) {
            mac_mods |= 0x0200;  /* shiftKey */
        }
        if (hid_mods & (HID_MOD_LEFT_CTRL | HID_MOD_RIGHT_CTRL)) {
            mac_mods |= 0x1000;  /* controlKey */
        }
        if (hid_mods & (HID_MOD_LEFT_ALT | HID_MOD_RIGHT_ALT)) {
            mac_mods |= 0x0800;  /* optionKey */
        }
        if (hid_mods & (HID_MOD_LEFT_GUI | HID_MOD_RIGHT_GUI)) {
            mac_mods |= 0x0100;  /* cmdKey */
        }
        return mac_mods;
    }
#endif
    return g_modifiers;
}

/*
 * Get PS2 keyboard state
 * Fills keyMap with current keyboard state, returns true on success
 * KeyMap is 128 bits (16 bytes) where each bit represents a Mac keycode
 */
Boolean GetPS2KeyboardState(KeyMap keyMap) {
    if (!keyMap) {
        return false;
    }
    /* Clear the keymap first */
    for (int i = 0; i < 4; i++) {
        ((uint32_t*)keyMap)[i] = 0;
    }
#ifdef QEMU_BUILD
    if (g_virtio_input_available) {
        /* Get keyboard state from VirtIO input driver */
        virtio_input_get_keyboard_state((uint8_t*)keyMap);
    }
#else
    if (g_usb_hid_available && usb_hid_keyboard_connected()) {
        /* Scan through USB HID keycodes 0x04-0x73 (common keys) */
        for (uint8_t usb_key = 0x04; usb_key < 0x74; usb_key++) {
            if (usb_hid_key_pressed(usb_key)) {
                uint8_t mac_key = usb_hid_to_mac_keycode(usb_key);
                if (mac_key != 0xFF && mac_key < 128) {
                    /* Set the bit in keyMap */
                    ((uint8_t*)keyMap)[mac_key >> 3] |= (1 << (mac_key & 7));
                }
            }
        }
    }
#endif
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
#else
    /* Initialize USB stack and HID driver for Raspberry Pi */
    g_usb_hid_available = false;

    if (usb_core_init()) {
        /* Wait for device connection */
        if (dwc2_port_connected()) {
            /* Reset port and enumerate device */
            if (dwc2_port_reset()) {
                int dev_addr = usb_enumerate_device();
                if (dev_addr > 0) {
                    /* Initialize HID subsystem and probe for HID devices */
                    usb_hid_init();
                    if (usb_hid_probe_device(dev_addr) > 0) {
                        g_usb_hid_available = true;
                    }
                }
            }
        }
    }
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
#else
    if (g_usb_hid_available) {
        /* Poll USB HID devices */
        usb_hid_poll();

        /* Update mouse state if mouse is connected */
        if (usb_hid_mouse_connected()) {
            int16_t mx, my;
            usb_hid_get_mouse_position(&mx, &my);
            g_mousePos.h = mx;
            g_mousePos.v = my;

            /* Convert USB HID buttons to Mac button format */
            uint8_t hid_buttons = usb_hid_get_mouse_buttons();
            g_mouseState = hid_buttons;  /* Same format: bit 0 = left */
        }
    }
#endif
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
    /* Poll input until button is released */
    while (g_mouseState & 0x01) {
        PollPS2Input();
    }
    return true;
}
