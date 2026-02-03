#include "pci.h"
#include "pci_irq.h"
#include "e1000.h"
#include "Platform/include/serial.h"
#include "Platform/include/network.h"

static e1000_t g_e1000;

static void e1000_irq_handler(uint8_t irq) {
    (void)irq;
    e1000_poll(&g_e1000);
}

int platform_network_init(void) {
    pci_device_t devices[32];
    int found = pci_scan(devices, 32);
    if (found > 32) {
        found = 32;
    }

    serial_printf("PCI devices found: %d\n", found);

    for (int i = 0; i < found; i++) {
        if (devices[i].vendor_id == E1000_VENDOR_ID &&
            devices[i].device_id == E1000_DEVICE_ID) {

            serial_printf("[NETWORK] e1000 BAR0 = 0x%08x size=0x%08x\n",
                          devices[i].bar_addrs[0],
                          devices[i].bar_sizes[0]);
            if (devices[i].bar_is_io[0]) {
                serial_puts("BAR0 is I/O\n");
            } else {
                serial_puts("BAR0 is MMIO\n");
            }

            uint32_t cmd = pci_read_config_dword(
                devices[i].bus,
                devices[i].slot,
                devices[i].func,
                0x04
            );

            cmd |= (1 << 1); // Memory Space
            cmd |= (1 << 2); // Bus Master

            pci_write_config_dword(
                devices[i].bus,
                devices[i].slot,
                devices[i].func,
                0x04,
                cmd
            );

            e1000_init(&g_e1000, &devices[i]);
            g_e1000.ip = 0x0A00020F; /* 10.0.2.15 - default for QEMU user-mode networking */
            pci_irq_register_handler(&devices[i], e1000_irq_handler);
            return 0;
        }
    }
    return -1;
}

void platform_network_poll(void) {
    e1000_poll(&g_e1000);
}
