#include "pci.h"
#include "Platform/include/io.h"

static uint32_t pci_read_bar_size(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t original) {
    pci_write_config_dword(bus, slot, func, offset, 0xFFFFFFFF);
    uint32_t size = pci_read_config_dword(bus, slot, func, offset);
    pci_write_config_dword(bus, slot, func, offset, original);

    if (original & 0x1) {
        size &= 0xFFFFFFFC;
    } else {
        size &= 0xFFFFFFF0;
    }
    if (size == 0) {
        return 0;
    }
    return (~size) + 1;
}

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

    /* Scan all 256 PCI buses */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            /* Check if a device is present in this slot */
            uint32_t vendor_device = pci_read_config_dword((uint8_t)bus, slot, 0, 0x00);
            uint16_t vendor = vendor_device & 0xFFFF;
            if (vendor == 0xFFFF) continue;

            /* Check if this is a multi-function device */
            uint32_t header = pci_read_config_dword((uint8_t)bus, slot, 0, 0x0C);
            int multifunction = header & 0x00800000;

            for (uint8_t func = 0; func < (multifunction ? 8 : 1); func++) {
                vendor_device = pci_read_config_dword((uint8_t)bus, slot, func, 0x00);
                vendor = vendor_device & 0xFFFF;
                if (vendor == 0xFFFF) continue;

                uint16_t device = (vendor_device >> 16) & 0xFFFF;

                if (count < max_devices) {
                    devices[count].bus = (uint8_t)bus;
                    devices[count].slot = slot;
                    devices[count].func = func;
                    devices[count].vendor_id = vendor;
                    devices[count].device_id = device;

                    uint32_t class_reg = pci_read_config_dword((uint8_t)bus, slot, func, 0x08);
                    devices[count].revision = class_reg & 0xFF;
                    devices[count].prog_if = (class_reg >> 8) & 0xFF;
                    devices[count].subclass = (class_reg >> 16) & 0xFF;
                    devices[count].class_code = (class_reg >> 24) & 0xFF;

                    for (int bar = 0; bar < 6; bar++) {
                        devices[count].bars[bar] = 0;
                        devices[count].bar_sizes[bar] = 0;
                    }

                    for (int bar = 0; bar < 6; bar++) {
                        uint8_t offset = (uint8_t)(0x10 + bar * 4);
                        uint32_t bar_val = pci_read_config_dword((uint8_t)bus, slot, func, offset);
                        devices[count].bars[bar] = bar_val;
                        if (bar_val == 0 || bar_val == 0xFFFFFFFF) {
                            continue;
                        }

                        if ((bar_val & 0x1) == 0) {
                            uint32_t type = (bar_val >> 1) & 0x3;
                            if (type == 0x2 && bar < 5) {
                                uint32_t bar_high = pci_read_config_dword((uint8_t)bus, slot, func, offset + 4);
                                pci_write_config_dword((uint8_t)bus, slot, func, offset, 0xFFFFFFFF);
                                pci_write_config_dword((uint8_t)bus, slot, func, offset + 4, 0xFFFFFFFF);
                                uint32_t size_low = pci_read_config_dword((uint8_t)bus, slot, func, offset);
                                uint32_t size_high = pci_read_config_dword((uint8_t)bus, slot, func, offset + 4);
                                pci_write_config_dword((uint8_t)bus, slot, func, offset, bar_val);
                                pci_write_config_dword((uint8_t)bus, slot, func, offset + 4, bar_high);

                                uint64_t size64 = ((uint64_t)size_high << 32) | (size_low & 0xFFFFFFF0);
                                size64 = (~size64) + 1;
                                devices[count].bar_sizes[bar] = (size64 > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)size64;
                                devices[count].bars[bar + 1] = bar_high;
                                devices[count].bar_sizes[bar + 1] = 0;
                                bar++;
                                continue;
                            }
                        }

                        devices[count].bar_sizes[bar] = pci_read_bar_size((uint8_t)bus, slot, func, offset, bar_val);
                    }

                    uint32_t irq_reg = pci_read_config_dword((uint8_t)bus, slot, func, 0x3C);
                    devices[count].irq_line = irq_reg & 0xFF;
                    devices[count].irq_pin = (irq_reg >> 8) & 0xFF;
                }

                count++;
            }
        }
    }

    return count;
}
