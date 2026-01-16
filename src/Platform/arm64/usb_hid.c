/*
 * USB HID Driver - Keyboard and Mouse support
 * For Raspberry Pi ARM64
 */

#include "usb_hid.h"
#include "usb_core.h"
#include "dwc2.h"
#include "display.h"
#include "timer.h"
#include "uart.h"
#include <string.h>

/* HID device registry */
static hid_device_t hid_devices[HID_MAX_DEVICES];
static int keyboard_idx = -1;
static int mouse_idx = -1;
static bool hid_initialized = false;

/* USB HID keycode to Mac ADB keycode translation table */
/* Index = USB HID keycode, Value = Mac ADB keycode (or 0xFF if unmapped) */
static const uint8_t usb_to_mac_keycode[256] = {
    /* 0x00-0x03: Reserved/Error */
    0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x04-0x1D: Letters A-Z */
    0x00, 0x0B, 0x08, 0x02, 0x0E, 0x03, 0x05, 0x04,  /* A-H */
    0x22, 0x26, 0x28, 0x25, 0x2E, 0x2D, 0x1F, 0x23,  /* I-P */
    0x0C, 0x0F, 0x01, 0x11, 0x20, 0x09, 0x0D, 0x07,  /* Q-X */
    0x10, 0x06,                                       /* Y-Z */
    /* 0x1E-0x27: Numbers 1-0 */
    0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C,  /* 1-8 */
    0x19, 0x1D,                                       /* 9-0 */
    /* 0x28-0x2C: Enter, Escape, Backspace, Tab, Space */
    0x24, 0x35, 0x33, 0x30, 0x31,
    /* 0x2D-0x38: Punctuation and symbols */
    0x1B, 0x18, 0x21, 0x1E, 0x27,  /* - = [ ] \ */
    0x2A, 0x29, 0x2B, 0x2F, 0x2C,  /* ; ' ` , . */
    0x2C,                          /* / */
    /* 0x39: Caps Lock */
    0x39,
    /* 0x3A-0x45: F1-F12 */
    0x7A, 0x78, 0x63, 0x76, 0x60, 0x61, 0x62, 0x64,  /* F1-F8 */
    0x65, 0x6D, 0x67, 0x6F,                           /* F9-F12 */
    /* 0x46-0x48: Print Screen, Scroll Lock, Pause */
    0xFF, 0xFF, 0xFF,
    /* 0x49-0x4C: Insert, Home, Page Up, Delete */
    0x72, 0x73, 0x74, 0x75,
    /* 0x4D-0x4F: End, Page Down, Right Arrow */
    0x77, 0x79, 0x7C,
    /* 0x50-0x52: Left Arrow, Down Arrow, Up Arrow */
    0x7B, 0x7D, 0x7E,
    /* 0x53: Num Lock */
    0x47,
    /* 0x54-0x63: Keypad */
    0x4B, 0x43, 0x4E, 0x45, 0x4C, 0x53, 0x54, 0x55,  /* / * - + Enter 1 2 3 */
    0x56, 0x57, 0x58, 0x59, 0x5B, 0x5C, 0x41, 0x52,  /* 4 5 6 7 8 9 0 . */
    /* 0x64-0x67: Non-US \, Application, Power, Keypad = */
    0xFF, 0xFF, 0xFF, 0x51,
    /* 0x68-0x73: F13-F24 */
    0x69, 0x6B, 0x71, 0x6A, 0x40, 0x4F, 0x50, 0x5A,
    0xFF, 0xFF, 0xFF, 0xFF,
    /* Fill the rest with unmapped */
    [0x74 ... 0xDF] = 0xFF,
    /* 0xE0-0xE7: Modifier keys */
    0x3B, 0x38, 0x3A, 0x37,  /* Left Ctrl, Shift, Alt, GUI */
    0x3E, 0x3C, 0x3D, 0x36,  /* Right Ctrl, Shift, Alt, GUI */
    /* Rest unmapped */
    [0xE8 ... 0xFF] = 0xFF,
};

/*
 * Initialize USB HID subsystem
 */
bool usb_hid_init(void) {
    if (hid_initialized) {
        return true;
    }

    memset(hid_devices, 0, sizeof(hid_devices));
    keyboard_idx = -1;
    mouse_idx = -1;

    hid_initialized = true;
    uart_puts("[HID] USB HID initialized\n");
    return true;
}

/*
 * Set HID protocol (boot or report)
 */
static int hid_set_protocol(uint8_t dev_addr, uint8_t interface, uint8_t protocol) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        .bRequest = HID_REQ_SET_PROTOCOL,
        .wValue = protocol,
        .wIndex = interface,
        .wLength = 0
    };

    return usb_control_transfer(dev_addr, &setup, NULL, 0);
}

/*
 * Set HID idle rate (0 = only report on change)
 */
static int hid_set_idle(uint8_t dev_addr, uint8_t interface, uint8_t duration, uint8_t report_id) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        .bRequest = HID_REQ_SET_IDLE,
        .wValue = (duration << 8) | report_id,
        .wIndex = interface,
        .wLength = 0
    };

    return usb_control_transfer(dev_addr, &setup, NULL, 0);
}

