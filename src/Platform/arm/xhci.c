/*
 * XHCI USB Host Controller Implementation
 * Raspberry Pi 4/5 USB 3.0/2.0 host driver
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "mmio.h"
#include "xhci.h"

/* Global XHCI state */
uint32_t xhci_base = 0;
static int xhci_initialized = 0;
static uint32_t xhci_capability_length = 0;

/* Cached device information */
static hid_device_info_t cached_keyboard = {0};
static hid_device_info_t cached_mouse = {0};
static int keyboard_found = 0;
static int mouse_found = 0;
static uint32_t device_count = 0;

/*
 * Discover XHCI base address
 * On Raspberry Pi 4/5, XHCI is behind PCIe
 * Must scan device tree or use known base addresses
 */
static int xhci_discover_base(void) {
    /* TODO: Read from device tree
     * Fallback: Pi 4/5 XHCI is typically at high memory address
     * For now, we'll try to detect it at compile-time or from device tree
     */

    /* Raspberry Pi 4: XHCI is at 0x600000000 (physical, 64-bit)
     * But on 32-bit ARM, must use lower mapping
     * Device tree will specify the correct address
     */

    Serial_WriteString("[XHCI] XHCI base address discovery not implemented\n");
    Serial_WriteString("[XHCI] This requires device tree parsing or bootloader support\n");

    return -1;
}

/*
 * Initialize XHCI controller
 */
int xhci_init(void) {
    Serial_WriteString("[XHCI] Initializing USB 3.0 host controller (XHCI)\n");

    if (xhci_discover_base() != 0) {
        Serial_WriteString("[XHCI] Failed to discover XHCI base address\n");
        Serial_WriteString("[XHCI] XHCI initialization aborted\n");
        return -1;
    }

    /* Read capability length from first register */
    uint8_t caplength = mmio_read8(xhci_base + XHCI_CAP_CAPLENGTH);
    xhci_capability_length = caplength;

    Serial_Printf("[XHCI] Base: 0x%lx, Capability Length: %u\n",
                  (unsigned long)xhci_base, caplength);

    /* Verify XHCI version */
    uint16_t version = mmio_read16(xhci_base + XHCI_CAP_HCIVERSION);
    Serial_Printf("[XHCI] XHCI Version: %x.%02x\n", (version >> 8) & 0xFF, version & 0xFF);

    /* Read structural parameters to detect port count */
    uint32_t params1 = mmio_read32(xhci_base + XHCI_CAP_HCSPARAMS1);
    uint32_t max_ports = params1 & 0xFF;
    Serial_Printf("[XHCI] Maximum ports: %lu\n", (unsigned long)max_ports);

    /* Reset controller if needed */
    if (xhci_reset_controller() != 0) {
        Serial_WriteString("[XHCI] Controller reset failed\n");
        return -1;
    }

    xhci_initialized = 1;
    Serial_WriteString("[XHCI] Controller initialization complete\n");

    return 0;
}

/*
 * Reset XHCI controller
 */
int xhci_reset_controller(void) {
    if (!xhci_base) {
        return -1;
    }

    uint32_t op_base = xhci_base + xhci_capability_length;

    /* Read current USBCMD */
    uint32_t cmd = mmio_read32(op_base + XHCI_OP_USBCMD);

    /* Halt controller first */
    cmd &= ~XHCI_CMD_RUN;
    mmio_write32(op_base + XHCI_OP_USBCMD, cmd);

    /* Wait for halt (HCH bit in status) */
    uint32_t timeout = 10000;
    while (timeout > 0) {
        uint32_t sts = mmio_read32(op_base + XHCI_OP_USBSTS);
        if (sts & XHCI_STS_HCH) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        Serial_WriteString("[XHCI] Controller halt timeout\n");
        return -1;
    }

    /* Send reset command */
    cmd |= XHCI_CMD_RESET;
    mmio_write32(op_base + XHCI_OP_USBCMD, cmd);

    /* Wait for reset to complete (CNR = Controller Not Ready bit clears) */
    timeout = 10000;
    while (timeout > 0) {
        uint32_t sts = mmio_read32(op_base + XHCI_OP_USBSTS);
        if (!(sts & XHCI_STS_CNR)) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        Serial_WriteString("[XHCI] Controller reset timeout\n");
        return -1;
    }

    Serial_WriteString("[XHCI] Controller reset successful\n");
    return 0;
}

