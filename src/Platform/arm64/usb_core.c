/*
 * USB Core - Device enumeration and control transfers
 * For Raspberry Pi ARM64
 */

#include "usb_core.h"
#include "dwc2.h"
#include "timer.h"
#include "uart.h"
#include "cache.h"
#include <string.h>

/* DMA buffer - must be 64-byte aligned for cache coherency */
static uint8_t __attribute__((aligned(64))) dma_buffer[512];

/* Device registry */
static usb_device_t devices[USB_MAX_DEVICES];
static uint8_t next_address = 1;
static bool usb_initialized = false;

/*
 * Initialize USB core
 */
bool usb_core_init(void) {
    if (usb_initialized) {
        return true;
    }

    /* Clear device registry */
    memset(devices, 0, sizeof(devices));
    next_address = 1;

    /* Initialize DWC2 controller */
    if (!dwc2_init()) {
        uart_puts("[USB] Controller init failed\n");
        return false;
    }

    usb_initialized = true;
    uart_puts("[USB] Core initialized\n");
    return true;
}

/*
 * Perform a control transfer
 */
int usb_control_transfer(uint8_t dev_addr, usb_setup_t *setup,
                         uint8_t *data, uint16_t length) {
    int ch;
    int ret;
    uint16_t max_pkt = 8;

    /* Get max packet size for this device */
    if (dev_addr > 0 && dev_addr < USB_MAX_DEVICES && devices[dev_addr].configured) {
        max_pkt = devices[dev_addr].max_packet_size;
    }

    /* Allocate channel */
    ch = dwc2_channel_alloc();
    if (ch < 0) {
        uart_puts("[USB] No channel available\n");
        return -1;
    }

    /* Copy setup packet to DMA buffer */
    memcpy(dma_buffer, setup, 8);
    dcache_clean_range(dma_buffer, 8);

    /* SETUP stage */
    ret = dwc2_transfer_start(ch, dev_addr, 0, EP_TYPE_CONTROL, false,
                              max_pkt, dma_buffer, 8, HCTSIZ_PID_SETUP);
    if (ret != 0) {
        dwc2_channel_free(ch);
        return -1;
    }

    ret = dwc2_transfer_wait(ch, USB_CTRL_TIMEOUT);
    if (ret != 0) {
        dwc2_channel_free(ch);
        return -1;
    }

    /* DATA stage (if any) */
    if (length > 0) {
        bool is_in = (setup->bmRequestType & USB_REQ_TYPE_DIR_IN) != 0;

        if (!is_in && data) {
            /* OUT - copy data to DMA buffer */
            memcpy(dma_buffer, data, length);
            dcache_clean_range(dma_buffer, length);
        }

        ret = dwc2_transfer_start(ch, dev_addr, 0, EP_TYPE_CONTROL, is_in,
                                  max_pkt, dma_buffer, length, HCTSIZ_PID_DATA1);
        if (ret != 0) {
            dwc2_channel_free(ch);
            return -1;
        }

        ret = dwc2_transfer_wait(ch, USB_CTRL_TIMEOUT);
        if (ret != 0) {
            dwc2_channel_free(ch);
            return -1;
        }

        if (is_in && data) {
            /* IN - copy data from DMA buffer */
            dcache_invalidate_range(dma_buffer, length);
            dwc2_channel_t *chan = dwc2_channel_get(ch);
            uint32_t actual = chan ? chan->transferred : length;
            if (actual > length) actual = length;  /* Bounds check */
            memcpy(data, dma_buffer, actual);
        }
    }

    /* STATUS stage (opposite direction) */
    bool status_in = (setup->bmRequestType & USB_REQ_TYPE_DIR_IN) == 0;
    if (length == 0) {
        status_in = true;  /* No data stage = IN status */
    }

    ret = dwc2_transfer_start(ch, dev_addr, 0, EP_TYPE_CONTROL, status_in,
                              max_pkt, dma_buffer, 0, HCTSIZ_PID_DATA1);
    if (ret != 0) {
        dwc2_channel_free(ch);
        return -1;
    }

    ret = dwc2_transfer_wait(ch, USB_CTRL_TIMEOUT);
    dwc2_channel_free(ch);

    return ret;
}

/*
 * Get device descriptor (full 18 bytes)
 */
int usb_get_device_descriptor(uint8_t dev_addr, usb_device_desc_t *desc) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_TYPE_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_device_desc_t)
    };

    return usb_control_transfer(dev_addr, &setup, (uint8_t*)desc, sizeof(usb_device_desc_t));
}

/*
 * Get device descriptor (first 8 bytes only - to get max packet size)
 */
int usb_get_device_descriptor_short(uint8_t dev_addr, usb_device_desc_t *desc) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_TYPE_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = 8
    };

    return usb_control_transfer(dev_addr, &setup, (uint8_t*)desc, 8);
}

/*
 * Set device address
 */
int usb_set_address(uint8_t new_addr) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = new_addr,
        .wIndex = 0,
        .wLength = 0
    };

    return usb_control_transfer(0, &setup, NULL, 0);
}

/*
 * Get configuration descriptor
 */
int usb_get_config_descriptor(uint8_t dev_addr, uint8_t *buffer, uint16_t length) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_TYPE_CONFIGURATION << 8) | 0,
        .wIndex = 0,
        .wLength = length
    };

    return usb_control_transfer(dev_addr, &setup, buffer, length);
}

/*
 * Set configuration
 */
