/*
 * VirtIO Block Driver for ARM64
 * Implements virtio-blk device interface for storage in QEMU
 * Supports both PCI and MMIO transports
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uart.h"
#include "virtio_blk.h"
#include "virtio_pci.h"

/* VirtIO MMIO registers */
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
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG          0x100

/* VirtIO MMIO device region (for QEMU virt machine) */
#define VIRTIO_MMIO_BASE_START      0x0a000000
#define VIRTIO_MMIO_SLOT_SIZE       0x00000200

/* VirtIO device IDs */
#define VIRTIO_ID_BLOCK     2

/* VirtIO status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

/* VirtIO descriptor flags */
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2

/* VirtIO block request types */
#define VIRTIO_BLK_T_IN         0   /* Read */
#define VIRTIO_BLK_T_OUT        1   /* Write */
#define VIRTIO_BLK_T_FLUSH      4   /* Flush */
#define VIRTIO_BLK_T_GET_ID     8   /* Get device ID */

/* VirtIO block status */
#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

/* VirtIO block feature bits */
#define VIRTIO_BLK_F_SIZE_MAX   1
#define VIRTIO_BLK_F_SEG_MAX    2
#define VIRTIO_BLK_F_GEOMETRY   4
#define VIRTIO_BLK_F_RO         5
#define VIRTIO_BLK_F_BLK_SIZE   6
#define VIRTIO_BLK_F_FLUSH      9
#define VIRTIO_BLK_F_TOPOLOGY   10

/* Sector size */
#define SECTOR_SIZE             512

/* VirtIO block request header */
struct virtio_blk_req_hdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

/* VirtIO block config (read from MMIO config space) */
struct virtio_blk_config {
    uint64_t capacity;      /* Number of 512-byte sectors */
    uint32_t size_max;      /* Max segment size */
    uint32_t seg_max;       /* Max segments */
    struct {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;      /* Block size */
} __attribute__((packed));

/* Block-specific virtqueue structures (sized for 32 entries) */
#define BLK_QUEUE_SIZE 32

struct blk_virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[BLK_QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed));

struct blk_virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[BLK_QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

/* Virtqueue structure for block */
struct blk_virtqueue {
    struct virtq_desc desc[BLK_QUEUE_SIZE];
    struct blk_virtq_avail avail;
    uint8_t padding[4096 - sizeof(struct virtq_desc) * BLK_QUEUE_SIZE - sizeof(struct blk_virtq_avail)];
    struct blk_virtq_used used;
} __attribute__((aligned(4096)));

/* Driver state */
static bool use_pci = false;
static virtio_pci_device_t pci_dev;
static volatile uint32_t *virtio_blk_base = NULL;
static struct blk_virtqueue requestq __attribute__((aligned(4096)));
static bool blk_initialized = false;
static uint16_t avail_idx = 0;
static uint16_t used_idx = 0;

/* Device info */
static uint64_t total_sectors = 0;
static uint32_t block_size __attribute__((unused)) = SECTOR_SIZE;
static bool device_readonly = false;

/* Request buffers - statically allocated for simplicity */
static struct virtio_blk_req_hdr req_header __attribute__((aligned(16)));
static uint8_t req_status __attribute__((aligned(16)));
static uint8_t data_buffer[SECTOR_SIZE * 8] __attribute__((aligned(4096)));

/* Helper to read MMIO register */
static inline uint32_t virtio_read32(uint32_t offset) {
    return *(volatile uint32_t *)((uintptr_t)virtio_blk_base + offset);
}

/* Helper to write MMIO register */
static inline void virtio_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)((uintptr_t)virtio_blk_base + offset) = value;
}

/* Helper to read config space */
static inline uint32_t virtio_config_read32(uint32_t offset) {
    return *(volatile uint32_t *)((uintptr_t)virtio_blk_base + VIRTIO_MMIO_CONFIG + offset);
}

static inline uint64_t virtio_config_read64(uint32_t offset) {
    uint32_t low = virtio_config_read32(offset);
    uint32_t high = virtio_config_read32(offset + 4);
    return ((uint64_t)high << 32) | low;
}

/* PCI config read helper */
static inline uint64_t pci_config_read_capacity(void) {
    volatile uint64_t *cap = (volatile uint64_t *)pci_dev.device_cfg;
    return *cap;
}

