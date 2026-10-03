/*
 * USB Controller Abstraction Layer
 * Provides unified interface for both XHCI (Pi 4/5) and DWCOTG (Pi 3)
 */

#ifndef ARM_USB_CONTROLLER_H
#define ARM_USB_CONTROLLER_H

#include <stdint.h>

/* ===== Raspberry Pi Model Detection ===== */
typedef enum {
    PI_MODEL_UNKNOWN = 0,
    PI_MODEL_3       = 3,
    PI_MODEL_4       = 4,
    PI_MODEL_5       = 5,
} rpi_model_t;

/* HID device details shared by the platform USB controllers. */
typedef struct {
    uint16_t idVendor;
    uint16_t idProduct;
    uint8_t  bInterfaceClass;      /* HID class = 0x03 */
    uint8_t  bInterfaceSubClass;   /* 1=keyboard, 2=mouse */
    uint8_t  bInterfaceProtocol;   /* 1=keyboard, 2=mouse */
    uint8_t  ep_in;                /* Input endpoint address */
    uint8_t  ep_in_interval;       /* Polling interval (ms) */
    uint8_t  ep_in_max_packet;     /* Max packet size */
} hid_device_info_t;

/* ===== Public API ===== */

/* Detect Raspberry Pi model */
rpi_model_t usb_detect_rpi_model(void);

/* Initialize USB controller (auto-selects XHCI or DWCOTG) */
int usb_controller_init(void);

/* Enumerate USB devices */
int usb_controller_enumerate(void);

/* Find HID keyboard */
int usb_find_keyboard(hid_device_info_t *kb_info);

/* Find HID mouse */
int usb_find_mouse(hid_device_info_t *mouse_info);

/* Poll keyboard */
int usb_poll_keyboard(uint8_t *key_code, uint8_t *modifiers);

/* Poll mouse */
int usb_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons);

/* Get device count */
uint32_t usb_device_count(void);

/* Shutdown USB controller */
void usb_controller_shutdown(void);

#endif /* ARM_USB_CONTROLLER_H */
