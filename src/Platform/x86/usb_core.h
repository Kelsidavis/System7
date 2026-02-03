#ifndef X86_USB_CORE_H
#define X86_USB_CORE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    USB_CTRL_XHCI = 1,
    USB_CTRL_EHCI = 2,
    USB_CTRL_UHCI = 3,
} usb_controller_type_t;

typedef struct {
    usb_controller_type_t type;
    uintptr_t base;
    bool mmio;
} usb_controller_t;

bool usb_core_x86_init(void);
bool usb_core_x86_register_controller(usb_controller_type_t type, uintptr_t base, bool mmio);

/* Phase-4 placeholders for HID support */
bool usb_hid_poll_keyboard(char *out_char);
bool usb_hid_poll_mouse(int16_t *dx, int16_t *dy, uint8_t *buttons);

#endif /* X86_USB_CORE_H */
