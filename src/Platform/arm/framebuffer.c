/*
 * ARM Framebuffer Integration
 * Provides framebuffer initialization for Raspberry Pi (VideoCore)
 * and QEMU virt machines (virtio-gpu)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "System71StdLib.h"
#include "QuickDraw.h"
#include "Platform/include/boot.h"
#include "device_tree.h"
#include "mmio.h"

#ifndef QEMU_VIRT
#include "videocore.h"
#endif

int arm_framebuffer_init(void);
int arm_framebuffer_get_info(hal_framebuffer_info_t *info);
int arm_set_framebuffer_size(uint32_t width, uint32_t height, uint32_t depth);
void arm_clear_framebuffer(uint32_t color);
void arm_draw_test_pattern(void);
void arm_get_framebuffer_size(uint32_t *width, uint32_t *height);
int arm_framebuffer_is_ready(void);
int arm_framebuffer_present(void);

static hal_framebuffer_info_t g_fb_info;
static int framebuffer_ready = 0;

#ifndef QEMU_VIRT

static videocore_fb_t arm_framebuffer;

static void rpi_store_fb_info(void) {
    g_fb_info.framebuffer = (void *)(uintptr_t)arm_framebuffer.fb_address;
    g_fb_info.width = arm_framebuffer.width;
    g_fb_info.height = arm_framebuffer.height;
    g_fb_info.pitch = arm_framebuffer.pitch;
    g_fb_info.depth = arm_framebuffer.depth;
    g_fb_info.red_offset = 16;
    g_fb_info.red_size = 8;
    g_fb_info.green_offset = 8;
    g_fb_info.green_size = 8;
    g_fb_info.blue_offset = 0;
    g_fb_info.blue_size = 8;
}

static int rpi_framebuffer_init(void) {
    Serial_WriteString("[FB] Initializing ARM framebuffer (VideoCore)\n");

    if (videocore_init() != 0) {
        Serial_WriteString("[FB] Failed to initialize VideoCore\n");
        return -1;
    }

    arm_framebuffer.width = 1024;
    arm_framebuffer.height = 768;
    arm_framebuffer.depth = 32;

    if (videocore_allocate_fb(&arm_framebuffer) != 0) {
        Serial_WriteString("[FB] Failed to allocate framebuffer from GPU\n");
        return -1;
    }

    rpi_store_fb_info();
    framebuffer_ready = 1;

    Serial_Printf("[FB] Framebuffer ready: %lux%lu @ %lu-bit (pitch %lu)\n",
                  (unsigned long)arm_framebuffer.width,
                  (unsigned long)arm_framebuffer.height,
                  (unsigned long)arm_framebuffer.depth,
                  (unsigned long)arm_framebuffer.pitch);
    Serial_Printf("[FB] Physical address: 0x%lx\n",
                  (unsigned long)arm_framebuffer.fb_address);
    return 0;
}

static int rpi_set_framebuffer_size(uint32_t width, uint32_t height, uint32_t depth) {
    if (!videocore_mbox_base) {
        Serial_WriteString("[FB] VideoCore not initialized\n");
        return -1;
    }

    Serial_Printf("[FB] Requesting framebuffer resize: %lux%lu @ %lu-bit\n",
                  (unsigned long)width, (unsigned long)height, (unsigned long)depth);

    if (videocore_set_fb_size(width, height, depth) != 0) {
        Serial_WriteString("[FB] Failed to resize framebuffer\n");
        return -1;
    }

    if (videocore_get_fb_info(&arm_framebuffer) != 0) {
        Serial_WriteString("[FB] Failed to fetch framebuffer info after resize\n");
        return -1;
    }

    rpi_store_fb_info();
    return 0;
}

#else /* QEMU_VIRT */

#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4

#define VIRTIO_STATUS_ACKNOWLEDGE       0x01
#define VIRTIO_STATUS_DRIVER            0x02
#define VIRTIO_STATUS_FEATURES_OK       0x08
#define VIRTIO_STATUS_DRIVER_OK         0x04
#define VIRTIO_STATUS_FAILED            0x80

#define VIRTIO_GPU_DEVICE_ID            16
#define VIRTIO_GPU_QUEUE_INDEX_CONTROL  0
#define VIRTIO_GPU_QUEUE_CAPACITY       8
#define VIRTIO_GPU_TIMEOUT              1000000

