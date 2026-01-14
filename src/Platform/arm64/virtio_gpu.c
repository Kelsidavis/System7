/*
 * VirtIO GPU Driver for ARM64
 * Supports both PCI and MMIO transports
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "virtio_gpu.h"
#include "virtio_pci.h"
#include "uart.h"

/* VirtIO MMIO registers (fallback for non-PCI) */
#define VIRTIO_MMIO_MAGIC           0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

/* VirtIO MMIO device region */
#define VIRTIO_MMIO_BASE_START      0x0a000000
#define VIRTIO_MMIO_SLOT_SIZE       0x00000200

/* VirtIO GPU control types */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO         0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D       0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF           0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT              0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH           0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D      0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING  0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING  0x0107

#define VIRTIO_GPU_RESP_OK_NODATA               0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO         0x1101

/* VirtIO GPU formats */
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM        2

/* Framebuffer configuration */
#define FB_WIDTH    320
#define FB_HEIGHT   240
#define QUEUE_SIZE  32

/* VirtIO GPU structures */
struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct virtio_gpu_mem_entry entries[1];
} __attribute__((packed));

/* Virtqueue structures with fixed size */
struct gpu_virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed));

struct gpu_virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

struct gpu_virtqueue {
    struct virtq_desc desc[QUEUE_SIZE];
    struct gpu_virtq_avail avail;
    uint8_t padding[4096 - sizeof(struct virtq_desc) * QUEUE_SIZE - sizeof(struct gpu_virtq_avail)];
    struct gpu_virtq_used used;
} __attribute__((aligned(4096)));

/* Driver state */
static bool use_pci = false;
static virtio_pci_device_t pci_dev;
static volatile uint32_t *mmio_base = NULL;
static struct gpu_virtqueue controlq __attribute__((aligned(4096)));
static uint32_t framebuffer[FB_WIDTH * FB_HEIGHT] __attribute__((aligned(4096)));
static bool initialized = false;
static uint16_t avail_idx = 0;
static uint16_t used_idx = 0;

/* Helper to print hex value */
static void print_hex(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex[(value >> i) & 0xF]);
    }
}

/* MMIO register access */
static inline uint32_t mmio_read32(uint32_t offset) {
    return *(volatile uint32_t *)((uintptr_t)mmio_base + offset);
}

static inline void mmio_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)((uintptr_t)mmio_base + offset) = value;
}

/* Notify the device about queue updates */
static void notify_queue(uint16_t queue_idx) {
    if (use_pci) {
        virtio_pci_notify_queue(&pci_dev, queue_idx);
    } else {
        mmio_write32(VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx);
    }
}

/* Send GPU command and wait for response */
static bool virtio_gpu_send_cmd(void *cmd, size_t cmd_len, void *resp, size_t resp_len) {
    uint16_t desc_idx = (avail_idx * 2) % QUEUE_SIZE;

    /* Setup command descriptor */
    controlq.desc[desc_idx].addr = (uint64_t)(uintptr_t)cmd;
    controlq.desc[desc_idx].len = cmd_len;
    controlq.desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    controlq.desc[desc_idx].next = (desc_idx + 1) % QUEUE_SIZE;

    /* Setup response descriptor */
    controlq.desc[(desc_idx + 1) % QUEUE_SIZE].addr = (uint64_t)(uintptr_t)resp;
    controlq.desc[(desc_idx + 1) % QUEUE_SIZE].len = resp_len;
    controlq.desc[(desc_idx + 1) % QUEUE_SIZE].flags = VIRTQ_DESC_F_WRITE;
    controlq.desc[(desc_idx + 1) % QUEUE_SIZE].next = 0;

    /* Add to available ring */
    controlq.avail.ring[avail_idx % QUEUE_SIZE] = desc_idx;
    __sync_synchronize();
    controlq.avail.idx = ++avail_idx;
    __sync_synchronize();

    /* Notify device */
    notify_queue(0);

    /* Wait for completion */
    int timeout = 100000;
    volatile uint16_t *used_idx_ptr = &controlq.used.idx;
    while (*used_idx_ptr == used_idx && --timeout > 0) {
        __asm__ volatile("dsb sy" ::: "memory");
    }

    if (timeout <= 0) {
        return false;
    }

    used_idx++;
    return true;
}

