/*
 * VideoCore GPU Implementation
 * Mailbox protocol for Raspberry Pi framebuffer allocation
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "mmio.h"
#include "videocore.h"

/* Global mailbox base address - set by videocore_init() */
uint32_t videocore_mbox_base = 0;

/* Cached framebuffer info */
static videocore_fb_t cached_fb = {0};
static int fb_cached = 0;

/*
 * Detect Raspberry Pi model and set mailbox base address
 * Pi 3/4: 0x3F00B880
 * Pi 5: 0xFC00B880
 */
static int detect_rpi_model(void) {
    /* Try reading from Pi 3/4 mailbox first */
    volatile uint32_t *status_34 = (volatile uint32_t *)VIDEOCORE_MBOX_BASE_PI34 + (MBOX_STATUS / 4);
    volatile uint32_t *status_5 = (volatile uint32_t *)VIDEOCORE_MBOX_BASE_PI5 + (MBOX_STATUS / 4);

    /* Simple heuristic: try Pi 3/4 first (more common)
     * In production, would read device tree or use other detection
     */
    uint32_t val34 = *status_34;
    if ((val34 & ~0xC0000000) == 0) {
        /* Looks like valid status register */
        videocore_mbox_base = VIDEOCORE_MBOX_BASE_PI34;
        Serial_WriteString("[VC] Detected Raspberry Pi 3/4 mailbox\n");
        return 0;
    }

    /* Try Pi 5 */
    uint32_t val5 = *status_5;
    if ((val5 & ~0xC0000000) == 0) {
        videocore_mbox_base = VIDEOCORE_MBOX_BASE_PI5;
        Serial_WriteString("[VC] Detected Raspberry Pi 5 mailbox\n");
        return 0;
    }

    /* Default to Pi 3/4 */
    videocore_mbox_base = VIDEOCORE_MBOX_BASE_PI34;
    Serial_WriteString("[VC] Using default Pi 3/4 mailbox address\n");
    return 0;
}

/*
 * Wait for mailbox to be ready (not full for write, not empty for read)
 */
static int wait_mailbox_ready(int writing) {
    volatile uint32_t *status_reg = (volatile uint32_t *)(videocore_mbox_base + MBOX_STATUS);

    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = mmio_read32((uint32_t)status_reg);

        if (writing) {
            /* Wait for write queue to not be full */
            if (!(status & MBOX_STATUS_FULL)) {
                return 0;
            }
        } else {
            /* Wait for read queue to not be empty */
            if (!(status & MBOX_STATUS_EMPTY)) {
                return 0;
            }
        }
    }

    return -1;  /* Timeout */
}

/*
 * Send mailbox message
 * message should be aligned to 16 bytes
 */
int videocore_mbox_send(uint8_t channel, uint32_t *message, uint32_t message_len) {
    (void)message_len;
    if (!videocore_mbox_base) {
        return -1;
    }

    if (wait_mailbox_ready(1) != 0) {
        Serial_WriteString("[VC] Mailbox send timeout\n");
        return -1;
    }

    /* Construct mailbox message: upper 28 bits = address, lower 4 bits = channel */
    uint32_t msg_addr = ((uint32_t)message & 0xFFFFFFF0) | (channel & 0xF);

    volatile uint32_t *write_reg = (volatile uint32_t *)(videocore_mbox_base + MBOX_WRITE);
    mmio_write32((uint32_t)write_reg, msg_addr);
    mmio_memory_barrier();

    return 0;
}

/*
 * Receive mailbox message
 * Blocks until message arrives
 */
int videocore_mbox_recv(uint8_t channel, uint32_t *message, uint32_t max_len) {
    if (!videocore_mbox_base) {
        return -1;
    }

    volatile uint32_t *read_reg = (volatile uint32_t *)(videocore_mbox_base + MBOX_READ);

    while (1) {
        if (wait_mailbox_ready(0) != 0) {
            Serial_WriteString("[VC] Mailbox recv timeout\n");
            return -1;
        }

        uint32_t msg_addr = mmio_read32((uint32_t)read_reg);
        uint8_t msg_channel = msg_addr & 0xF;
        uint32_t *msg_ptr = (uint32_t *)(msg_addr & 0xFFFFFFF0);

        if (msg_channel == channel) {
            /* This is our message */
            if (message && max_len > 0) {
                /* Copy message (at least first dword which is message size) */
                uint32_t size = msg_ptr[0];
                if (size > max_len) {
                    size = max_len;
                }
                memcpy(message, msg_ptr, size);
            }
            return 0;
        }

        /* Wrong channel, keep trying */
    }
}

/*
 * Initialize VideoCore mailbox interface
 */
int videocore_init(void) {
    Serial_WriteString("[VC] Initializing VideoCore mailbox\n");

    if (detect_rpi_model() != 0) {
        Serial_WriteString("[VC] Failed to detect Raspberry Pi model\n");
        return -1;
    }

    Serial_Printf("[VC] Mailbox base: 0x%lx\n", (unsigned long)videocore_mbox_base);
    return 0;
}

/*
 * Allocate framebuffer from VideoCore GPU
 * Creates a message buffer on stack and sends to GPU
 */