#define VIRTIO_GPU_RESOURCE_ID          1
#define VIRTIO_GPU_SCANOUT_ID           0

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO         0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D       0x0101
#define VIRTIO_GPU_CMD_SET_SCANOUT              0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH           0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D      0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING  0x0106

#define VIRTIO_GPU_RESP_OK_NODATA               0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO         0x1101

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM        1

#define VRING_DESC_F_NEXT  1u
#define VRING_DESC_F_WRITE 2u

#define VIRT_GPU_MAX_WIDTH        1920u
#define VIRT_GPU_MAX_HEIGHT       1080u
#define VIRT_GPU_BYTES_PER_PIXEL  4u
#define VIRT_GPU_MAX_FB_SIZE      (VIRT_GPU_MAX_WIDTH * VIRT_GPU_MAX_HEIGHT * VIRT_GPU_BYTES_PER_PIXEL)

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_GPU_QUEUE_CAPACITY];
    uint16_t used_event;
} virtq_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTIO_GPU_QUEUE_CAPACITY];
    uint16_t avail_event;
} virtq_used_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} virtio_gpu_ctrl_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_rect_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint32_t enabled;
    uint32_t flags;
} virtio_gpu_display_one_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[16];
} virtio_gpu_resp_display_info_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_resource_attach_backing_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_resource_attach_backing_t backing;
    virtio_gpu_mem_entry_t entry;
} virtio_gpu_attach_backing_cmd_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t scanout_id;
    virtio_gpu_rect_t rect;
    uint32_t resource_id;
} virtio_gpu_set_scanout_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint64_t offset;
} virtio_gpu_transfer_to_host_2d_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_flush_t;

static volatile virtq_desc_t g_desc[VIRTIO_GPU_QUEUE_CAPACITY] __attribute__((aligned(16)));
static volatile virtq_avail_t g_avail __attribute__((aligned(16)));
static volatile virtq_used_t g_used __attribute__((aligned(16)));
static uint16_t g_queue_size = 0;
static uint16_t g_avail_idx = 0;
static uint16_t g_last_used_idx = 0;

static uint32_t g_virtio_base = 0;
static uint32_t g_current_width = 0;
static uint32_t g_current_height = 0;
static uint8_t g_framebuffer_storage[VIRT_GPU_MAX_FB_SIZE] __attribute__((aligned(4096)));

static inline void memory_barrier(void) {
    __sync_synchronize();
}

static void clear_volatile(volatile void *ptr, size_t size) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    for (size_t i = 0; i < size; ++i) {
        p[i] = 0;
    }
}

static int virtio_gpu_wait_for_completion(uint32_t base) {
    uint32_t timeout = VIRTIO_GPU_TIMEOUT;
    while (timeout--) {
        uint16_t used_idx = g_used.idx;
        if (used_idx != g_last_used_idx) {
            g_last_used_idx = used_idx;
            uint32_t isr = mmio_read32(base + VIRTIO_MMIO_INTERRUPT_STATUS);
            if (isr) {
                mmio_write32(base + VIRTIO_MMIO_INTERRUPT_ACK, isr);
            }
            return 0;
        }
    }
    Serial_WriteString("[FB] virtio-gpu command timeout\n");
    return -1;
}

static int virtio_gpu_send_cmd(uint32_t base,
                               void *request, uint32_t request_len,
                               void *response, uint32_t response_len) {
    if (g_queue_size < 2) {
        Serial_WriteString("[FB] virtio-gpu queue not initialized\n");
        return -1;
    }

    g_desc[0].addr = (uint64_t)(uintptr_t)request;
    g_desc[0].len = request_len;
    g_desc[0].flags = VRING_DESC_F_NEXT;
    g_desc[0].next = 1;

    g_desc[1].addr = (uint64_t)(uintptr_t)response;
    g_desc[1].len = response_len;
    g_desc[1].flags = VRING_DESC_F_WRITE;
    g_desc[1].next = 0;

    g_avail.ring[g_avail_idx % g_queue_size] = 0;
    memory_barrier();
    g_avail.idx = g_avail_idx + 1;
    memory_barrier();
    g_avail_idx++;

    mmio_write32(base + VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_GPU_QUEUE_INDEX_CONTROL);

    if (virtio_gpu_wait_for_completion(base) != 0) {
        return -1;
    }

    return 0;
}

static int virtio_gpu_send_ok_nodata(uint32_t base, void *request, uint32_t request_len) {
    virtio_gpu_ctrl_hdr_t response;
    memset(&response, 0, sizeof(response));

    if (virtio_gpu_send_cmd(base, request, request_len, &response, sizeof(response)) != 0) {
        return -1;
    }

    if (response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        Serial_Printf("[FB] virtio-gpu unexpected response 0x%x\n", response.type);
        return -1;
    }

    return 0;
}

