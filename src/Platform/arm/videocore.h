/*
 * VideoCore GPU Interface for Raspberry Pi
 * Mailbox protocol for framebuffer allocation and GPU communication
 *
 * VideoCore is the GPU on Raspberry Pi that handles:
 * - Framebuffer allocation
 * - Clock management
 * - Temperature/voltage monitoring
 */

#ifndef ARM_VIDEOCORE_H
#define ARM_VIDEOCORE_H

#include <stdint.h>

/* ===== Platform-specific mailbox addresses ===== */

/* Raspberry Pi 3/4 */
#define VIDEOCORE_MBOX_BASE_PI34    0x3F00B880

/* Raspberry Pi 5 */
#define VIDEOCORE_MBOX_BASE_PI5     0xFC00B880

/* Detect at runtime and set MBOX_BASE accordingly */
extern uint32_t videocore_mbox_base;

/* Mailbox register offsets from base address */
#define MBOX_READ    0x00
#define MBOX_WRITE   0x20
#define MBOX_STATUS  0x18
#define MBOX_CONFIG  0x1C

/* Status register bit flags */
#define MBOX_STATUS_EMPTY   (UINT32_C(1) << 30)  /* Read queue is empty */
#define MBOX_STATUS_FULL    (UINT32_C(1) << 31)  /* Write queue is full */

/* ===== Mailbox channels ===== */
#define MBOX_CHANNEL_POWER      0
#define MBOX_CHANNEL_FB         1   /* Framebuffer */
#define MBOX_CHANNEL_VUART      2
#define MBOX_CHANNEL_VCHIQ      3
#define MBOX_CHANNEL_LEDS       4
#define MBOX_CHANNEL_BUTTONS    5
#define MBOX_CHANNEL_TOUCHSC    6
#define MBOX_CHANNEL_COUNT      7
#define MBOX_CHANNEL_PROP_ARM2VC 8   /* ARM to VideoCore (properties) */
#define MBOX_CHANNEL_PROP_VC2ARM 9   /* VideoCore to ARM (properties) */

/* ===== Framebuffer Request Tags ===== */

/* Physical display width/height (in pixels) */
#define MBOX_TAG_SET_PHYS_WH        0x00048003
#define MBOX_TAG_GET_PHYS_WH        0x00040003

/* Virtual display width/height (framebuffer size) */
#define MBOX_TAG_SET_VIRT_WH        0x00048004
#define MBOX_TAG_GET_VIRT_WH        0x00040004

/* Depth (bits per pixel) */
#define MBOX_TAG_SET_DEPTH          0x00048005
#define MBOX_TAG_GET_DEPTH          0x00040005

/* Pixel order (BGR vs RGB) */
#define MBOX_TAG_SET_PIXEL_ORDER    0x00048006
#define MBOX_TAG_GET_PIXEL_ORDER    0x00040006

/* Framebuffer allocation */
#define MBOX_TAG_ALLOCATE_FB        0x00040001

/* Framebuffer physical address (for reading current FB) */
#define MBOX_TAG_GET_FB_ADDR        0x00040008

/* Power state */
#define MBOX_TAG_SET_POWER_STATE    0x00028001
#define MBOX_TAG_GET_POWER_STATE    0x00020001

/* Clock rate */
#define MBOX_TAG_SET_CLOCK_RATE     0x00038002
#define MBOX_TAG_GET_CLOCK_RATE     0x00030002

/* Get board model/revision */
#define MBOX_TAG_GET_BOARD_MODEL    0x00010001
#define MBOX_TAG_GET_BOARD_REV      0x00010002

/* Get board serial number */
#define MBOX_TAG_GET_BOARD_SERIAL   0x00010004

/* ===== Audio Tags (HDMI/Analog) ===== */

/* Enable/Disable audio output */
#define MBOX_TAG_SET_AUDIO_POWER    0x00028042
#define MBOX_TAG_GET_AUDIO_POWER    0x00020042

/* HDMI audio status */
#define MBOX_TAG_GET_HDMI_AUDIO     0x00020008
#define MBOX_TAG_SET_HDMI_AUDIO     0x00028008

/* Audio buffer */
#define MBOX_TAG_GET_AUDIO_BUFFER   0x00040009
#define MBOX_TAG_SET_AUDIO_BUFFER   0x00048009

/* ===== Framebuffer info structure ===== */
typedef struct {
    uint32_t width;             /* Physical width in pixels */
    uint32_t height;            /* Physical height in pixels */
    uint32_t virt_width;        /* Virtual width (framebuffer) */
    uint32_t virt_height;       /* Virtual height (framebuffer) */
    uint32_t pitch;             /* Bytes per line (auto-calculated) */
    uint32_t depth;             /* Bits per pixel (typically 32) */
    uint32_t x_offset;          /* X offset in framebuffer */
    uint32_t y_offset;          /* Y offset in framebuffer */
    uint32_t fb_address;        /* Physical address of framebuffer */
    uint32_t fb_size;           /* Size of framebuffer in bytes */
} videocore_fb_t;

/* ===== Public API ===== */

/* Initialize mailbox for framebuffer access */
int videocore_init(void);

/* Allocate framebuffer from GPU
 * Returns 0 on success, -1 on failure
 */
int videocore_allocate_fb(videocore_fb_t *fb_info);

/* Get current framebuffer info */
int videocore_get_fb_info(videocore_fb_t *fb_info);

/* Set framebuffer dimensions
 * Returns 0 on success, -1 on failure
 */
int videocore_set_fb_size(uint32_t width, uint32_t height, uint32_t depth);

/* Get board information */
uint32_t videocore_get_board_model(void);
uint32_t videocore_get_board_revision(void);
uint64_t videocore_get_board_serial(void);

/* Mailbox low-level operations (internal) */
int videocore_mbox_send(uint8_t channel, uint32_t *message, uint32_t message_len);
int videocore_mbox_recv(uint8_t channel, uint32_t *message, uint32_t max_len);

#endif /* ARM_VIDEOCORE_H */
