/*
 * HID Input Handler for USB Keyboard and Mouse
 * Processes USB HID reports and converts to system events
 * Integrates with EventManager for event dispatch
 *
 * References:
 *   - USB Device Class Definition for Human Interface Devices (HID)
 *   - USB HID Usage Tables
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "hid_input.h"
#include "xhci.h"

/* ===== HID Class Codes ===== */
#define HID_CLASS           0x03
#define HID_SUBCLASS_BOOT   0x01

/* ===== HID Protocol IDs ===== */
#define HID_PROTOCOL_KEYBOARD   1
#define HID_PROTOCOL_MOUSE      2

/* ===== USB HID Keyboard ===== */

/* USB HID Keyboard Report Format (8 bytes) */
typedef struct {
    uint8_t modifier;      /* Shift, Ctrl, Alt, etc. */
    uint8_t reserved;      /* Always 0 */
    uint8_t keycodes[6];   /* Up to 6 simultaneously pressed keys */
} hid_keyboard_report_t;

/* ===== USB HID Mouse ===== */

/* USB HID Mouse Report Format (3-4 bytes) */
typedef struct {
    uint8_t buttons;       /* Bit 0=left, 1=right, 2=middle */
    int8_t  x_delta;       /* Signed X movement */
    int8_t  y_delta;       /* Signed Y movement */
    int8_t  wheel;         /* Optional wheel (4th byte) */
} hid_mouse_report_t;

/* ===== HID Modifier Keys ===== */
#define HID_MOD_LEFT_CTRL   0x01
#define HID_MOD_LEFT_SHIFT  0x02
#define HID_MOD_LEFT_ALT    0x04
#define HID_MOD_LEFT_GUI    0x08
#define HID_MOD_RIGHT_CTRL  0x10
#define HID_MOD_RIGHT_SHIFT 0x20
#define HID_MOD_RIGHT_ALT   0x40
#define HID_MOD_RIGHT_GUI   0x80

/* ===== USB HID Keyboard Scancodes ===== */
/* Maps USB HID keycodes to Mac keycodes (rough mapping) */
static const uint8_t usb_to_mac_keycode[] = {
    0x00,  /* 0: Reserved */
    0x00,  /* 1: Keyboard Error Roll Over */
    0x00,  /* 2: Keyboard POST Fail */
    0x00,  /* 3: Keyboard Error Undefined */
    0x00,  /* 4: A (0x00 in Mac) */
    0x0B,  /* 5: B */
    0x08,  /* 6: C */
    0x02,  /* 7: D */
    0x0E,  /* 8: E */
    0x03,  /* 9: F */
    0x05,  /* 10: G */
    0x04,  /* 11: H */
    0x22,  /* 12: I */
    0x26,  /* 13: J */
    0x28,  /* 14: K */
    0x25,  /* 15: L */
    0x2E,  /* 16: M */
    0x2D,  /* 17: N */
    0x1F,  /* 18: O */
    0x23,  /* 19: P */
    0x0C,  /* 20: Q */
    0x0F,  /* 21: R */
    0x01,  /* 22: S */
    0x11,  /* 23: T */
    0x20,  /* 24: U */
    0x09,  /* 25: V */
    0x0D,  /* 26: W */
    0x07,  /* 27: X */
    0x10,  /* 28: Y */
    0x06,  /* 29: Z */
    0x12,  /* 30: 1 ! */
    0x13,  /* 31: 2 @ */
    0x14,  /* 32: 3 # */
    0x15,  /* 33: 4 $ */
    0x17,  /* 34: 5 % */
    0x16,  /* 35: 6 ^ */
    0x1A,  /* 36: 7 & */
    0x1C,  /* 37: 8 * */
    0x19,  /* 38: 9 ( */
    0x1D,  /* 39: 0 ) */
    0x24,  /* 40: Return */
    0x35,  /* 41: Escape */
    0x33,  /* 42: Backspace */
    0x30,  /* 43: Tab */
    0x31,  /* 44: Space */
    0x1B,  /* 45: - _ */
    0x18,  /* 46: = + */
    0x21,  /* 47: [ { */
    0x1E,  /* 48: ] } */
    0x2A,  /* 49: \ | */
    0x29,  /* 50: ; : */
    0x27,  /* 51: ' " */
    0x32,  /* 52: ` ~ */
    0x2B,  /* 53: , < */
    0x2F,  /* 54: . > */
    0x2C,  /* 55: / ? */
    0x39,  /* 56: Caps Lock */
};

/* Global input state */
static uint8_t last_keyboard_report[8] = {0};
static uint8_t last_mouse_report[4] = {0};
static int keyboard_attached = 0;
static int mouse_attached = 0;

/* Forward declarations */
extern int event_post_key(uint8_t keycode, uint8_t modifiers, int key_down);
extern int event_post_mouse(int16_t x, int16_t y, uint8_t buttons);

/*
 * Process HID keyboard report
 * Detects key presses/releases and posts events
 */
