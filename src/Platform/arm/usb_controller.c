/*
 * USB Controller Abstraction Implementation
 * Unified driver for XHCI (Pi 4/5) and DWCOTG (Pi 3)
 */

#include <stdint.h>
#include <stddef.h>
#include "System71StdLib.h"
#include "usb_controller.h"

/* Forward declarations */
extern int xhci_init(void);
extern int xhci_enumerate_devices(void);
extern int xhci_find_keyboard(void *kb_info);
extern int xhci_find_mouse(void *mouse_info);
extern int xhci_poll_keyboard(uint8_t *key_code, uint8_t *modifiers);
extern int xhci_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons);
extern uint32_t xhci_device_count(void);
extern void xhci_shutdown(void);

extern int dwcotg_init(void);
extern int dwcotg_enumerate_devices(void);
extern int dwcotg_find_keyboard(void *kb_info);
extern int dwcotg_find_mouse(void *mouse_info);
extern int dwcotg_poll_keyboard(uint8_t *key_code, uint8_t *modifiers);
extern int dwcotg_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons);
extern uint32_t dwcotg_device_count(void);
extern void dwcotg_shutdown(void);

/* Global state */
static rpi_model_t detected_model = PI_MODEL_UNKNOWN;
static int controller_initialized = 0;

/* Function pointers for abstraction */
static int (*usb_init_fn)(void) = NULL;
static int (*usb_enumerate_fn)(void) = NULL;
static int (*usb_find_kb_fn)(void *) = NULL;
static int (*usb_find_mouse_fn)(void *) = NULL;
static int (*usb_poll_kb_fn)(uint8_t *, uint8_t *) = NULL;
static int (*usb_poll_mouse_fn)(int8_t *, int8_t *, uint8_t *) = NULL;
static uint32_t (*usb_device_count_fn)(void) = NULL;
static void (*usb_shutdown_fn)(void) = NULL;

/*
 * Detect Raspberry Pi model from hardware
 * Uses device tree or hardware registers
 */
rpi_model_t usb_detect_rpi_model(void) {
    if (detected_model != PI_MODEL_UNKNOWN) {
        /* Already detected */
        return detected_model;
    }

    Serial_WriteString("[USB] Detecting Raspberry Pi model...\n");

    /* TODO: Read from device tree (/proc/device-tree/model)
     * or use hardware revision register
     *
     * For now, use heuristic: check for XHCI vs DWCOTG presence
     */

    /* Try reading XHCI registers (Pi 4/5) */
    volatile uint32_t *xhci_status = (volatile uint32_t *)0x00000000;  /* Placeholder */

    /* Try reading DWCOTG registers (Pi 3) */
    volatile uint32_t *dwcotg_hwcfg = (volatile uint32_t *)0x20980000;
    uint32_t dwcotg_val = *(volatile uint32_t *)dwcotg_hwcfg;

    if (dwcotg_val != 0 && dwcotg_val != 0xFFFFFFFF) {
        /* Looks like Pi 3 with DWCOTG */
        detected_model = PI_MODEL_3;
        Serial_WriteString("[USB] Detected: Raspberry Pi 3 (DWCOTG)\n");
        return PI_MODEL_3;
    }

    /* Default to Pi 4 (XHCI) if detection is uncertain */
    detected_model = PI_MODEL_4;
    Serial_WriteString("[USB] Detected: Raspberry Pi 4 or 5 (XHCI)\n");
    return PI_MODEL_4;
}

/*
 * Initialize USB controller
 * Selects appropriate driver based on Pi model
 */
int usb_controller_init(void) {
    Serial_WriteString("[USB] Initializing USB controller abstraction\n");

    rpi_model_t model = usb_detect_rpi_model();

    switch (model) {
        case PI_MODEL_3:
            Serial_WriteString("[USB] Using DWCOTG driver for Pi 3\n");
            usb_init_fn = dwcotg_init;
            usb_enumerate_fn = dwcotg_enumerate_devices;
            usb_find_kb_fn = dwcotg_find_keyboard;
            usb_find_mouse_fn = dwcotg_find_mouse;
            usb_poll_kb_fn = dwcotg_poll_keyboard;
            usb_poll_mouse_fn = dwcotg_poll_mouse;
            usb_device_count_fn = dwcotg_device_count;
            usb_shutdown_fn = dwcotg_shutdown;
            break;

        case PI_MODEL_4:
        case PI_MODEL_5:
            Serial_WriteString("[USB] Using XHCI driver for Pi 4/5\n");
            usb_init_fn = xhci_init;
            usb_enumerate_fn = xhci_enumerate_devices;
            usb_find_kb_fn = xhci_find_keyboard;
            usb_find_mouse_fn = xhci_find_mouse;
            usb_poll_kb_fn = xhci_poll_keyboard;
            usb_poll_mouse_fn = xhci_poll_mouse;
            usb_device_count_fn = xhci_device_count;
            usb_shutdown_fn = xhci_shutdown;
            break;

        default:
            Serial_WriteString("[USB] Error: Unknown Pi model\n");
            return -1;
    }

    /* Initialize selected controller */
    if (usb_init_fn() != 0) {
        Serial_WriteString("[USB] Controller initialization failed\n");
        return -1;
    }

    controller_initialized = 1;
    Serial_WriteString("[USB] Controller initialization complete\n");
    return 0;
}

/*
 * Enumerate USB devices
 */
int usb_controller_enumerate(void) {
    if (!controller_initialized || !usb_enumerate_fn) {
        return -1;
    }

    return usb_enumerate_fn();
}

/*
 * Find HID keyboard
 */
int usb_find_keyboard(void *kb_info) {
    if (!controller_initialized || !usb_find_kb_fn) {
        return -1;
    }

    return usb_find_kb_fn(kb_info);
}

/*
 * Find HID mouse
 */
int usb_find_mouse(void *mouse_info) {
    if (!controller_initialized || !usb_find_mouse_fn) {
        return -1;
    }

    return usb_find_mouse_fn(mouse_info);
}

/*
 * Poll keyboard
 */
int usb_poll_keyboard(uint8_t *key_code, uint8_t *modifiers) {
    if (!controller_initialized || !usb_poll_kb_fn) {
        return -1;
    }

    return usb_poll_kb_fn(key_code, modifiers);
}

/*
 * Poll mouse
 */
int usb_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons) {
    if (!controller_initialized || !usb_poll_mouse_fn) {
        return -1;
    }

    return usb_poll_mouse_fn(dx, dy, buttons);
}

/*
 * Get device count
 */
uint32_t usb_device_count(void) {
    if (!controller_initialized || !usb_device_count_fn) {
        return 0;
    }

    return usb_device_count_fn();
}

/*
 * Shutdown USB controller
 */
void usb_controller_shutdown(void) {
    if (!controller_initialized || !usb_shutdown_fn) {
        return;
    }

    usb_shutdown_fn();
    controller_initialized = 0;
}