static int virtio_gpu_setup_queue(uint32_t base) {
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, VIRTIO_GPU_QUEUE_INDEX_CONTROL);
    uint32_t max_entries = mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_entries == 0) {
        Serial_WriteString("[FB] virtio-gpu queue not available\n");
        return -1;
    }

    g_queue_size = (uint16_t)((max_entries < VIRTIO_GPU_QUEUE_CAPACITY) ? max_entries : VIRTIO_GPU_QUEUE_CAPACITY);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, g_queue_size);

    uintptr_t desc_addr = (uintptr_t)g_desc;
    uintptr_t avail_addr = (uintptr_t)&g_avail;
    uintptr_t used_addr = (uintptr_t)&g_used;

    mmio_write32(base + VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)desc_addr);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)avail_addr);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)used_addr);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_USED_HIGH, 0);

    clear_volatile(g_desc, sizeof(g_desc));
    clear_volatile(&g_avail, sizeof(g_avail));
    clear_volatile(&g_used, sizeof(g_used));

    g_avail_idx = 0;
    g_last_used_idx = 0;

    mmio_write32(base + VIRTIO_MMIO_QUEUE_READY, 1);
    return 0;
}

static int virtio_gpu_get_display_info(uint32_t base, uint32_t *width, uint32_t *height) {
    virtio_gpu_ctrl_hdr_t request;
    virtio_gpu_resp_display_info_t response;

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));

    request.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    if (virtio_gpu_send_cmd(base, &request, sizeof(request), &response, sizeof(response)) != 0) {
        return -1;
    }

    if (response.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        Serial_Printf("[FB] virtio-gpu display info failed (0x%x)\n", response.hdr.type);
        return -1;
    }

    for (int i = 0; i < 16; ++i) {
        if (response.pmodes[i].enabled) {
            *width = response.pmodes[i].rect.width;
            *height = response.pmodes[i].rect.height;
            return 0;
        }
    }

    *width = response.pmodes[0].rect.width;
    *height = response.pmodes[0].rect.height;
    return 0;
}

static int virtio_gpu_configure_scanout(uint32_t base,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t framebuffer_bytes) {
    virtio_gpu_resource_create_2d_t create;
    virtio_gpu_attach_backing_cmd_t attach;
    virtio_gpu_set_scanout_t set;
    memset(&create, 0, sizeof(create));
    memset(&attach, 0, sizeof(attach));
    memset(&set, 0, sizeof(set));

    create.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create.resource_id = VIRTIO_GPU_RESOURCE_ID;
    create.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    create.width = width;
    create.height = height;

    if (virtio_gpu_send_ok_nodata(base, &create, sizeof(create)) != 0) {
        Serial_WriteString("[FB] virtio-gpu resource_create_2d failed\n");
        return -1;
    }

    attach.backing.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach.backing.resource_id = VIRTIO_GPU_RESOURCE_ID;
    attach.backing.nr_entries = 1;
    attach.entry.addr = (uint64_t)(uintptr_t)g_framebuffer_storage;
    attach.entry.length = framebuffer_bytes;
    attach.entry.padding = 0;

    if (virtio_gpu_send_ok_nodata(base, &attach, sizeof(attach)) != 0) {
        Serial_WriteString("[FB] virtio-gpu attach_backing failed\n");
        return -1;
    }

    set.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    set.scanout_id = VIRTIO_GPU_SCANOUT_ID;
    set.rect.x = 0;
    set.rect.y = 0;
    set.rect.width = width;
    set.rect.height = height;
    set.resource_id = VIRTIO_GPU_RESOURCE_ID;

    if (virtio_gpu_send_ok_nodata(base, &set, sizeof(set)) != 0) {
        Serial_WriteString("[FB] virtio-gpu set_scanout failed\n");
        return -1;
    }

    return 0;
}