/* Initialize using PCI transport */
static bool init_pci_transport(void) {
    uart_puts("[VIRTIO-GPU] Trying PCI transport...\n");

    /* Scan for VirtIO GPU on PCI */
    if (!virtio_pci_find_device(VIRTIO_DEV_GPU, &pci_dev)) {
        uart_puts("[VIRTIO-GPU] No PCI GPU device found\n");
        return false;
    }

    uart_puts("[VIRTIO-GPU] Found PCI GPU device\n");

    /* Initialize device with no special features */
    if (!virtio_pci_init_device(&pci_dev, 0)) {
        uart_puts("[VIRTIO-GPU] PCI device init failed\n");
        return false;
    }

    /* Setup control queue (queue 0) */
    if (!virtio_pci_setup_queue(&pci_dev, 0,
                                controlq.desc,
                                (struct virtq_avail *)&controlq.avail,
                                (struct virtq_used *)&controlq.used,
                                QUEUE_SIZE)) {
        uart_puts("[VIRTIO-GPU] Queue setup failed\n");
        return false;
    }

    /* Mark device ready */
    virtio_pci_device_ready(&pci_dev);

    use_pci = true;
    return true;
}

/* Initialize using MMIO transport */
static bool init_mmio_transport(void) {
    uart_puts("[VIRTIO-GPU] Trying MMIO transport...\n");

    /* Scan MMIO slots */
    for (int slot = 0; slot < 32; slot++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + (slot * VIRTIO_MMIO_SLOT_SIZE);
        mmio_base = (volatile uint32_t *)base;

        uint32_t magic = mmio_read32(VIRTIO_MMIO_MAGIC);
        if (magic != 0x74726976) continue;

        uint32_t device_id = mmio_read32(VIRTIO_MMIO_DEVICE_ID);
        if (device_id == VIRTIO_DEV_GPU) {
            uart_puts("[VIRTIO-GPU] Found MMIO GPU at slot ");
            uart_putc('0' + (slot / 10));
            uart_putc('0' + (slot % 10));
            uart_puts("\n");

            /* Reset device */
            mmio_write32(VIRTIO_MMIO_STATUS, 0);

            /* Acknowledge */
            mmio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
            mmio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

            /* Features */
            mmio_read32(VIRTIO_MMIO_DEVICE_FEATURES);
            mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);
            mmio_write32(VIRTIO_MMIO_STATUS,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

            /* Setup queue */
            mmio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);
            uint32_t max_size = mmio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);
            if (max_size < QUEUE_SIZE) {
                uart_puts("[VIRTIO-GPU] Queue too small\n");
                continue;
            }

            mmio_write32(VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);

            uint64_t desc_addr = (uint64_t)(uintptr_t)&controlq.desc;
            uint64_t avail_addr = (uint64_t)(uintptr_t)&controlq.avail;
            uint64_t used_addr = (uint64_t)(uintptr_t)&controlq.used;

            mmio_write32(VIRTIO_MMIO_QUEUE_DESC_LOW, desc_addr & 0xFFFFFFFF);
            mmio_write32(VIRTIO_MMIO_QUEUE_DESC_HIGH, desc_addr >> 32);
            mmio_write32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_addr & 0xFFFFFFFF);
            mmio_write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_addr >> 32);
            mmio_write32(VIRTIO_MMIO_QUEUE_USED_LOW, used_addr & 0xFFFFFFFF);
            mmio_write32(VIRTIO_MMIO_QUEUE_USED_HIGH, used_addr >> 32);

            mmio_write32(VIRTIO_MMIO_QUEUE_READY, 1);

            /* Driver OK */
            mmio_write32(VIRTIO_MMIO_STATUS,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                        VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

            use_pci = false;
            return true;
        }
    }

    uart_puts("[VIRTIO-GPU] No MMIO GPU device found\n");
    return false;
}

