#include "pci.h"
#include "Platform/include/io.h"

static inline void pci_write_address(uint32_t address) {
    hal_outl(PCI_CONFIG_ADDRESS, address);
}

static inline uint32_t pci_read_data() {
    return hal_inl(PCI_CONFIG_DATA);
}

static inline void pci_write_data(uint32_t value) {
    hal_outl(PCI_CONFIG_DATA, value);
}

uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);

    pci_write_address(address);
    return pci_read_data();
}

void pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);

    pci_write_address(address);
    pci_write_data(value);
}

int pci_scan(pci_device_t* devices, int max_devices) {
    int count = 0;
    uint8_t bus = 0;  // QEMU: все устройства на bus 0

    for (uint8_t slot = 0; slot < 32; slot++) {
        // Проверяем, есть ли устройство в слоте 0
        uint32_t vendor_device = pci_read_config_dword(bus, slot, 0, 0x00);
        uint16_t vendor = vendor_device & 0xFFFF;
        if (vendor == 0xFFFF) continue;

        // Узнаём, многофункциональное ли устройство
        uint32_t header = pci_read_config_dword(bus, slot, 0, 0x0C);
        int multifunction = header & 0x00800000;

        for (uint8_t func = 0; func < (multifunction ? 8 : 1); func++) {
            vendor_device = pci_read_config_dword(bus, slot, func, 0x00);
            vendor = vendor_device & 0xFFFF;
            if (vendor == 0xFFFF) continue;

            uint16_t device = (vendor_device >> 16) & 0xFFFF;

            if (count < max_devices) {
                devices[count].bus = bus;
                devices[count].slot = slot;
                devices[count].func = func;
                devices[count].vendor_id = vendor;
                devices[count].device_id = device;

                uint32_t class_reg = pci_read_config_dword(bus, slot, func, 0x08);
                devices[count].revision = class_reg & 0xFF;
                devices[count].prog_if = (class_reg >> 8) & 0xFF;
                devices[count].subclass = (class_reg >> 16) & 0xFF;
                devices[count].class_code = (class_reg >> 24) & 0xFF;

                devices[count].bar0 = pci_read_config_dword(bus, slot, func, 0x10);
            }

            count++;
        }
    }

    return count;
}