int hid_process_keyboard_report(const uint8_t *report, uint32_t report_len) {
    if (!report || report_len < 8 || !keyboard_attached) {
        return -1;
    }

    const hid_keyboard_report_t *kb_report = (const hid_keyboard_report_t *)report;
    hid_keyboard_report_t *last_report = (hid_keyboard_report_t *)last_keyboard_report;

    /* Check for modifier key changes */
    uint8_t mod_changed = kb_report->modifier ^ last_report->modifier;
    if (mod_changed) {
        /* TODO: Post modifier key events
         * For now, just log
         */
        Serial_Printf("[HID] Modifier: 0x%02x -> 0x%02x\n", last_report->modifier, kb_report->modifier);
    }

    /* Check for keycode changes
     * USB HID keyboard can report up to 6 simultaneous keys
     * 0 = no key pressed, 1 = reserved, 2+ = actual keycodes
     */

    /* Simple approach: check which keys are newly pressed */
    for (int i = 0; i < 6; i++) {
        uint8_t current_key = kb_report->keycodes[i];

        if (current_key != 0 && current_key != 1) {
            /* Check if this key was already pressed */
            int already_pressed = 0;
            for (int j = 0; j < 6; j++) {
                if (last_report->keycodes[j] == current_key) {
                    already_pressed = 1;
                    break;
                }
            }

            if (!already_pressed) {
                /* New key press */
                uint8_t mac_keycode = 0;
                if (current_key < sizeof(usb_to_mac_keycode)) {
                    mac_keycode = usb_to_mac_keycode[current_key];
                }

                Serial_Printf("[HID] Key pressed: USB 0x%02x -> Mac 0x%02x\n",
                            current_key, mac_keycode);

                /* Post key-down event */
                event_post_key(mac_keycode, kb_report->modifier, 1);
            }
        }
    }

    /* Check for key releases */
    for (int i = 0; i < 6; i++) {
        uint8_t last_key = last_report->keycodes[i];

        if (last_key != 0 && last_key != 1) {
            /* Check if this key is still pressed */
            int still_pressed = 0;
            for (int j = 0; j < 6; j++) {
                if (kb_report->keycodes[j] == last_key) {
                    still_pressed = 1;
                    break;
                }
            }

            if (!still_pressed) {
                /* Key release */
                uint8_t mac_keycode = 0;
                if (last_key < sizeof(usb_to_mac_keycode)) {
                    mac_keycode = usb_to_mac_keycode[last_key];
                }

                Serial_Printf("[HID] Key released: USB 0x%02x -> Mac 0x%02x\n",
                            last_key, mac_keycode);

                /* Post key-up event */
                event_post_key(mac_keycode, last_report->modifier, 0);
            }
        }
    }

    /* Save for next comparison */
    memcpy(last_keyboard_report, report, 8);

    return 0;
}

/*
 * Process HID mouse report
 * Detects movement and button changes
 */
int hid_process_mouse_report(const uint8_t *report, uint32_t report_len) {
    if (!report || report_len < 3 || !mouse_attached) {
        return -1;
    }

    const hid_mouse_report_t *mouse_report = (const hid_mouse_report_t *)report;
    hid_mouse_report_t *last_report = (hid_mouse_report_t *)last_mouse_report;

    /* Check for button changes */
    uint8_t button_changed = mouse_report->buttons ^ last_report->buttons;
    if (button_changed) {
        Serial_Printf("[HID] Mouse buttons: 0x%02x -> 0x%02x\n",
                     last_report->buttons, mouse_report->buttons);
    }

    /* Check for movement */
    if (mouse_report->x_delta != 0 || mouse_report->y_delta != 0) {
        Serial_Printf("[HID] Mouse movement: dx=%d, dy=%d\n",
                     mouse_report->x_delta, mouse_report->y_delta);

        /* Post mouse movement event
         * Note: Y is typically inverted on Mac (positive down)
         */
        event_post_mouse(mouse_report->x_delta, -mouse_report->y_delta, mouse_report->buttons);
    }

    /* Save for next comparison */
    memcpy(last_mouse_report, report, (report_len > 4) ? 4 : report_len);

    return 0;
}

/*
 * Attach USB keyboard
 */
int hid_attach_keyboard(void) {
    Serial_WriteString("[HID] USB keyboard attached\n");
    keyboard_attached = 1;
    memset(last_keyboard_report, 0, sizeof(last_keyboard_report));
    return 0;
}

/*
 * Detach USB keyboard
 */
void hid_detach_keyboard(void) {
    Serial_WriteString("[HID] USB keyboard detached\n");
    keyboard_attached = 0;
}

/*
 * Attach USB mouse
 */
int hid_attach_mouse(void) {
    Serial_WriteString("[HID] USB mouse attached\n");
    mouse_attached = 1;
    memset(last_mouse_report, 0, sizeof(last_mouse_report));
    return 0;
}

/*
 * Detach USB mouse
 */
void hid_detach_mouse(void) {
    Serial_WriteString("[HID] USB mouse detached\n");
    mouse_attached = 0;
}

/*
 * Check if keyboard is attached
 */
int hid_keyboard_attached(void) {
    return keyboard_attached;
}

/*
 * Check if mouse is attached
 */
int hid_mouse_attached(void) {
    return mouse_attached;
}