/*
 * Parse configuration descriptor to find HID interfaces
 */
static int hid_parse_config(uint8_t dev_addr, uint8_t *config_data, uint16_t config_len) {
    uint8_t *ptr = config_data;
    uint8_t *end = config_data + config_len;
    int found = 0;

    usb_interface_desc_t *current_interface = NULL;
    hid_device_t *hid = NULL;

    while (ptr < end) {
        uint8_t desc_len = ptr[0];
        uint8_t desc_type = ptr[1];

        if (desc_len == 0) break;
        if (ptr + desc_len > end) break;

        if (desc_type == USB_DESC_TYPE_INTERFACE && desc_len >= 9) {
            current_interface = (usb_interface_desc_t *)ptr;

            /* Check for HID class with boot subclass */
            if (current_interface->bInterfaceClass == USB_CLASS_HID &&
                current_interface->bInterfaceSubClass == HID_SUBCLASS_BOOT) {

                /* Find free HID slot */
                int slot = -1;
                for (int i = 0; i < HID_MAX_DEVICES; i++) {
                    if (!hid_devices[i].configured) {
                        slot = i;
                        break;
                    }
                }

                if (slot >= 0) {
                    hid = &hid_devices[slot];
                    memset(hid, 0, sizeof(hid_device_t));
                    hid->dev_addr = dev_addr;
                    hid->interface_num = current_interface->bInterfaceNumber;
                    hid->protocol = current_interface->bInterfaceProtocol;

                    if (hid->protocol == HID_BOOT_PROTOCOL_KEYBOARD) {
                        uart_puts("[HID] Found keyboard interface\n");
                        if (keyboard_idx < 0) keyboard_idx = slot;
                    } else if (hid->protocol == HID_BOOT_PROTOCOL_MOUSE) {
                        uart_puts("[HID] Found mouse interface\n");
                        if (mouse_idx < 0) mouse_idx = slot;
                    }
                } else {
                    hid = NULL;
                }
            } else {
                hid = NULL;
            }
        } else if (desc_type == USB_DESC_TYPE_ENDPOINT && desc_len >= 7 && hid != NULL) {
            usb_endpoint_desc_t *ep = (usb_endpoint_desc_t *)ptr;

            /* We want interrupt IN endpoints */
            if ((ep->bmAttributes & 0x03) == USB_EP_TYPE_INTERRUPT &&
                (ep->bEndpointAddress & 0x80)) {  /* IN endpoint */

                hid->ep_addr = ep->bEndpointAddress;
                hid->ep_max_packet = ep->wMaxPacketSize;
                hid->ep_interval = ep->bInterval;
                hid->configured = true;
                found++;

                uart_puts("[HID] Endpoint: 0x");
                char hex[3];
                hex[0] = "0123456789ABCDEF"[(ep->bEndpointAddress >> 4) & 0xF];
                hex[1] = "0123456789ABCDEF"[ep->bEndpointAddress & 0xF];
                hex[2] = '\0';
                uart_puts(hex);
                uart_puts("\n");

                hid = NULL;  /* Done with this interface */
            }
        }

        ptr += desc_len;
    }

    return found;
}

/*
 * Probe a USB device for HID interfaces
 */
int usb_hid_probe_device(uint8_t dev_addr) {
    usb_device_t *dev = usb_get_device(dev_addr);
    if (!dev || !dev->configured) {
        return -1;
    }

    uart_puts("[HID] Probing device at address ");
    char num[4];
    num[0] = '0' + dev_addr;
    num[1] = '\n';
    num[2] = '\0';
    uart_puts(num);

    /* Get configuration descriptor */
    uint8_t config_buf[256];
    int ret;

    /* First get header to find total length */
    ret = usb_get_config_descriptor(dev_addr, config_buf, 9);
    if (ret != 0) {
        uart_puts("[HID] Failed to get config descriptor\n");
        return -1;
    }

    usb_config_desc_t *cfg = (usb_config_desc_t *)config_buf;
    uint16_t total_len = cfg->wTotalLength;
    if (total_len > sizeof(config_buf)) {
        total_len = sizeof(config_buf);
    }

    /* Get full configuration */
    ret = usb_get_config_descriptor(dev_addr, config_buf, total_len);
    if (ret != 0) {
        return -1;
    }

    /* Parse for HID interfaces */
    int found = hid_parse_config(dev_addr, config_buf, total_len);

    /* Configure found HID devices */
    for (int i = 0; i < HID_MAX_DEVICES; i++) {
        if (hid_devices[i].configured && hid_devices[i].dev_addr == dev_addr) {
            /* Set boot protocol */
            hid_set_protocol(dev_addr, hid_devices[i].interface_num, HID_PROTOCOL_BOOT);

            /* Set idle rate to 0 (report only on change) for keyboard */
            if (hid_devices[i].protocol == HID_BOOT_PROTOCOL_KEYBOARD) {
                hid_set_idle(dev_addr, hid_devices[i].interface_num, 0, 0);
            }
        }
    }

    return found;
}

