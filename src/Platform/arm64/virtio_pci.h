/*
 * VirtIO PCI Transport Header
 * Common definitions for VirtIO PCI devices
 */

#ifndef VIRTIO_PCI_H
#define VIRTIO_PCI_H

#include <stdint.h>
#include <stdbool.h>

/* VirtIO device types */
#define VIRTIO_DEV_NET      1
#define VIRTIO_DEV_BLK      2
#define VIRTIO_DEV_CONSOLE  3
#define VIRTIO_DEV_RNG      4
#define VIRTIO_DEV_GPU      16
#define VIRTIO_DEV_INPUT    18

/* VirtIO status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

/* Virtqueue descriptor flags */
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2
#define VIRTQ_DESC_F_INDIRECT   4

/* VirtIO PCI capability structure */
typedef struct {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;
} virtio_pci_cap_t;

/* VirtIO PCI common configuration structure */
typedef struct {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
} __attribute__((packed)) virtio_pci_common_cfg_t;

/* Virtqueue descriptor */
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

/* Virtqueue available ring */
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

/* Virtqueue used element */
struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

/* Virtqueue used ring */
struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
} __attribute__((packed));

/* VirtIO PCI device structure */
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t device_id;

    /* BAR addresses */
    uint64_t bar0;
    uint64_t bar1;

    /* Capability locations */
    uint8_t common_cfg_bar;
    uint32_t common_cfg_offset;
    uint8_t notify_bar;
    uint32_t notify_offset;
    uint32_t notify_off_multiplier;
    uint8_t device_cfg_bar;
    uint32_t device_cfg_offset;

    /* Memory-mapped regions */
    volatile virtio_pci_common_cfg_t *common_cfg;
    volatile uint16_t *notify_base;
    volatile uint8_t *isr;
    volatile void *device_cfg;
} virtio_pci_device_t;

/* PCI configuration access */
uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset);
uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset);
void pci_config_write8(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint8_t value);
void pci_config_write16(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint16_t value);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint32_t value);

/* VirtIO PCI functions */
void virtio_pci_init_bus(void);  /* Must be called before find_device when multiple devices present */
bool virtio_pci_scan(void);
bool virtio_pci_find_device(uint16_t device_id, virtio_pci_device_t *dev);
bool virtio_pci_init_device(virtio_pci_device_t *dev, uint64_t supported_features);
bool virtio_pci_setup_queue(virtio_pci_device_t *dev, uint16_t queue_idx,
                            struct virtq_desc *desc, struct virtq_avail *avail,
                            struct virtq_used *used, uint16_t queue_size);
void virtio_pci_device_ready(virtio_pci_device_t *dev);
void virtio_pci_notify_queue(virtio_pci_device_t *dev, uint16_t queue_idx);

#endif /* VIRTIO_PCI_H */
