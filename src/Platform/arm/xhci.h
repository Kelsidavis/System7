/*
 * XHCI USB Host Controller Driver for Raspberry Pi 4/5
 * Implements USB 3.0/2.0 host controller interface (eXtensible HCI)
 *
 * References:
 *   - XHCI Specification 1.2
 *   - USB 3.0 Specification
 *   - Raspberry Pi 4/5 Device Tree and firmware
 */

#ifndef ARM_XHCI_H
#define ARM_XHCI_H

#include <stdint.h>
#include "usb_controller.h"

/* ===== XHCI Controller Base Addresses ===== */

/* Raspberry Pi 4/5 XHCI is behind PCIe
 * Physical address depends on PCIe configuration
 * Typical: 0x600000000 (above 32-bit memory space)
 * Must be discovered via device tree or PCIe enumeration
 */
#define XHCI_BASE_DEFAULT   0x00000000  /* Will be set dynamically */

extern uint32_t xhci_base;  /* Global XHCI controller base address */

/* ===== USB Device Speed Codes ===== */
#define USB_SPEED_FULL      1  /* 12 Mbps */
#define USB_SPEED_LOW       2  /* 1.5 Mbps */
#define USB_SPEED_HIGH      3  /* 480 Mbps */
#define USB_SPEED_SUPER     4  /* 5 Gbps */

/* ===== XHCI Capability Registers (offset 0x00) ===== */
#define XHCI_CAP_CAPLENGTH      0x00  /* Capability Register Length */
#define XHCI_CAP_HCIVERSION     0x02  /* XHCI Version */
#define XHCI_CAP_HCSPARAMS1     0x04  /* Structural Parameters 1 */
#define XHCI_CAP_HCSPARAMS2     0x08  /* Structural Parameters 2 */
#define XHCI_CAP_HCSPARAMS3     0x0C  /* Structural Parameters 3 */
#define XHCI_CAP_HCCPARAMS1     0x10  /* Capability Parameters 1 */
#define XHCI_CAP_DBOFF          0x14  /* Doorbell Offset */
#define XHCI_CAP_RTSOFF         0x18  /* Runtime Registers Offset */
#define XHCI_CAP_HCCPARAMS2     0x1C  /* Capability Parameters 2 */

/* ===== XHCI Operational Registers (offset from CAPLENGTH) ===== */
#define XHCI_OP_USBCMD          0x00  /* USB Command */
#define XHCI_OP_USBSTS          0x04  /* USB Status */
#define XHCI_OP_PAGESIZE        0x08  /* Page Size Register */
#define XHCI_OP_DNCTRL          0x14  /* Device Notification Control */
#define XHCI_OP_CRCR            0x18  /* Command Ring Control */
#define XHCI_OP_DCBAAP          0x30  /* Device Context Base Address Array Pointer */
#define XHCI_OP_CONFIG          0x38  /* Configure Register */
#define XHCI_OP_PORTSC(n)       (0x400 + 0x10*(n))  /* Port Status/Control (port n) */

/* ===== XHCI Command Register Bits ===== */
#define XHCI_CMD_RUN            (1 << 0)   /* Run/Stop */
#define XHCI_CMD_RESET          (1 << 1)   /* Host Controller Reset */
#define XHCI_CMD_INTE           (1 << 2)   /* Interrupter Enable */
#define XHCI_CMD_HSEE           (1 << 3)   /* Host System Error Enable */

/* ===== XHCI Status Register Bits ===== */
#define XHCI_STS_HCH            (1 << 0)   /* Host Controller Halted */
#define XHCI_STS_FATAL          (1 << 2)   /* Host System Error */
#define XHCI_STS_EINT           (1 << 3)   /* Event Interrupt */
#define XHCI_STS_PCD            (1 << 4)   /* Port Change Detect */
#define XHCI_STS_SSS            (1 << 8)   /* Save State Status */
#define XHCI_STS_RSS            (1 << 9)   /* Restore State Status */
#define XHCI_STS_SRE            (1 << 10)  /* Save/Restore Error */
#define XHCI_STS_CNR            (1 << 11)  /* Controller Not Ready */
#define XHCI_STS_HCE            (1 << 12)  /* Host Controller Error */

/* ===== USB Endpoint Types ===== */
#define USB_EP_CONTROL      0
#define USB_EP_ISOCHRONOUS  1
#define USB_EP_BULK         2
#define USB_EP_INTERRUPT    3

/* ===== USB Device Descriptor ===== */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_descriptor_t;

/* ===== USB Configuration Descriptor ===== */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_config_descriptor_t;

/* ===== USB Interface Descriptor ===== */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

/* ===== USB Endpoint Descriptor ===== */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_descriptor_t;

/* ===== Public API ===== */

/* Initialize XHCI controller */
int xhci_init(void);

/* Enumerate connected USB devices */
int xhci_enumerate_devices(void);

/* Find HID keyboard device */
int xhci_find_keyboard(hid_device_info_t *kb_info);

/* Find HID mouse device */
int xhci_find_mouse(hid_device_info_t *mouse_info);

/* Poll keyboard for input */
int xhci_poll_keyboard(uint8_t *key_code, uint8_t *modifiers);

/* Poll mouse for movement/buttons */
int xhci_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons);

/* Get number of connected devices */
uint32_t xhci_device_count(void);

/* Shutdown XHCI controller */
void xhci_shutdown(void);

/* Low-level operations */
int xhci_reset_controller(void);
int xhci_wait_ready(void);
int xhci_get_port_count(void);
int xhci_port_enabled(uint8_t port);

#endif /* ARM_XHCI_H */