/* Notify the device about queue updates */
static void notify_queue(uint16_t queue_idx) {
    if (use_pci) {
        virtio_pci_notify_queue(&pci_dev, queue_idx);
    } else {
        virtio_write32(VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx);
    }
}

/* Cache maintenance functions */
extern void dcache_clean_range(void *start, size_t length);
extern void dcache_invalidate_range(void *start, size_t length);

/* Send block request and wait for completion */
static bool virtio_blk_request(uint32_t type, uint64_t sector, void *buffer, uint32_t count) {
    if (!blk_initialized) return false;

    /* Setup request header */
    req_header.type = type;
    req_header.reserved = 0;
    req_header.sector = sector;
    req_status = 0xFF;  /* Invalid status to detect completion */

    /* Calculate descriptor indices */
    uint16_t desc_idx = (avail_idx * 3) % 32;

    /* Descriptor 0: Request header (device reads) */
    requestq.desc[desc_idx].addr = (uint64_t)(uintptr_t)&req_header;
    requestq.desc[desc_idx].len = sizeof(req_header);
    requestq.desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    requestq.desc[desc_idx].next = (desc_idx + 1) % 32;

    /* Descriptor 1: Data buffer */
    requestq.desc[(desc_idx + 1) % 32].addr = (uint64_t)(uintptr_t)buffer;
    requestq.desc[(desc_idx + 1) % 32].len = count * SECTOR_SIZE;
    if (type == VIRTIO_BLK_T_IN) {
        /* Read: device writes to buffer */
        requestq.desc[(desc_idx + 1) % 32].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    } else {
        /* Write: device reads from buffer */
        requestq.desc[(desc_idx + 1) % 32].flags = VIRTQ_DESC_F_NEXT;
    }
    requestq.desc[(desc_idx + 1) % 32].next = (desc_idx + 2) % 32;

    /* Descriptor 2: Status byte (device writes) */
    requestq.desc[(desc_idx + 2) % 32].addr = (uint64_t)(uintptr_t)&req_status;
    requestq.desc[(desc_idx + 2) % 32].len = 1;
    requestq.desc[(desc_idx + 2) % 32].flags = VIRTQ_DESC_F_WRITE;
    requestq.desc[(desc_idx + 2) % 32].next = 0;

    /* Add to available ring */
    requestq.avail.ring[avail_idx % 32] = desc_idx;
    __sync_synchronize();
    requestq.avail.idx = ++avail_idx;
    __sync_synchronize();

    /* Clean cache before notifying device */
    dcache_clean_range(&req_header, sizeof(req_header));
    dcache_clean_range(&requestq, sizeof(requestq));
    if (type == VIRTIO_BLK_T_OUT) {
        /* Write: device will read from buffer, clean it */
        dcache_clean_range(buffer, count * SECTOR_SIZE);
    }

    /* Notify device */
    notify_queue(0);

    /* Wait for completion */
    int timeout = 100000;
    while (1) {
        dcache_invalidate_range((void*)&requestq.used, sizeof(requestq.used));
        if (requestq.used.idx != used_idx) break;
        if (--timeout <= 0) break;
        __asm__ volatile("dsb sy" ::: "memory");
    }
    used_idx++;

    /* Invalidate cache after device wrote data */
    dcache_invalidate_range(&req_status, sizeof(req_status));
    if (type == VIRTIO_BLK_T_IN) {
        /* Read: device wrote to buffer, invalidate it */
        dcache_invalidate_range(buffer, count * SECTOR_SIZE);
    }

    return (req_status == VIRTIO_BLK_S_OK);
}

/* Initialize using PCI transport */
static bool init_pci_transport(void) {
    uart_puts("[VIRTIO-BLK] Trying PCI transport...\n");

    /* Scan for VirtIO Block on PCI */
    if (!virtio_pci_find_device(VIRTIO_DEV_BLK, &pci_dev)) {
        uart_puts("[VIRTIO-BLK] No PCI block device found\n");
        return false;
    }

    uart_puts("[VIRTIO-BLK] Found PCI block device\n");

    /* Initialize device with no special features */
    if (!virtio_pci_init_device(&pci_dev, 0)) {
        uart_puts("[VIRTIO-BLK] PCI device init failed\n");
        return false;
    }

    /* Setup request queue (queue 0) */
    if (!virtio_pci_setup_queue(&pci_dev, 0,
                                requestq.desc,
                                (struct virtq_avail *)&requestq.avail,
                                (struct virtq_used *)&requestq.used,
                                32)) {
        uart_puts("[VIRTIO-BLK] Queue setup failed\n");
        return false;
    }

    /* Mark device ready */
    virtio_pci_device_ready(&pci_dev);

    /* Read capacity from PCI config space */
    if (pci_dev.device_cfg) {
        total_sectors = pci_config_read_capacity();
    }

    use_pci = true;
    return true;
}

