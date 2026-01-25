#include "pci.h"
#include "e1000.h"
#include "Platform/include/serial.h"
#include "Platform/include/network.h"

static e1000_t g_e1000;

int platform_network_init(void) {
    pci_device_t devices[32];
    int found = pci_scan(devices, 32);

    serial_printf("PCI devices found: %d\n", found);

    for (int i = 0; i < found; i++) {
        if (devices[i].vendor_id == E1000_VENDOR_ID &&
            devices[i].device_id == E1000_DEVICE_ID) {

            serial_printf("[NETWORK] e1000 BAR0 = 0x%08x\n", devices[i].bar0);
            if (devices[i].bar0 & 0x1) {
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
            return 0;
        }
    }
    return -1;
}

void platform_network_poll(void) {
    e1000_poll(&g_e1000);
}
