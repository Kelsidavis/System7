/*
 * VirtIO PCI Transport for ARM64
 * Common PCI infrastructure for VirtIO devices on QEMU virt machine
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "virtio_pci.h"
#include "uart.h"

/* PCI ECAM base for QEMU virt machine */
#define PCI_ECAM_BASE       0x4010000000ULL

/* PCI MMIO regions for QEMU virt machine */
#define PCI_MMIO_BASE       0x10000000ULL
#define PCI_MMIO_SIZE       0x2eff0000ULL

/* Next available MMIO address for BAR allocation */
static uint64_t next_mmio_addr = PCI_MMIO_BASE;

/* PCI configuration space offsets */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_CLASS_CODE      0x09
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_CAP_PTR         0x34

/* PCI command bits */
#define PCI_COMMAND_IO          0x0001
#define PCI_COMMAND_MEMORY      0x0002
#define PCI_COMMAND_MASTER      0x0004

/* VirtIO PCI vendor/device IDs */
#define PCI_VENDOR_VIRTIO       0x1AF4
#define PCI_DEVICE_NET          0x1000  /* Legacy */
#define PCI_DEVICE_BLK          0x1001  /* Legacy */
#define PCI_DEVICE_GPU          0x1050  /* Modern transitional */
#define PCI_DEVICE_INPUT        0x1052  /* Modern transitional */

/* VirtIO PCI capability types */
#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

/* PCI capability header */
#define PCI_CAP_ID_VNDR         0x09  /* Vendor-specific */

/* Helper to print hex */
static void print_hex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    uart_putc(hex[(value >> 28) & 0xF]);
    uart_putc(hex[(value >> 24) & 0xF]);
    uart_putc(hex[(value >> 20) & 0xF]);
    uart_putc(hex[(value >> 16) & 0xF]);
    uart_putc(hex[(value >> 12) & 0xF]);
    uart_putc(hex[(value >> 8) & 0xF]);
    uart_putc(hex[(value >> 4) & 0xF]);
    uart_putc(hex[value & 0xF]);
}

static void print_hex16(uint16_t value) {
    static const char hex[] = "0123456789ABCDEF";
    uart_putc(hex[(value >> 12) & 0xF]);
    uart_putc(hex[(value >> 8) & 0xF]);
    uart_putc(hex[(value >> 4) & 0xF]);
    uart_putc(hex[value & 0xF]);
}

static void print_dec(int value) {
    if (value < 0) {
        uart_putc('-');
        value = -value;
    }
    if (value >= 10) {
        print_dec(value / 10);
    }
    uart_putc('0' + (value % 10));
}

/* PCI configuration space access */
static inline uintptr_t pci_config_addr(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset) {
    return PCI_ECAM_BASE | ((uint64_t)bus << 20) | ((uint64_t)device << 15) |
           ((uint64_t)func << 12) | offset;
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    return *(volatile uint8_t *)addr;
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    return *(volatile uint16_t *)addr;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    return *(volatile uint32_t *)addr;
}

void pci_config_write8(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint8_t value) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    *(volatile uint8_t *)addr = value;
}

void pci_config_write16(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint16_t value) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    *(volatile uint16_t *)addr = value;
}

void pci_config_write32(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint32_t value) {
    uintptr_t addr = pci_config_addr(bus, device, func, offset);
    *(volatile uint32_t *)addr = value;
}

/* Find VirtIO PCI capability */
static bool find_virtio_cap(uint8_t bus, uint8_t device, uint8_t func,
                            uint8_t cap_type, virtio_pci_cap_t *cap) {
    uint8_t cap_ptr = pci_config_read8(bus, device, func, PCI_CAP_PTR);

    while (cap_ptr != 0) {
        uint8_t cap_id = pci_config_read8(bus, device, func, cap_ptr);

        if (cap_id == PCI_CAP_ID_VNDR) {
            /* This is a vendor capability - check if it's the VirtIO type we want */
            uint8_t cfg_type = pci_config_read8(bus, device, func, cap_ptr + 3);

            if (cfg_type == cap_type) {
                cap->cap_vndr = cap_id;
                cap->cap_next = pci_config_read8(bus, device, func, cap_ptr + 1);
                cap->cap_len = pci_config_read8(bus, device, func, cap_ptr + 2);
                cap->cfg_type = cfg_type;
                cap->bar = pci_config_read8(bus, device, func, cap_ptr + 4);
                cap->offset = pci_config_read32(bus, device, func, cap_ptr + 8);
                cap->length = pci_config_read32(bus, device, func, cap_ptr + 12);
                return true;
            }
        }

        cap_ptr = pci_config_read8(bus, device, func, cap_ptr + 1);
    }

    return false;
}

