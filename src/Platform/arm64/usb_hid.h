/*
 * USB HID Driver - Keyboard and Mouse support
 * For Raspberry Pi ARM64
 */

#ifndef ARM64_USB_HID_H
#define ARM64_USB_HID_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_core.h"

/* HID Class-specific requests */
#define HID_REQ_GET_REPORT      0x01
#define HID_REQ_GET_IDLE        0x02
#define HID_REQ_GET_PROTOCOL    0x03
#define HID_REQ_SET_REPORT      0x09
#define HID_REQ_SET_IDLE        0x0A
#define HID_REQ_SET_PROTOCOL    0x0B

/* HID Protocol values */
#define HID_PROTOCOL_BOOT       0
#define HID_PROTOCOL_REPORT     1

/* HID Subclass */
#define HID_SUBCLASS_NONE       0
#define HID_SUBCLASS_BOOT       1

/* HID Boot Protocol types */
#define HID_BOOT_PROTOCOL_NONE      0
#define HID_BOOT_PROTOCOL_KEYBOARD  1
#define HID_BOOT_PROTOCOL_MOUSE     2

/* Boot protocol keyboard report (8 bytes) */
typedef struct {
    uint8_t modifiers;      /* Modifier keys (ctrl, shift, alt, gui) */
    uint8_t reserved;       /* Reserved byte */
    uint8_t keys[6];        /* Up to 6 simultaneous key codes */
} __attribute__((packed)) hid_keyboard_report_t;

/* Boot protocol mouse report (3+ bytes) */
typedef struct {
    uint8_t buttons;        /* Button states */
    int8_t  x_delta;        /* X movement (-127 to 127) */
    int8_t  y_delta;        /* Y movement (-127 to 127) */
} __attribute__((packed)) hid_mouse_report_t;

/* Keyboard modifier bits */
#define HID_MOD_LEFT_CTRL       0x01
#define HID_MOD_LEFT_SHIFT      0x02
#define HID_MOD_LEFT_ALT        0x04
#define HID_MOD_LEFT_GUI        0x08
#define HID_MOD_RIGHT_CTRL      0x10
#define HID_MOD_RIGHT_SHIFT     0x20
#define HID_MOD_RIGHT_ALT       0x40
#define HID_MOD_RIGHT_GUI       0x80

/* Mouse button bits */
#define HID_MOUSE_BTN_LEFT      0x01
#define HID_MOUSE_BTN_RIGHT     0x02
#define HID_MOUSE_BTN_MIDDLE    0x04

/* HID device state */
typedef struct {
    uint8_t  dev_addr;          /* USB device address */
    uint8_t  interface_num;     /* Interface number */
    uint8_t  ep_addr;           /* Interrupt endpoint address */
    uint16_t ep_max_packet;     /* Endpoint max packet size */
    uint8_t  ep_interval;       /* Polling interval (ms) */
    uint8_t  protocol;          /* Boot protocol type */
    uint8_t  data_toggle;       /* Data toggle for interrupt transfers */
    bool     configured;        /* Device is configured and ready */

    /* Keyboard state */
    hid_keyboard_report_t kbd_report;
    hid_keyboard_report_t kbd_prev_report;

    /* Mouse state */
    hid_mouse_report_t mouse_report;
    int16_t mouse_x;            /* Accumulated X position */
    int16_t mouse_y;            /* Accumulated Y position */
} hid_device_t;

/* Maximum HID devices */
#define HID_MAX_DEVICES     4

/* Public API */
bool usb_hid_init(void);

/* Device detection after USB enumeration */
int usb_hid_probe_device(uint8_t dev_addr);

/* Polling - call periodically to update input state */
void usb_hid_poll(void);

/* Keyboard state access */
bool usb_hid_keyboard_connected(void);
uint8_t usb_hid_get_modifiers(void);
bool usb_hid_key_pressed(uint8_t usb_keycode);
uint8_t usb_hid_get_key(void);  /* Returns first pressed key, 0 if none */

/* Mouse state access */
bool usb_hid_mouse_connected(void);
uint8_t usb_hid_get_mouse_buttons(void);
void usb_hid_get_mouse_position(int16_t *x, int16_t *y);
void usb_hid_get_mouse_delta(int8_t *dx, int8_t *dy);

/* USB HID keycode to Mac keycode translation */
uint8_t usb_hid_to_mac_keycode(uint8_t usb_keycode);

#endif /* ARM64_USB_HID_H */