static int virtio_gpu_sync_display(uint32_t base, uint32_t width, uint32_t height) {
    virtio_gpu_transfer_to_host_2d_t transfer;
    virtio_gpu_resource_flush_t flush;
    virtio_gpu_ctrl_hdr_t response;

    memset(&transfer, 0, sizeof(transfer));
    memset(&flush, 0, sizeof(flush));
    memset(&response, 0, sizeof(response));

    transfer.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    transfer.rect.x = 0;
    transfer.rect.y = 0;
    transfer.rect.width = width;
    transfer.rect.height = height;
    transfer.offset = 0;

    if (virtio_gpu_send_cmd(base, &transfer, sizeof(transfer), &response, sizeof(response)) != 0) {
        Serial_WriteString("[FB] virtio-gpu transfer_to_host failed\n");
        return -1;
    }
    if (response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        Serial_Printf("[FB] virtio-gpu transfer unexpected resp 0x%x\n", response.type);
        return -1;
    }

    memset(&response, 0, sizeof(response));
    flush.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush.rect.x = 0;
    flush.rect.y = 0;
    flush.rect.width = width;
    flush.rect.height = height;
    flush.resource_id = VIRTIO_GPU_RESOURCE_ID;

    if (virtio_gpu_send_cmd(base, &flush, sizeof(flush), &response, sizeof(response)) != 0) {
        Serial_WriteString("[FB] virtio-gpu resource_flush failed\n");
        return -1;
    }
    if (response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        Serial_Printf("[FB] virtio-gpu flush unexpected resp 0x%x\n", response.type);
        return -1;
    }

    return 0;
}

static int virtio_gpu_present(void) {
    if (!g_virtio_base || g_current_width == 0 || g_current_height == 0) {
        return -1;
    }
    return virtio_gpu_sync_display(g_virtio_base, g_current_width, g_current_height);
}