/* Initialize using MMIO transport */
static bool init_mmio_transport(void) {
    uint32_t magic, version, device_id, features;

    uart_puts("[VIRTIO-BLK] Trying MMIO transport...\n");

    /* Scan all 32 MMIO slots for block device */
    for (int slot = 0; slot < 32; slot++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + (slot * VIRTIO_MMIO_SLOT_SIZE);
        virtio_blk_base = (volatile uint32_t *)base;

        magic = virtio_read32(VIRTIO_MMIO_MAGIC);
        if (magic != 0x74726976) continue;

        device_id = virtio_read32(VIRTIO_MMIO_DEVICE_ID);

        if (device_id == VIRTIO_ID_BLOCK) {
            uart_puts("[VIRTIO-BLK] Found MMIO block device at slot ");
            if (slot < 10) {
                uart_putc('0' + slot);
            } else {
                uart_putc('0' + (slot / 10));
                uart_putc('0' + (slot % 10));
            }
            uart_puts("\n");
            break;
        }
    }

    if (virtio_read32(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_ID_BLOCK) {
        uart_puts("[VIRTIO-BLK] No MMIO block device found\n");
        virtio_blk_base = NULL;
        return false;
    }

    /* Check version */
    version = virtio_read32(VIRTIO_MMIO_VERSION);
    if (version != 1 && version != 2) {
        uart_puts("[VIRTIO-BLK] Unsupported version\n");
        return false;
    }

    /* Reset device */
    virtio_write32(VIRTIO_MMIO_STATUS, 0);

    /* Acknowledge device */
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    /* Set driver status */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Read features */
    features = virtio_read32(VIRTIO_MMIO_DEVICE_FEATURES);

    /* Check if read-only */
    device_readonly = (features & (1 << VIRTIO_BLK_F_RO)) != 0;

    /* Accept no features for simplicity */
    virtio_write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);

    /* Features OK */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK);

    /* Setup request queue (queue 0) */
    virtio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);

    uint32_t max_queue_size = virtio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_queue_size < 32) {
        uart_puts("[VIRTIO-BLK] Queue too small\n");
        return false;
    }

    virtio_write32(VIRTIO_MMIO_QUEUE_NUM, 32);

    /* Set queue addresses */
    uint64_t desc_addr = (uint64_t)(uintptr_t)&requestq.desc;
    uint64_t avail_addr = (uint64_t)(uintptr_t)&requestq.avail;
    uint64_t used_addr = (uint64_t)(uintptr_t)&requestq.used;

    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_LOW, desc_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_HIGH, desc_addr >> 32);
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_addr >> 32);
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_LOW, used_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_HIGH, used_addr >> 32);

    virtio_write32(VIRTIO_MMIO_QUEUE_READY, 1);

    /* Driver OK */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    /* Read device configuration */
    total_sectors = virtio_config_read64(0);  /* Capacity at offset 0 */

    use_pci = false;
    return true;
}

/* Print capacity helper */
static void print_capacity(void) {
    uart_puts("[VIRTIO-BLK] Capacity: ");
    /* Print capacity in MB */
    uint32_t mb = (uint32_t)(total_sectors / 2048);
    if (mb > 0) {
        char buf[16];
        int i = 0;
        uint32_t n = mb;
        if (n == 0) {
            buf[i++] = '0';
        } else {
            char tmp[16];
            int j = 0;
            while (n > 0) {
                tmp[j++] = '0' + (n % 10);
                n /= 10;
            }
            while (j > 0) {
                buf[i++] = tmp[--j];
            }
        }
        buf[i] = '\0';
        uart_puts(buf);
        uart_puts(" MB\n");
    } else {
        uart_puts("< 1 MB\n");
    }
}

/*
 * Initialize virtio-blk device
 */