/* Align value up to alignment boundary */
static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

/* Program a BAR with an MMIO address if not already programmed */
static uint64_t program_bar(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar) {
    uint16_t bar_offset = PCI_BAR0 + bar * 4;

    /* Save original value and disable decoding */
    uint16_t cmd = pci_config_read16(bus, device, func, PCI_COMMAND);
    pci_config_write16(bus, device, func, PCI_COMMAND, cmd & ~(PCI_COMMAND_MEMORY | PCI_COMMAND_IO));

    /* Read current BAR value */
    uint32_t bar_low = pci_config_read32(bus, device, func, bar_offset);
    bool is_io = (bar_low & 1) != 0;
    bool is_64bit = !is_io && ((bar_low & 0x6) == 0x4);

    /* If BAR is already programmed (non-zero address), just return it */
    uint64_t current_addr;
    if (is_64bit) {
        uint32_t bar_high = pci_config_read32(bus, device, func, bar_offset + 4);
        current_addr = ((uint64_t)bar_high << 32) | (bar_low & ~0xFULL);
    } else {
        current_addr = bar_low & ~0xFULL;
    }

    if (current_addr != 0) {
        pci_config_write16(bus, device, func, PCI_COMMAND, cmd);
        return current_addr;
    }

    /* BAR is not programmed - determine size and allocate */
    /* Write all 1s to determine size */
    pci_config_write32(bus, device, func, bar_offset, 0xFFFFFFFF);
    uint32_t size_low = pci_config_read32(bus, device, func, bar_offset);

    if (size_low == 0) {
        /* BAR not implemented */
        pci_config_write32(bus, device, func, bar_offset, bar_low);
        pci_config_write16(bus, device, func, PCI_COMMAND, cmd);
        return 0;
    }

    uint64_t size;
    if (is_64bit) {
        pci_config_write32(bus, device, func, bar_offset + 4, 0xFFFFFFFF);
        uint32_t size_high = pci_config_read32(bus, device, func, bar_offset + 4);
        uint64_t mask = ((uint64_t)size_high << 32) | (size_low & ~0xFULL);
        size = (~mask) + 1;
    } else {
        size = (~(size_low & ~0xFULL)) + 1;
        size &= 0xFFFFFFFF;  /* Ensure 32-bit size */
    }

    /* Allocate MMIO address */
    next_mmio_addr = align_up(next_mmio_addr, size);
    uint64_t addr = next_mmio_addr;
    next_mmio_addr += size;

    /* Program the BAR */
    pci_config_write32(bus, device, func, bar_offset, (uint32_t)addr);
    if (is_64bit) {
        pci_config_write32(bus, device, func, bar_offset + 4, (uint32_t)(addr >> 32));
    }

    /* Re-enable memory decoding */
    pci_config_write16(bus, device, func, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);

    uart_puts("[VIRTIO-PCI] Programmed BAR");
    print_dec(bar);
    uart_puts(" = 0x");
    print_hex32((uint32_t)(addr >> 32));
    print_hex32((uint32_t)addr);
    uart_puts(" size=0x");
    print_hex32((uint32_t)size);
    uart_puts("\n");

    return addr;
}

/* Get BAR address, programming it if necessary */
static uint64_t get_bar_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar) {
    return program_bar(bus, device, func, bar);
}

/* Scan PCI bus for VirtIO devices */
bool virtio_pci_scan(void) {
    uart_puts("[VIRTIO-PCI] Scanning PCI bus...\n");

    int found = 0;

    for (uint8_t device = 0; device < 32; device++) {
        uint16_t vendor = pci_config_read16(0, device, 0, PCI_VENDOR_ID);

        if (vendor == 0xFFFF) continue;

        uint16_t dev_id = pci_config_read16(0, device, 0, PCI_DEVICE_ID);

        uart_puts("[VIRTIO-PCI] Device ");
        print_dec(device);
        uart_puts(": vendor=0x");
        print_hex16(vendor);
        uart_puts(" device=0x");
        print_hex16(dev_id);

        if (vendor == PCI_VENDOR_VIRTIO) {
            uart_puts(" (VirtIO");
            if (dev_id == PCI_DEVICE_GPU || dev_id == 0x1040 + 16) {
                uart_puts(" GPU");
            } else if (dev_id == PCI_DEVICE_INPUT || dev_id == 0x1040 + 18) {
                uart_puts(" Input");
            } else if (dev_id == PCI_DEVICE_BLK || dev_id == 0x1040 + 2) {
                uart_puts(" Block");
            } else if (dev_id == PCI_DEVICE_NET || dev_id == 0x1040 + 1) {
                uart_puts(" Network");
            }
            uart_puts(")");
            found++;
        }

        uart_puts("\n");
    }

    uart_puts("[VIRTIO-PCI] Found ");
    print_dec(found);
    uart_puts(" VirtIO device(s)\n");

    return found > 0;
}

