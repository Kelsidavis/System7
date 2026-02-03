/*
 * ehci.c - Minimal x86 EHCI controller bring-up (QEMU)
 *
 * Phase-2 groundwork: detect EHCI, enable MMIO, reset, log ports.
 * HID transfers are not implemented yet.
 */

#include "ehci.h"
#include "pci.h"
#include "usb_core.h"
#include "Platform/include/serial.h"
#include <stdint.h>

#define EHCI_CLASS_CODE 0x0C
#define EHCI_SUBCLASS   0x03
#define EHCI_PROG_IF    0x20

/* Capability registers */
#define EHCI_CAPLENGTH     0x00
#define EHCI_HCIVERSION    0x02
#define EHCI_HCSPARAMS     0x04

/* Operational registers (offset from op base) */
#define EHCI_USBCMD        0x00
#define EHCI_USBSTS        0x04
#define EHCI_PORTSC_BASE   0x44
#define EHCI_PORTSC_STRIDE 0x04

/* USBCMD bits */
#define EHCI_CMD_RUN       (1u << 0)
#define EHCI_CMD_RESET     (1u << 1)

/* USBSTS bits */
#define EHCI_STS_HCH       (1u << 12)

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off) {
    volatile uint32_t *ptr = (volatile uint32_t *)(base + off);
    return *ptr;
}

static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t value) {
    volatile uint32_t *ptr = (volatile uint32_t *)(base + off);
    *ptr = value;
}

static bool ehci_reset(uintptr_t base, uint32_t cap_len) {
    uintptr_t op_base = base + cap_len;
    uint32_t cmd = mmio_read32(op_base, EHCI_USBCMD);
    cmd &= ~EHCI_CMD_RUN;
    mmio_write32(op_base, EHCI_USBCMD, cmd);

    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        uint32_t sts = mmio_read32(op_base, EHCI_USBSTS);
        if (sts & EHCI_STS_HCH) {
            break;
        }
    }

    cmd |= EHCI_CMD_RESET;
    mmio_write32(op_base, EHCI_USBCMD, cmd);

    timeout = 100000;
    while (timeout-- > 0) {
        uint32_t cmd_now = mmio_read32(op_base, EHCI_USBCMD);
        if ((cmd_now & EHCI_CMD_RESET) == 0) {
            return true;
        }
    }
    return false;
}

bool ehci_init_x86(void) {
    pci_device_t devices[64];
    int found = pci_scan(devices, 64);
    if (found > 64) {
        found = 64;
    }

    for (int i = 0; i < found; i++) {
        if (devices[i].class_code == EHCI_CLASS_CODE &&
            devices[i].subclass == EHCI_SUBCLASS &&
            devices[i].prog_if == EHCI_PROG_IF) {

            uint32_t cmd = pci_read_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04);
            cmd |= (1 << 1); /* Memory Space */
            cmd |= (1 << 2); /* Bus Master */
            pci_write_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04, cmd);

            uintptr_t base = (uintptr_t)devices[i].bar_addrs[0];
            serial_printf("[EHCI] MMIO base=0x%08x size=0x%08x\n",
                          (uint32_t)base, devices[i].bar_sizes[0]);
            usb_core_x86_register_controller(USB_CTRL_EHCI, base, true);

            uint8_t cap_len = (uint8_t)(mmio_read32(base, EHCI_CAPLENGTH) & 0xFF);
            uint16_t version = (uint16_t)((mmio_read32(base, EHCI_HCIVERSION)) & 0xFFFF);
            uint32_t params = mmio_read32(base, EHCI_HCSPARAMS);
            uint8_t ports = (uint8_t)(params & 0x0F);

            serial_printf("[EHCI] caplen=%u version=%x ports=%u\n", cap_len, version, ports);

            if (!ehci_reset(base, cap_len)) {
                serial_puts("[EHCI] reset timeout\n");
                return false;
            }

            uintptr_t op_base = base + cap_len;
            for (uint8_t p = 0; p < ports; p++) {
                uint32_t portsc = mmio_read32(op_base, EHCI_PORTSC_BASE + p * EHCI_PORTSC_STRIDE);
                if (portsc & 0x1) {
                    serial_printf("[EHCI] port %u connected (PORTSC=0x%08x)\n", (unsigned)p + 1, portsc);
                }
            }

            serial_puts("[EHCI] Phase-2 init complete (no HID transfers yet)\n");
            return true;
        }
    }

    serial_puts("[EHCI] controller not found\n");
    return false;
}
