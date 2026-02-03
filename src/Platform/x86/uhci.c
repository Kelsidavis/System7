/*
 * uhci.c - Minimal x86 UHCI controller bring-up (QEMU)
 *
 * Phase-3 groundwork: detect UHCI, enable I/O space, log port status.
 * HID transfers are not implemented yet.
 */

#include "uhci.h"
#include "pci.h"
#include "Platform/include/serial.h"
#include "Platform/include/io.h"
#include <stdint.h>

#define UHCI_CLASS_CODE 0x0C
#define UHCI_SUBCLASS   0x03
#define UHCI_PROG_IF    0x00

/* UHCI I/O register offsets */
#define UHCI_USBCMD     0x00
#define UHCI_USBSTS     0x02
#define UHCI_PORTSC1    0x10
#define UHCI_PORTSC2    0x12

/* USBCMD bits */
#define UHCI_CMD_RUN    (1u << 0)
#define UHCI_CMD_HCRESET (1u << 1)

static inline uint16_t io_read16(uint16_t base, uint16_t off) {
    return hal_inw((uint16_t)(base + off));
}

static inline void io_write16(uint16_t base, uint16_t off, uint16_t value) {
    hal_outw((uint16_t)(base + off), value);
}

static bool uhci_reset(uint16_t io_base) {
    uint16_t cmd = io_read16(io_base, UHCI_USBCMD);
    cmd |= UHCI_CMD_HCRESET;
    io_write16(io_base, UHCI_USBCMD, cmd);

    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        uint16_t now = io_read16(io_base, UHCI_USBCMD);
        if ((now & UHCI_CMD_HCRESET) == 0) {
            return true;
        }
    }
    return false;
}

bool uhci_init_x86(void) {
    pci_device_t devices[64];
    int found = pci_scan(devices, 64);
    if (found > 64) {
        found = 64;
    }

    for (int i = 0; i < found; i++) {
        if (devices[i].class_code == UHCI_CLASS_CODE &&
            devices[i].subclass == UHCI_SUBCLASS &&
            devices[i].prog_if == UHCI_PROG_IF) {

            uint32_t cmd = pci_read_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04);
            cmd |= (1 << 0); /* I/O Space */
            cmd |= (1 << 2); /* Bus Master */
            pci_write_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04, cmd);

            uint16_t io_base = (uint16_t)(devices[i].bar_addrs[0] & 0xFFFF);
            serial_printf("[UHCI] IO base=0x%04x\n", io_base);

            if (!uhci_reset(io_base)) {
                serial_puts("[UHCI] reset timeout\n");
                return false;
            }

            uint16_t port1 = io_read16(io_base, UHCI_PORTSC1);
            uint16_t port2 = io_read16(io_base, UHCI_PORTSC2);
            if (port1 & 0x0001) {
                serial_printf("[UHCI] port1 connected (PORTSC1=0x%04x)\n", port1);
            }
            if (port2 & 0x0001) {
                serial_printf("[UHCI] port2 connected (PORTSC2=0x%04x)\n", port2);
            }

            serial_puts("[UHCI] Phase-3 init complete (no HID transfers yet)\n");
            return true;
        }
    }

    serial_puts("[UHCI] controller not found\n");
    return false;
}