/* Find and initialize a VirtIO PCI device */
bool virtio_pci_find_device(uint16_t device_id, virtio_pci_device_t *dev) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint16_t vendor = pci_config_read16(0, slot, 0, PCI_VENDOR_ID);

        if (vendor != PCI_VENDOR_VIRTIO) continue;

        uint16_t dev_id = pci_config_read16(0, slot, 0, PCI_DEVICE_ID);

        /* Check for both legacy and modern device IDs */
        /* Modern: 0x1040 + device_type, Legacy: varies */
        bool match = false;
        if (device_id == VIRTIO_DEV_GPU) {
            match = (dev_id == PCI_DEVICE_GPU || dev_id == 0x1040 + 16);
        } else if (device_id == VIRTIO_DEV_INPUT) {
            match = (dev_id == PCI_DEVICE_INPUT || dev_id == 0x1040 + 18);
        } else if (device_id == VIRTIO_DEV_BLK) {
            match = (dev_id == PCI_DEVICE_BLK || dev_id == 0x1040 + 2);
        }

        if (!match) continue;

        uart_puts("[VIRTIO-PCI] Found device 0x");
        print_hex16(dev_id);
        uart_puts(" at slot ");
        print_dec(slot);
        uart_puts("\n");

        dev->bus = 0;
        dev->device = slot;
        dev->function = 0;
        dev->device_id = dev_id;

        /* Enable memory access and bus mastering */
        uint16_t cmd = pci_config_read16(0, slot, 0, PCI_COMMAND);
        cmd |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
        pci_config_write16(0, slot, 0, PCI_COMMAND, cmd);

        /* Find VirtIO capabilities */
        virtio_pci_cap_t cap;

        if (find_virtio_cap(0, slot, 0, VIRTIO_PCI_CAP_COMMON_CFG, &cap)) {
            dev->common_cfg_bar = cap.bar;
            dev->common_cfg_offset = cap.offset;
            dev->common_cfg = (volatile virtio_pci_common_cfg_t *)
                (get_bar_address(0, slot, 0, cap.bar) + cap.offset);
            uart_puts("[VIRTIO-PCI] Common config at BAR");
            print_dec(cap.bar);
            uart_puts(" + 0x");
            print_hex32(cap.offset);
            uart_puts("\n");
        } else {
            uart_puts("[VIRTIO-PCI] No common config capability found\n");
            return false;
        }

        if (find_virtio_cap(0, slot, 0, VIRTIO_PCI_CAP_NOTIFY_CFG, &cap)) {
            dev->notify_bar = cap.bar;
            dev->notify_offset = cap.offset;
            /* Read notify_off_multiplier from capability */
            dev->notify_off_multiplier = pci_config_read32(0, slot, 0,
                pci_config_read8(0, slot, 0, PCI_CAP_PTR) + 16);
            dev->notify_base = (volatile uint16_t *)
                (get_bar_address(0, slot, 0, cap.bar) + cap.offset);
            uart_puts("[VIRTIO-PCI] Notify at BAR");
            print_dec(cap.bar);
            uart_puts(" + 0x");
            print_hex32(cap.offset);
            uart_puts("\n");
        }

        if (find_virtio_cap(0, slot, 0, VIRTIO_PCI_CAP_ISR_CFG, &cap)) {
            dev->isr = (volatile uint8_t *)
                (get_bar_address(0, slot, 0, cap.bar) + cap.offset);
        }

        if (find_virtio_cap(0, slot, 0, VIRTIO_PCI_CAP_DEVICE_CFG, &cap)) {
            dev->device_cfg_bar = cap.bar;
            dev->device_cfg_offset = cap.offset;
            dev->device_cfg = (volatile void *)
                (get_bar_address(0, slot, 0, cap.bar) + cap.offset);
            uart_puts("[VIRTIO-PCI] Device config at BAR");
            print_dec(cap.bar);
            uart_puts(" + 0x");
            print_hex32(cap.offset);
            uart_puts("\n");
        }

        /* Get BAR addresses for direct MMIO access if needed */
        dev->bar0 = get_bar_address(0, slot, 0, 0);
        dev->bar1 = get_bar_address(0, slot, 0, 1);

        uart_puts("[VIRTIO-PCI] BAR0=0x");
        print_hex32((uint32_t)dev->bar0);
        uart_puts(" BAR1=0x");
        print_hex32((uint32_t)dev->bar1);
        uart_puts("\n");

        return true;
    }

    return false;
}

