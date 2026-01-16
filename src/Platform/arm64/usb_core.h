/*
 * USB Core - Device enumeration and control transfers
 * For Raspberry Pi ARM64
 */

#ifndef ARM64_USB_CORE_H
#define ARM64_USB_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* USB Request Types */
#define USB_REQ_TYPE_DIR_MASK       0x80
#define USB_REQ_TYPE_DIR_OUT        0x00
#define USB_REQ_TYPE_DIR_IN         0x80

#define USB_REQ_TYPE_TYPE_MASK      0x60
#define USB_REQ_TYPE_STANDARD       0x00
#define USB_REQ_TYPE_CLASS          0x20
#define USB_REQ_TYPE_VENDOR         0x40

#define USB_REQ_TYPE_RECIP_MASK     0x1F
#define USB_REQ_TYPE_DEVICE         0x00
#define USB_REQ_TYPE_INTERFACE      0x01
#define USB_REQ_TYPE_ENDPOINT       0x02
#define USB_REQ_TYPE_OTHER          0x03

/* USB Standard Requests */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

/* USB Descriptor Types */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIGURATION 0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_HID_REPORT    0x22

/* USB Class Codes */
#define USB_CLASS_HID               0x03
#define USB_CLASS_HUB               0x09

/* USB Endpoint Types */
#define USB_EP_TYPE_CONTROL         0x00
#define USB_EP_TYPE_ISOCHRONOUS     0x01
#define USB_EP_TYPE_BULK            0x02
#define USB_EP_TYPE_INTERRUPT       0x03

/* USB Setup Packet (8 bytes) */
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_t;

/* USB Device Descriptor (18 bytes) */
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
} __attribute__((packed)) usb_device_desc_t;

/* USB Configuration Descriptor (9 bytes) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_desc_t;

/* USB Interface Descriptor (9 bytes) */
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
} __attribute__((packed)) usb_interface_desc_t;

/* USB Endpoint Descriptor (7 bytes) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_desc_t;

/* USB HID Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wReportDescriptorLength;
} __attribute__((packed)) usb_hid_desc_t;

/* USB Device State */
typedef struct {
    uint8_t  address;
    uint8_t  max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  num_configurations;
    uint8_t  num_interfaces;
    bool     configured;
    uint8_t  speed;
} usb_device_t;

/* Maximum devices supported */
#define USB_MAX_DEVICES     8

/* Control transfer timeout (ms) */
#define USB_CTRL_TIMEOUT    500

/* Public API */
bool usb_core_init(void);

/* Control transfers */
int usb_control_transfer(uint8_t dev_addr, usb_setup_t *setup,
                         uint8_t *data, uint16_t length);

/* Standard requests */
int usb_get_device_descriptor(uint8_t dev_addr, usb_device_desc_t *desc);
int usb_get_device_descriptor_short(uint8_t dev_addr, usb_device_desc_t *desc);
int usb_set_address(uint8_t new_addr);
int usb_get_config_descriptor(uint8_t dev_addr, uint8_t *buffer, uint16_t length);
int usb_set_configuration(uint8_t dev_addr, uint8_t config);

/* Device enumeration */
int usb_enumerate_device(void);
usb_device_t *usb_get_device(uint8_t address);

/* Interrupt transfers (for HID) */
int usb_interrupt_transfer(uint8_t dev_addr, uint8_t ep_addr,
                           uint8_t *buffer, uint16_t length,
                           uint16_t max_packet, uint8_t *toggle);

#endif /* ARM64_USB_CORE_H */