/*
 * Wait for controller ready
 */
int xhci_wait_ready(void) {
    if (!xhci_base || !xhci_initialized) {
        return -1;
    }

    uint32_t op_base = xhci_base + xhci_capability_length;

    uint32_t timeout = 10000;
    while (timeout > 0) {
        uint32_t sts = mmio_read32(op_base + XHCI_OP_USBSTS);
        if (!(sts & XHCI_STS_CNR)) {
            return 0;
        }
        timeout--;
    }

    return -1;
}

/*
 * Get port count
 */
int xhci_get_port_count(void) {
    if (!xhci_base) {
        return 0;
    }

    uint32_t params1 = mmio_read32(xhci_base + XHCI_CAP_HCSPARAMS1);
    return params1 & 0xFF;
}

/*
 * Check if port is enabled
 */
int xhci_port_enabled(uint8_t port) {
    if (!xhci_base || port > xhci_get_port_count()) {
        return 0;
    }

    uint32_t op_base = xhci_base + xhci_capability_length;
    uint32_t portsc = mmio_read32(op_base + XHCI_OP_PORTSC(port));

    /* Bit 0: CCS = Current Connect Status */
    return (portsc & 0x01) != 0;
}

/*
 * Enumerate USB devices
 */
int xhci_enumerate_devices(void) {
    Serial_WriteString("[XHCI] Enumerating USB devices...\n");

    if (!xhci_initialized) {
        Serial_WriteString("[XHCI] XHCI not initialized\n");
        return -1;
    }

    int port_count = xhci_get_port_count();
    Serial_Printf("[XHCI] Checking %d ports\n", port_count);

    device_count = 0;
    keyboard_found = 0;
    mouse_found = 0;

    /* Scan each port for connected devices */
    for (int port = 1; port <= port_count; port++) {
        if (xhci_port_enabled(port)) {
            Serial_Printf("[XHCI] Device detected on port %d\n", port);
            device_count++;

            /* TODO: Get device descriptor, enumerate endpoints
             * For now, just count connected devices
             */
        }
    }

    Serial_Printf("[XHCI] Found %lu devices\n", (unsigned long)device_count);

    /* TODO: Parse device descriptors and find HID devices
     * Set keyboard_found and mouse_found flags
     */

    return 0;
}

/*
 * Find HID keyboard
 */
int xhci_find_keyboard(hid_device_info_t *kb_info) {
    if (!kb_info) {
        return -1;
    }

    if (!keyboard_found) {
        return -1;  /* No keyboard found */
    }

    memcpy(kb_info, &cached_keyboard, sizeof(*kb_info));
    return 0;
}

/*
 * Find HID mouse
 */
int xhci_find_mouse(hid_device_info_t *mouse_info) {
    if (!mouse_info) {
        return -1;
    }

    if (!mouse_found) {
        return -1;  /* No mouse found */
    }

    memcpy(mouse_info, &cached_mouse, sizeof(*mouse_info));
    return 0;
}

/*
 * Poll keyboard
 * Returns 0 if no data, 1 if key pressed, -1 on error
 */
int xhci_poll_keyboard(uint8_t *key_code, uint8_t *modifiers) {
    if (!key_code || !modifiers || !keyboard_found) {
        return -1;
    }

    /* TODO: Read from interrupt endpoint
     * HID keyboard reports are 8 bytes:
     *   [0] = modifier keys
     *   [1] = reserved
     *   [2-7] = up to 6 key codes (USB HID Keyboard)
     */

    return 0;  /* No data available yet */
}

/*
 * Poll mouse
 * Returns number of reports available
 */
int xhci_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons) {
    if (!dx || !dy || !buttons || !mouse_found) {
        return -1;
    }

    /* TODO: Read from interrupt endpoint
     * HID mouse reports are 3-4 bytes:
     *   [0] = button states
     *   [1] = X movement (signed)
     *   [2] = Y movement (signed)
     *   [3] = wheel (optional)
     */

    return 0;  /* No data available yet */
}

/*
 * Get number of connected devices
 */
uint32_t xhci_device_count(void) {
    return device_count;
}

/*
 * Shutdown XHCI
 */
void xhci_shutdown(void) {
    Serial_WriteString("[XHCI] Shutting down XHCI controller\n");

    if (!xhci_base) {
        return;
    }

    /* Reset controller to clean state */
    xhci_reset_controller();

    xhci_initialized = 0;
}