/* Initialize VirtIO device (reset and negotiate features) */
bool virtio_pci_init_device(virtio_pci_device_t *dev, uint64_t supported_features) {
    if (!dev->common_cfg) return false;

    volatile virtio_pci_common_cfg_t *cfg = dev->common_cfg;

    /* Reset device */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Wait for reset */
    while (cfg->device_status != 0) {
        __asm__ volatile("" ::: "memory");
    }

    /* Acknowledge device */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Driver loaded */
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Read and negotiate features */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t features_lo = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t features_hi = cfg->device_feature;

    uint64_t device_features = ((uint64_t)features_hi << 32) | features_lo;
    uint64_t negotiated = device_features & supported_features;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = (uint32_t)negotiated;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = (uint32_t)(negotiated >> 32);
    __sync_synchronize();

    /* Features OK */
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Verify features accepted */
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        uart_puts("[VIRTIO-PCI] Feature negotiation failed\n");
        cfg->device_status = VIRTIO_STATUS_FAILED;
        return false;
    }

    return true;
}

/* Setup a virtqueue */
bool virtio_pci_setup_queue(virtio_pci_device_t *dev, uint16_t queue_idx,
                            struct virtq_desc *desc, struct virtq_avail *avail,
                            struct virtq_used *used, uint16_t queue_size) {
    if (!dev->common_cfg) return false;

    volatile virtio_pci_common_cfg_t *cfg = dev->common_cfg;

    /* Select queue */
    cfg->queue_select = queue_idx;
    __sync_synchronize();

    /* Check max queue size */
    uint16_t max_size = cfg->queue_size;
    if (max_size == 0) {
        uart_puts("[VIRTIO-PCI] Queue ");
        print_dec(queue_idx);
        uart_puts(" not available\n");
        return false;
    }

    if (queue_size > max_size) {
        queue_size = max_size;
    }

    /* Set queue size */
    cfg->queue_size = queue_size;
    __sync_synchronize();

    /* Set queue addresses */
    uint64_t desc_addr = (uint64_t)(uintptr_t)desc;
    uint64_t avail_addr = (uint64_t)(uintptr_t)avail;
    uint64_t used_addr = (uint64_t)(uintptr_t)used;

    cfg->queue_desc = desc_addr;
    cfg->queue_driver = avail_addr;
    cfg->queue_device = used_addr;
    __sync_synchronize();

    /* Enable queue */
    cfg->queue_enable = 1;
    __sync_synchronize();

    uart_puts("[VIRTIO-PCI] Queue ");
    print_dec(queue_idx);
    uart_puts(" enabled (size=");
    print_dec(queue_size);
    uart_puts(")\n");

    return true;
}

/* Mark device as ready */
void virtio_pci_device_ready(virtio_pci_device_t *dev) {
    if (!dev->common_cfg) return;

    dev->common_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    uart_puts("[VIRTIO-PCI] Device ready\n");
}

/* Notify a queue */
void virtio_pci_notify_queue(virtio_pci_device_t *dev, uint16_t queue_idx) {
    if (!dev->common_cfg || !dev->notify_base) return;

    /* Get queue notify offset */
    dev->common_cfg->queue_select = queue_idx;
    __sync_synchronize();
    uint16_t notify_off = dev->common_cfg->queue_notify_off;

    /* Calculate notify address */
    volatile uint16_t *notify_addr = (volatile uint16_t *)
        ((uintptr_t)dev->notify_base + notify_off * dev->notify_off_multiplier);

    *notify_addr = queue_idx;
    __sync_synchronize();
}