bool virtio_gpu_init(void) {
    struct virtio_gpu_ctrl_hdr resp_hdr;

    uart_puts("[VIRTIO-GPU] Initializing...\n");

    /* Try PCI first, then MMIO */
    if (!init_pci_transport() && !init_mmio_transport()) {
        uart_puts("[VIRTIO-GPU] No GPU device found on PCI or MMIO\n");
        return false;
    }

    /* Create 2D resource */
    struct virtio_gpu_resource_create_2d create_cmd;
    create_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create_cmd.hdr.flags = 0;
    create_cmd.hdr.fence_id = 0;
    create_cmd.hdr.ctx_id = 0;
    create_cmd.hdr.padding = 0;
    create_cmd.resource_id = 1;
    create_cmd.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    create_cmd.width = FB_WIDTH;
    create_cmd.height = FB_HEIGHT;

    if (!virtio_gpu_send_cmd(&create_cmd, sizeof(create_cmd), &resp_hdr, sizeof(resp_hdr))) {
        uart_puts("[VIRTIO-GPU] Failed to create resource\n");
        return false;
    }

    /* Attach backing store */
    struct virtio_gpu_resource_attach_backing attach_cmd;
    attach_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_cmd.hdr.flags = 0;
    attach_cmd.hdr.fence_id = 0;
    attach_cmd.hdr.ctx_id = 0;
    attach_cmd.hdr.padding = 0;
    attach_cmd.resource_id = 1;
    attach_cmd.nr_entries = 1;
    attach_cmd.entries[0].addr = (uint64_t)(uintptr_t)framebuffer;
    attach_cmd.entries[0].length = FB_WIDTH * FB_HEIGHT * 4;
    attach_cmd.entries[0].padding = 0;

    if (!virtio_gpu_send_cmd(&attach_cmd, sizeof(attach_cmd), &resp_hdr, sizeof(resp_hdr))) {
        uart_puts("[VIRTIO-GPU] Failed to attach backing\n");
        return false;
    }

    /* Set scanout */
    struct virtio_gpu_set_scanout scanout_cmd;
    scanout_cmd.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout_cmd.hdr.flags = 0;
    scanout_cmd.hdr.fence_id = 0;
    scanout_cmd.hdr.ctx_id = 0;
    scanout_cmd.hdr.padding = 0;
    scanout_cmd.r.x = 0;
    scanout_cmd.r.y = 0;
    scanout_cmd.r.width = FB_WIDTH;
    scanout_cmd.r.height = FB_HEIGHT;
    scanout_cmd.scanout_id = 0;
    scanout_cmd.resource_id = 1;

    if (!virtio_gpu_send_cmd(&scanout_cmd, sizeof(scanout_cmd), &resp_hdr, sizeof(resp_hdr))) {
        uart_puts("[VIRTIO-GPU] Failed to set scanout\n");
        return false;
    }

    initialized = true;
    uart_puts("[VIRTIO-GPU] Initialization complete!\n");
    return true;
}

void virtio_gpu_flush(void) {
    if (!initialized) return;

    struct virtio_gpu_ctrl_hdr resp_hdr;

    /* Transfer to host */
    struct virtio_gpu_transfer_to_host_2d transfer_cmd = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .padding = 0
        },
        .r = { .x = 0, .y = 0, .width = FB_WIDTH, .height = FB_HEIGHT },
        .offset = 0,
        .resource_id = 1,
        .padding = 0
    };

    virtio_gpu_send_cmd(&transfer_cmd, sizeof(transfer_cmd), &resp_hdr, sizeof(resp_hdr));

    /* Flush */
    struct virtio_gpu_resource_flush flush_cmd = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .padding = 0
        },
        .r = { .x = 0, .y = 0, .width = FB_WIDTH, .height = FB_HEIGHT },
        .resource_id = 1,
        .padding = 0
    };

    virtio_gpu_send_cmd(&flush_cmd, sizeof(flush_cmd), &resp_hdr, sizeof(resp_hdr));
}

void virtio_gpu_clear(uint32_t color) {
    if (!initialized) return;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        framebuffer[i] = color;
    }
}

void virtio_gpu_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!initialized) return;
    for (uint32_t py = y; py < y + h && py < FB_HEIGHT; py++) {
        for (uint32_t px = x; px < x + w && px < FB_WIDTH; px++) {
            framebuffer[py * FB_WIDTH + px] = color;
        }
    }
}

uint32_t* virtio_gpu_get_buffer(void) {
    return framebuffer;
}

uint32_t virtio_gpu_get_width(void) {
    return FB_WIDTH;
}

uint32_t virtio_gpu_get_height(void) {
    return FB_HEIGHT;
}

bool virtio_gpu_is_initialized(void) {
    return initialized;
}