bool virtio_blk_init(void) {
    uart_puts("[VIRTIO-BLK] Initializing...\n");

    /* Try PCI first, then MMIO */
    if (!init_pci_transport() && !init_mmio_transport()) {
        uart_puts("[VIRTIO-BLK] No block device found on PCI or MMIO\n");
        return false;
    }

    print_capacity();

    if (device_readonly) {
        uart_puts("[VIRTIO-BLK] Device is read-only\n");
    }

    blk_initialized = true;
    uart_puts("[VIRTIO-BLK] Initialized successfully\n");
    return true;
}

/*
 * Read sectors from block device
 */
int virtio_blk_read(uint64_t sector, uint32_t count, void *buffer) {
    if (!blk_initialized) return -1;
    if (sector + count > total_sectors) return -1;
    if (count == 0) return 0;

    /* Read in chunks if necessary */
    uint8_t *buf = (uint8_t *)buffer;
    uint32_t remaining = count;
    uint64_t current_sector = sector;

    while (remaining > 0) {
        uint32_t chunk = (remaining > 8) ? 8 : remaining;

        if (!virtio_blk_request(VIRTIO_BLK_T_IN, current_sector, data_buffer, chunk)) {
            return -1;
        }

        /* Copy from internal buffer to user buffer */
        for (uint32_t i = 0; i < chunk * SECTOR_SIZE; i++) {
            buf[i] = data_buffer[i];
        }

        buf += chunk * SECTOR_SIZE;
        current_sector += chunk;
        remaining -= chunk;
    }

    return (int)count;
}

/*
 * Write sectors to block device
 */
int virtio_blk_write(uint64_t sector, uint32_t count, const void *buffer) {
    if (!blk_initialized) return -1;
    if (device_readonly) return -1;
    if (sector + count > total_sectors) return -1;
    if (count == 0) return 0;

    /* Write in chunks if necessary */
    const uint8_t *buf = (const uint8_t *)buffer;
    uint32_t remaining = count;
    uint64_t current_sector = sector;

    while (remaining > 0) {
        uint32_t chunk = (remaining > 8) ? 8 : remaining;

        /* Copy from user buffer to internal buffer */
        for (uint32_t i = 0; i < chunk * SECTOR_SIZE; i++) {
            data_buffer[i] = buf[i];
        }

        if (!virtio_blk_request(VIRTIO_BLK_T_OUT, current_sector, data_buffer, chunk)) {
            return -1;
        }

        buf += chunk * SECTOR_SIZE;
        current_sector += chunk;
        remaining -= chunk;
    }

    return (int)count;
}

/*
 * Get total number of sectors
 */
uint64_t virtio_blk_get_capacity(void) {
    return total_sectors;
}

/*
 * Get sector size (always 512 for virtio-blk)
 */
uint32_t virtio_blk_get_sector_size(void) {
    return SECTOR_SIZE;
}

/*
 * Check if device is read-only
 */
bool virtio_blk_is_readonly(void) {
    return device_readonly;
}

/*
 * Check if block device is initialized
 */
bool virtio_blk_is_initialized(void) {
    return blk_initialized;
}

/*
 * Flush any cached writes
 */
bool virtio_blk_flush(void) {
    if (!blk_initialized) return false;

    /* Send flush command (no data) */
    req_header.type = VIRTIO_BLK_T_FLUSH;
    req_header.reserved = 0;
    req_header.sector = 0;
    req_status = 0xFF;

    uint16_t desc_idx = (avail_idx * 3) % 32;

    /* Just header and status for flush */
    requestq.desc[desc_idx].addr = (uint64_t)(uintptr_t)&req_header;
    requestq.desc[desc_idx].len = sizeof(req_header);
    requestq.desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    requestq.desc[desc_idx].next = (desc_idx + 1) % 32;

    requestq.desc[(desc_idx + 1) % 32].addr = (uint64_t)(uintptr_t)&req_status;
    requestq.desc[(desc_idx + 1) % 32].len = 1;
    requestq.desc[(desc_idx + 1) % 32].flags = VIRTQ_DESC_F_WRITE;
    requestq.desc[(desc_idx + 1) % 32].next = 0;

    requestq.avail.ring[avail_idx % 32] = desc_idx;
    __sync_synchronize();
    requestq.avail.idx = ++avail_idx;
    __sync_synchronize();

    notify_queue(0);

    int timeout = 100000;
    while (requestq.used.idx == used_idx && --timeout > 0) {
        __asm__ volatile("dsb sy" ::: "memory");
    }
    used_idx++;

    return (req_status == VIRTIO_BLK_S_OK);
}
