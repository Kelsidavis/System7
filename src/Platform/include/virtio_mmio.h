#ifndef PLATFORM_VIRTIO_MMIO_H
#define PLATFORM_VIRTIO_MMIO_H

/* VirtIO MMIO register map shared by ARM and ARM64 drivers. */
#define VIRTIO_MMIO_MAGIC_VALUE          0x000
#define VIRTIO_MMIO_VERSION               0x004
#define VIRTIO_MMIO_DEVICE_ID             0x008
#define VIRTIO_MMIO_VENDOR_ID             0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES       0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL   0x014
#define VIRTIO_MMIO_DRIVER_FEATURES       0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL   0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE       0x028 /* Legacy transport only */
#define VIRTIO_MMIO_QUEUE_SEL             0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX         0x034
#define VIRTIO_MMIO_QUEUE_NUM             0x038
#define VIRTIO_MMIO_QUEUE_ALIGN           0x03c /* Legacy transport only */
#define VIRTIO_MMIO_QUEUE_PFN             0x040 /* Legacy transport only */
#define VIRTIO_MMIO_QUEUE_READY           0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY          0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS      0x060
#define VIRTIO_MMIO_INTERRUPT_ACK         0x064
#define VIRTIO_MMIO_STATUS                0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW        0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH       0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW       0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH      0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW        0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH       0x0a4
#define VIRTIO_MMIO_CONFIG                0x100

/* QEMU virt machine's VirtIO MMIO device window. */
#define VIRTIO_MMIO_BASE_START            0x0a000000
#define VIRTIO_MMIO_SLOT_SIZE             0x00000200

#endif
