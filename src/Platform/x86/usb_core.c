/*
 * usb_core.c - Minimal x86 USB core scaffolding
 *
 * This provides a shared registration layer for xHCI/EHCI/UHCI so we can
 * build HID support incrementally. Transfers are not implemented yet.
 */

#include "usb_core.h"
#include "Platform/include/serial.h"
#include <string.h>

#define MAX_USB_CONTROLLERS 4

static usb_controller_t g_usb_controllers[MAX_USB_CONTROLLERS];
static uint8_t g_usb_controller_count = 0;
static bool g_usb_core_inited = false;

bool usb_core_x86_init(void) {
    if (g_usb_core_inited) {
        return true;
    }
    memset(g_usb_controllers, 0, sizeof(g_usb_controllers));
    g_usb_controller_count = 0;
    g_usb_core_inited = true;
    serial_puts("[USB] x86 core initialized (scaffold)\n");
    return true;
}

bool usb_core_x86_register_controller(usb_controller_type_t type, uintptr_t base, bool mmio) {
    if (!g_usb_core_inited) {
        usb_core_x86_init();
    }
    if (g_usb_controller_count >= MAX_USB_CONTROLLERS) {
        return false;
    }
    usb_controller_t *c = &g_usb_controllers[g_usb_controller_count++];
    c->type = type;
    c->base = base;
    c->mmio = mmio;

    const char *name = (type == USB_CTRL_XHCI) ? "xHCI" :
                       (type == USB_CTRL_EHCI) ? "EHCI" : "UHCI";
    serial_printf("[USB] registered %s controller base=0x%08x (%s)\n",
                  name, (uint32_t)base, mmio ? "MMIO" : "PIO");
    return true;
}

bool usb_hid_poll_keyboard(char *out_char) {
    (void)out_char;
    return false;
}

bool usb_hid_poll_mouse(int16_t *dx, int16_t *dy, uint8_t *buttons) {
    (void)dx;
    (void)dy;
    (void)buttons;
    return false;
}