int videocore_allocate_fb(videocore_fb_t *fb_info) {
    if (!fb_info || !videocore_mbox_base) {
        return -1;
    }

    /* Framebuffer allocation message buffer
     * Must be 16-byte aligned
     * Format: [size, request_code, tag1, tag2, ..., end_tag]
     */
    uint32_t message[256] __attribute__((aligned(16)));
    memset(message, 0, sizeof(message));

    uint32_t *ptr = message;

    /* Message header */
    *ptr++ = 0;  /* Size - will be filled in later */
    *ptr++ = 0;  /* Request code (0 = process request) */

    /* Tag: Set physical width/height */
    *ptr++ = MBOX_TAG_SET_PHYS_WH;
    *ptr++ = 8;              /* Value buffer size */
    *ptr++ = 0;              /* Request? Actually response size */
    *ptr++ = fb_info->width ? fb_info->width : 1024;    /* Width */
    *ptr++ = fb_info->height ? fb_info->height : 768;   /* Height */

    /* Tag: Set depth */
    *ptr++ = MBOX_TAG_SET_DEPTH;
    *ptr++ = 4;              /* Value buffer size */
    *ptr++ = 0;              /* Response size */
    *ptr++ = fb_info->depth ? fb_info->depth : 32;      /* Depth in bits */

    /* Tag: Allocate framebuffer */
    *ptr++ = MBOX_TAG_ALLOCATE_FB;
    *ptr++ = 8;              /* Value buffer size */
    *ptr++ = 0;              /* Response size */
    *ptr++ = 16;             /* Alignment (16 bytes) */
    *ptr++ = 0;              /* Size (GPU fills in) */

    /* End tag */
    *ptr++ = 0;

    /* Update message size */
    uint32_t msg_size = ((uint32_t)ptr - (uint32_t)message);
    message[0] = msg_size;

    Serial_Printf("[VC] Sending framebuffer allocation request (%lu bytes)\n",
                  (unsigned long)msg_size);

    /* Send to GPU (channel 8 = ARM to VideoCore) */
    if (videocore_mbox_send(MBOX_CHANNEL_PROP_ARM2VC, message, msg_size) != 0) {
        Serial_WriteString("[VC] Failed to send mailbox message\n");
        return -1;
    }

    /* Receive response */
    if (videocore_mbox_recv(MBOX_CHANNEL_PROP_VC2ARM, message, sizeof(message)) != 0) {
        Serial_WriteString("[VC] Failed to receive mailbox response\n");
        return -1;
    }

    /* Parse response - extract framebuffer info from tags */
    uint32_t response_code = message[1];
    if (response_code != 0x80000000) {  /* Success code */
        Serial_Printf("[VC] GPU returned error code: 0x%lx\n", (unsigned long)response_code);
        return -1;
    }

    /* Extract values from response tags */
    ptr = &message[2];
    while (*ptr != 0) {
        uint32_t tag = *ptr++;
        uint32_t value_len = *ptr++;
        uint32_t response_len = *ptr++;  /* Response length (if set, response succeeded) */

        switch (tag) {
            case MBOX_TAG_SET_PHYS_WH:
                if (response_len) {
                    fb_info->width = *ptr++;
                    fb_info->height = *ptr++;
                }
                break;

            case MBOX_TAG_SET_DEPTH:
                if (response_len) {
                    fb_info->depth = *ptr++;
                }
                break;

            case MBOX_TAG_ALLOCATE_FB:
                if (response_len) {
                    fb_info->fb_address = *ptr++;
                    fb_info->fb_size = *ptr++;
                }
                break;

            default:
                /* Skip unknown tag */
                ptr += (value_len + 3) / 4;  /* Advance by response size */
                break;
        }
    }

    /* Calculate pitch (bytes per line) */
    fb_info->pitch = fb_info->width * (fb_info->depth / 8);

    Serial_Printf("[VC] Framebuffer allocated: %lux%lu, depth=%lu, pitch=%lu\n",
                  (unsigned long)fb_info->width, (unsigned long)fb_info->height,
                  (unsigned long)fb_info->depth, (unsigned long)fb_info->pitch);
    Serial_Printf("[VC] FB address: 0x%lx, size: %lu bytes\n",
                  (unsigned long)fb_info->fb_address, (unsigned long)fb_info->fb_size);

    memcpy(&cached_fb, fb_info, sizeof(*fb_info));
    fb_cached = 1;

    return 0;
}

/*
 * Get current framebuffer info (from cache if available)
 */
int videocore_get_fb_info(videocore_fb_t *fb_info) {
    if (!fb_info) {
        return -1;
    }

    if (fb_cached) {
        memcpy(fb_info, &cached_fb, sizeof(*fb_info));
        return 0;
    }

    /* Not cached yet */
    return -1;
}

/*
 * Set framebuffer dimensions
 */
int videocore_set_fb_size(uint32_t width, uint32_t height, uint32_t depth) {
    videocore_fb_t fb_info;
    memset(&fb_info, 0, sizeof(fb_info));

    fb_info.width = width;
    fb_info.height = height;
    fb_info.depth = depth;

    return videocore_allocate_fb(&fb_info);
}

/*
 * Get board model
 */
uint32_t videocore_get_board_model(void) {
    if (!videocore_mbox_base) {
        return 0;
    }

    /* TODO: Send mailbox request for board model */
    return 0;
}

/*
 * Get board revision
 */
uint32_t videocore_get_board_revision(void) {
    if (!videocore_mbox_base) {
        return 0;
    }

    /* TODO: Send mailbox request for board revision */
    return 0;
}

/*
 * Get board serial number
 */
uint64_t videocore_get_board_serial(void) {
    if (!videocore_mbox_base) {
        return 0;
    }

    /* TODO: Send mailbox request for serial number */
    return 0;
}