/*
 * Poll a single HID device
 */
static void hid_poll_device(hid_device_t *hid) {
    if (!hid->configured) return;

    uint8_t report[8];
    int ret;

    ret = usb_interrupt_transfer(hid->dev_addr, hid->ep_addr,
                                  report, sizeof(report),
                                  hid->ep_max_packet, &hid->data_toggle);

    if (ret > 0) {
        if (hid->protocol == HID_BOOT_PROTOCOL_KEYBOARD && ret >= 8) {
            /* Save previous report for key transition detection */
            memcpy(&hid->kbd_prev_report, &hid->kbd_report, sizeof(hid_keyboard_report_t));
            memcpy(&hid->kbd_report, report, sizeof(hid_keyboard_report_t));
        } else if (hid->protocol == HID_BOOT_PROTOCOL_MOUSE && ret >= 3) {
            memcpy(&hid->mouse_report, report, sizeof(hid_mouse_report_t));

            /* Accumulate mouse position */
            hid->mouse_x += hid->mouse_report.x_delta;
            hid->mouse_y += hid->mouse_report.y_delta;

            /* Clamp to display bounds */
            int16_t max_x = (int16_t)display_get_width();
            int16_t max_y = (int16_t)display_get_height();
            if (max_x == 0) max_x = 640;  /* Default if display not init */
            if (max_y == 0) max_y = 480;

            if (hid->mouse_x < 0) hid->mouse_x = 0;
            if (hid->mouse_y < 0) hid->mouse_y = 0;
            if (hid->mouse_x >= max_x) hid->mouse_x = max_x - 1;
            if (hid->mouse_y >= max_y) hid->mouse_y = max_y - 1;
        }
    }
}

/*
 * Poll all HID devices
 */
void usb_hid_poll(void) {
    if (!hid_initialized) return;

    for (int i = 0; i < HID_MAX_DEVICES; i++) {
        if (hid_devices[i].configured) {
            hid_poll_device(&hid_devices[i]);
        }
    }
}

/*
 * Check if keyboard is connected
 */
bool usb_hid_keyboard_connected(void) {
    return keyboard_idx >= 0 && hid_devices[keyboard_idx].configured;
}

/*
 * Get current modifier state
 */
uint8_t usb_hid_get_modifiers(void) {
    if (keyboard_idx < 0) return 0;
    return hid_devices[keyboard_idx].kbd_report.modifiers;
}

/*
 * Check if a specific USB keycode is currently pressed
 */
bool usb_hid_key_pressed(uint8_t usb_keycode) {
    if (keyboard_idx < 0) return false;

    hid_keyboard_report_t *report = &hid_devices[keyboard_idx].kbd_report;

    for (int i = 0; i < 6; i++) {
        if (report->keys[i] == usb_keycode) {
            return true;
        }
    }
    return false;
}

/*
 * Get first newly pressed key (not in previous report)
 */
uint8_t usb_hid_get_key(void) {
    if (keyboard_idx < 0) return 0;

    hid_device_t *hid = &hid_devices[keyboard_idx];
    hid_keyboard_report_t *curr = &hid->kbd_report;
    hid_keyboard_report_t *prev = &hid->kbd_prev_report;

    /* Find a key in current report that wasn't in previous */
    for (int i = 0; i < 6; i++) {
        uint8_t key = curr->keys[i];
        if (key == 0) continue;

        /* Check if this key was in previous report */
        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (prev->keys[j] == key) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed) {
            return key;
        }
    }

    return 0;
}

/*
 * Check if mouse is connected
 */
bool usb_hid_mouse_connected(void) {
    return mouse_idx >= 0 && hid_devices[mouse_idx].configured;
}

/*
 * Get mouse button state
 */
uint8_t usb_hid_get_mouse_buttons(void) {
    if (mouse_idx < 0) return 0;
    return hid_devices[mouse_idx].mouse_report.buttons;
}

/*
 * Get accumulated mouse position
 */
void usb_hid_get_mouse_position(int16_t *x, int16_t *y) {
    if (mouse_idx < 0) {
        *x = 0;
        *y = 0;
        return;
    }

    *x = hid_devices[mouse_idx].mouse_x;
    *y = hid_devices[mouse_idx].mouse_y;
}

/*
 * Get mouse delta since last call
 */
void usb_hid_get_mouse_delta(int8_t *dx, int8_t *dy) {
    if (mouse_idx < 0) {
        *dx = 0;
        *dy = 0;
        return;
    }

    *dx = hid_devices[mouse_idx].mouse_report.x_delta;
    *dy = hid_devices[mouse_idx].mouse_report.y_delta;
}

/*
 * Convert USB HID keycode to Mac ADB keycode
 */
uint8_t usb_hid_to_mac_keycode(uint8_t usb_keycode) {
    return usb_to_mac_keycode[usb_keycode];
}