int usb_set_configuration(uint8_t dev_addr, uint8_t config) {
    usb_setup_t setup = {
        .bmRequestType = USB_REQ_TYPE_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = config,
        .wIndex = 0,
        .wLength = 0
    };

    return usb_control_transfer(dev_addr, &setup, NULL, 0);
}

/*
 * Enumerate a newly connected device
 */
int usb_enumerate_device(void) {
    usb_device_desc_t desc;
    uint8_t config_buf[256];
    int ret;

    uart_puts("[USB] Enumerating device...\n");

    /* Step 1: Get first 8 bytes to learn max packet size */
    memset(&desc, 0, sizeof(desc));
    ret = usb_get_device_descriptor_short(0, &desc);
    if (ret != 0) {
        uart_puts("[USB] Failed to get device descriptor\n");
        return -1;
    }

    uint8_t max_pkt = desc.bMaxPacketSize0;
    if (max_pkt == 0) max_pkt = 8;

    uart_puts("[USB] Device max packet: ");
    /* Simple number print */
    char num[4];
    num[0] = '0' + (max_pkt / 10);
    num[1] = '0' + (max_pkt % 10);
    num[2] = '\n';
    num[3] = '\0';
    uart_puts(num);

    /* Step 2: Assign address */
    uint8_t new_addr = next_address++;
    if (new_addr >= USB_MAX_DEVICES) {
        uart_puts("[USB] Too many devices\n");
        return -1;
    }

    ret = usb_set_address(new_addr);
    if (ret != 0) {
        uart_puts("[USB] Failed to set address\n");
        return -1;
    }

    /* Address recovery time */
    timer_usleep(2000);

    /* Store initial device info */
    usb_device_t *dev = &devices[new_addr];
    dev->address = new_addr;
    dev->max_packet_size = max_pkt;
    dev->speed = dwc2_port_speed();

    /* Step 3: Get full device descriptor */
    ret = usb_get_device_descriptor(new_addr, &desc);
    if (ret != 0) {
        uart_puts("[USB] Failed to get full descriptor\n");
        return -1;
    }

    dev->vendor_id = desc.idVendor;
    dev->product_id = desc.idProduct;
    dev->device_class = desc.bDeviceClass;
    dev->device_subclass = desc.bDeviceSubClass;
    dev->device_protocol = desc.bDeviceProtocol;
    dev->num_configurations = desc.bNumConfigurations;

    uart_puts("[USB] Device enumerated at address ");
    num[0] = '0' + new_addr;
    num[1] = '\n';
    num[2] = '\0';
    uart_puts(num);

    /* Step 4: Get configuration descriptor header */
    ret = usb_get_config_descriptor(new_addr, config_buf, 9);
    if (ret != 0) {
        uart_puts("[USB] Failed to get config descriptor\n");
        return new_addr;  /* Device is usable but not configured */
    }

    usb_config_desc_t *cfg = (usb_config_desc_t*)config_buf;
    uint16_t total_len = cfg->wTotalLength;
    if (total_len > sizeof(config_buf)) {
        total_len = sizeof(config_buf);
    }

    /* Get full configuration */
    ret = usb_get_config_descriptor(new_addr, config_buf, total_len);
    if (ret != 0) {
        return new_addr;
    }

    dev->num_interfaces = cfg->bNumInterfaces;

    /* Step 5: Set configuration */
    ret = usb_set_configuration(new_addr, cfg->bConfigurationValue);
    if (ret != 0) {
        uart_puts("[USB] Failed to set configuration\n");
        return new_addr;
    }

    dev->configured = true;
    uart_puts("[USB] Device configured\n");

    return new_addr;
}

/*
 * Get device info by address
 */
usb_device_t *usb_get_device(uint8_t address) {
    if (address > 0 && address < USB_MAX_DEVICES) {
        return &devices[address];
    }
    return NULL;
}

/*
 * Perform an interrupt transfer (for HID polling)
 */
int usb_interrupt_transfer(uint8_t dev_addr, uint8_t ep_addr,
                           uint8_t *buffer, uint16_t length,
                           uint16_t max_packet, uint8_t *toggle) {
    int ch;
    int ret;
    bool is_in = (ep_addr & 0x80) != 0;
    uint8_t ep_num = ep_addr & 0x0F;

    /* Allocate channel */
    ch = dwc2_channel_alloc();
    if (ch < 0) {
        return -1;
    }

    /* Determine PID based on toggle */
    uint32_t pid = (*toggle) ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;

    /* For IN transfers, use our DMA buffer */
    uint8_t *xfer_buf = is_in ? dma_buffer : buffer;
    if (!is_in) {
        memcpy(dma_buffer, buffer, length);
        dcache_clean_range(dma_buffer, length);
        xfer_buf = dma_buffer;
    }

    /* Start transfer */
    ret = dwc2_transfer_start(ch, dev_addr, ep_num, EP_TYPE_INTERRUPT,
                              is_in, max_packet, xfer_buf, length, pid);
    if (ret != 0) {
        dwc2_channel_free(ch);
        return -1;
    }

    /* Wait with short timeout for interrupt transfers */
    ret = dwc2_transfer_wait(ch, 50);

    if (ret == 0) {
        /* Toggle data PID for next transfer */
        *toggle = !(*toggle);

        if (is_in) {
            dcache_invalidate_range(dma_buffer, length);
            dwc2_channel_t *chan = dwc2_channel_get(ch);
            uint32_t actual = chan ? chan->transferred : length;
            if (actual > length) actual = length;  /* Bounds check */
            memcpy(buffer, dma_buffer, actual);
            dwc2_channel_free(ch);
            return (int)actual;
        }
    }

    dwc2_channel_free(ch);
    return ret;
}