static int virtio_gpu_initialize(uint32_t base) {
    mmio_write32(base + VIRTIO_MMIO_STATUS, 0);
    mmio_write32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    mmio_write32(base + VIRTIO_MMIO_STATUS,
                 mmio_read32(base + VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write32(base + VIRTIO_MMIO_STATUS,
                 mmio_read32(base + VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);

    uint32_t status = mmio_read32(base + VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        Serial_WriteString("[FB] virtio-gpu feature negotiation failed\n");
        return -1;
    }

    if (virtio_gpu_setup_queue(base) != 0) {
        mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return -1;
    }

    mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_DRIVER_OK);
    return 0;
}

static int virt_framebuffer_init(void) {
    device_tree_reg_t regs[32];
    size_t count = device_tree_find_compatible("virtio,mmio", regs, 32);
    if (count == 0) {
        Serial_WriteString("[FB] No virtio-mmio nodes found in device tree\n");
        return -1;
    }

    uint32_t base = 0;
    for (size_t i = 0; i < count; ++i) {
        if (regs[i].base > 0xFFFFFFFFu) {
            continue;
        }
        uint32_t candidate = (uint32_t)regs[i].base;
        uint32_t magic = mmio_read32(candidate + VIRTIO_MMIO_MAGIC_VALUE);
        uint32_t device = mmio_read32(candidate + VIRTIO_MMIO_DEVICE_ID);

        if (magic == 0x74726976 && device == VIRTIO_GPU_DEVICE_ID) {
            base = candidate;
            break;
        }
    }

    if (!base) {
        Serial_WriteString("[FB] virtio-gpu device not found\n");
        return -1;
    }

    if (virtio_gpu_initialize(base) != 0) {
        return -1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (virtio_gpu_get_display_info(base, &width, &height) != 0) {
        return -1;
    }

    if (width == 0 || height == 0) {
        width = 1024;
        height = 768;
    }

    if (width > VIRT_GPU_MAX_WIDTH || height > VIRT_GPU_MAX_HEIGHT) {
        Serial_WriteString("[FB] Requested resolution exceeds buffer limit\n");
        return -1;
    }

    /* Check for integer overflow before multiplication */
    if (width > UINT32_MAX / height ||
        (uint32_t)width * height > UINT32_MAX / VIRT_GPU_BYTES_PER_PIXEL) {
        Serial_WriteString("[FB] Framebuffer size calculation overflow\n");
        return -1;
    }

    uint32_t fb_bytes = width * height * VIRT_GPU_BYTES_PER_PIXEL;

    /* Verify result fits in allocated storage */
    if (fb_bytes > VIRT_GPU_MAX_FB_SIZE) {
        Serial_WriteString("[FB] Framebuffer size exceeds storage limit\n");
        return -1;
    }

    memset(g_framebuffer_storage, 0, fb_bytes);

    if (virtio_gpu_configure_scanout(base, width, height, fb_bytes) != 0) {
        return -1;
    }

    g_fb_info.framebuffer = g_framebuffer_storage;
    g_fb_info.width = width;
    g_fb_info.height = height;
    g_fb_info.pitch = width * VIRT_GPU_BYTES_PER_PIXEL;
    g_fb_info.depth = 32;
    g_fb_info.red_offset = 16;
    g_fb_info.red_size = 8;
    g_fb_info.green_offset = 8;
    g_fb_info.green_size = 8;
    g_fb_info.blue_offset = 0;
    g_fb_info.blue_size = 8;

    g_virtio_base = base;
    g_current_width = width;
    g_current_height = height;

    if (virtio_gpu_present() != 0) {
        Serial_WriteString("[FB] Warning: initial display sync failed\n");
    }

    framebuffer_ready = 1;
    Serial_Printf("[FB] virtio-gpu framebuffer ready: %ux%u @ 32-bit\n", width, height);
    return 0;
}

#endif /* QEMU_VIRT */

int arm_framebuffer_init(void) {
    memset(&g_fb_info, 0, sizeof(g_fb_info));
    framebuffer_ready = 0;
#ifndef QEMU_VIRT
    return rpi_framebuffer_init();
#else
    return virt_framebuffer_init();
#endif
}

int arm_framebuffer_get_info(hal_framebuffer_info_t *info) {
    if (!framebuffer_ready || !info) {
        return -1;
    }

    memcpy(info, &g_fb_info, sizeof(g_fb_info));
    return 0;
}

int arm_set_framebuffer_size(uint32_t width, uint32_t height, uint32_t depth) {
#ifndef QEMU_VIRT
    if (rpi_set_framebuffer_size(width, height, depth) == 0) {
        framebuffer_ready = 1;
        return 0;
    }
    return -1;
#else
    (void)width;
    (void)height;
    (void)depth;
    Serial_WriteString("[FB] virtio-gpu dynamic resize not yet supported\n");
    return -1;
#endif
}

void arm_clear_framebuffer(uint32_t color) {
    if (!framebuffer_ready || !g_fb_info.framebuffer || g_fb_info.pitch == 0) {
        return;
    }

    uint32_t row_pixels = g_fb_info.pitch / 4;
    uint32_t total_pixels = row_pixels * g_fb_info.height;
    uint32_t *fb = (uint32_t *)g_fb_info.framebuffer;

    for (uint32_t i = 0; i < total_pixels; ++i) {
        fb[i] = color;
    }

#ifdef QEMU_VIRT
    (void)virtio_gpu_present();
#endif
}

void arm_draw_test_pattern(void) {
    if (!framebuffer_ready || !g_fb_info.framebuffer) {
        return;
    }

    uint32_t width = g_fb_info.width;
    uint32_t height = g_fb_info.height;
    uint32_t pitch_pixels = g_fb_info.pitch / 4;
    uint32_t *fb = (uint32_t *)g_fb_info.framebuffer;

    uint32_t colors[] = {
        0x00FF0000u,  /* Red */
        0x0000FF00u,  /* Green */
        0x000000FFu,  /* Blue */
        0x00FFFFFFu   /* White */
    };

    uint32_t half_width = width / 2;
    uint32_t half_height = height / 2;

    for (uint32_t y = 0; y < half_height; ++y) {
        for (uint32_t x = 0; x < half_width; ++x) {
            fb[y * pitch_pixels + x] = colors[0];
        }
        for (uint32_t x = half_width; x < width; ++x) {
            fb[y * pitch_pixels + x] = colors[1];
        }
    }

    for (uint32_t y = half_height; y < height; ++y) {
        for (uint32_t x = 0; x < half_width; ++x) {
            fb[y * pitch_pixels + x] = colors[2];
        }
        for (uint32_t x = half_width; x < width; ++x) {
            fb[y * pitch_pixels + x] = colors[3];
        }
    }

#ifdef QEMU_VIRT
    (void)virtio_gpu_present();
#endif
    Serial_WriteString("[FB] Test pattern displayed\n");
}

void arm_get_framebuffer_size(uint32_t *width, uint32_t *height) {
    if (width) {
        *width = g_fb_info.width;
    }
    if (height) {
        *height = g_fb_info.height;
    }
}

int arm_framebuffer_is_ready(void) {
    return framebuffer_ready;
}

int arm_framebuffer_present(void) {
#ifdef QEMU_VIRT
    if (!framebuffer_ready) {
        return -1;
    }
    return virtio_gpu_present();
#else
    (void)framebuffer_ready;
    return 0;
#endif
}
