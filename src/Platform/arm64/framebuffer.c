/*
 * ARM64 Framebuffer Driver
 * VideoCore framebuffer via mailbox for Raspberry Pi 3/4/5
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mailbox.h"

/* String function declarations */
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);

/* Mailbox property tag structure */
typedef struct {
    uint32_t buffer_size;
    uint32_t request_code;

    /* Physical size tag */
    uint32_t tag_phys_size;
    uint32_t value_size_phys;
    uint32_t request_phys;
    uint32_t width_phys;
    uint32_t height_phys;

    /* Virtual size tag */
    uint32_t tag_virt_size;
    uint32_t value_size_virt;
    uint32_t request_virt;
    uint32_t width_virt;
    uint32_t height_virt;

    /* Depth tag */
    uint32_t tag_depth;
    uint32_t value_size_depth;
    uint32_t request_depth;
    uint32_t depth;

    /* Pixel order tag */
    uint32_t tag_pixel_order;
    uint32_t value_size_pixel;
    uint32_t request_pixel;
    uint32_t pixel_order;

    /* Allocate buffer tag */
    uint32_t tag_allocate;
    uint32_t value_size_allocate;
    uint32_t request_allocate;
    uint32_t fb_address;
    uint32_t fb_size;

    /* Pitch tag */
    uint32_t tag_pitch;
    uint32_t value_size_pitch;
    uint32_t request_pitch;
    uint32_t pitch;

    /* End tag */
    uint32_t end_tag;
} __attribute__((aligned(16))) fb_mailbox_t;

/* Framebuffer information */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t depth;
    uint32_t *buffer;
    uint32_t buffer_size;
    bool initialized;
} framebuffer_t;

static framebuffer_t fb = {0};

/* Mailbox buffer for framebuffer setup */
static fb_mailbox_t __attribute__((aligned(16))) fb_msg;

/* Mailbox channel for property tags */
#define MBOX_CH_PROP    8

/* Tag IDs */
#define MBOX_TAG_SET_PHYSICAL_SIZE  0x00048003
#define MBOX_TAG_SET_VIRTUAL_SIZE   0x00048004
#define MBOX_TAG_SET_DEPTH          0x00048005
#define MBOX_TAG_SET_PIXEL_ORDER    0x00048006
#define MBOX_TAG_ALLOCATE_BUFFER    0x00040001
#define MBOX_TAG_GET_PITCH          0x00040008

/* Request/response codes */
#define MBOX_REQUEST            0x00000000
#define MBOX_RESPONSE_SUCCESS   0x80000000

/* Pixel orders */
#define PIXEL_ORDER_BGR         0
#define PIXEL_ORDER_RGB         1

/*
 * Initialize framebuffer
 */
bool framebuffer_init(uint32_t width, uint32_t height, uint32_t depth) {
    if (fb.initialized) return true;

    /* Clear mailbox message */
    memset(&fb_msg, 0, sizeof(fb_msg));

    /* Set up mailbox message */
    fb_msg.buffer_size = sizeof(fb_msg);
    fb_msg.request_code = MBOX_REQUEST;

    /* Set physical size */
    fb_msg.tag_phys_size = MBOX_TAG_SET_PHYSICAL_SIZE;
    fb_msg.value_size_phys = 8;
    fb_msg.request_phys = 0;
    fb_msg.width_phys = width;
    fb_msg.height_phys = height;

    /* Set virtual size (same as physical) */
    fb_msg.tag_virt_size = MBOX_TAG_SET_VIRTUAL_SIZE;
    fb_msg.value_size_virt = 8;
    fb_msg.request_virt = 0;
    fb_msg.width_virt = width;
    fb_msg.height_virt = height;

    /* Set depth */
    fb_msg.tag_depth = MBOX_TAG_SET_DEPTH;
    fb_msg.value_size_depth = 4;
    fb_msg.request_depth = 0;
    fb_msg.depth = depth;

    /* Set pixel order (RGB) */
    fb_msg.tag_pixel_order = MBOX_TAG_SET_PIXEL_ORDER;
    fb_msg.value_size_pixel = 4;
    fb_msg.request_pixel = 0;
    fb_msg.pixel_order = PIXEL_ORDER_RGB;

    /* Allocate buffer */
    fb_msg.tag_allocate = MBOX_TAG_ALLOCATE_BUFFER;
    fb_msg.value_size_allocate = 8;
    fb_msg.request_allocate = 0;
    fb_msg.fb_address = 16;  /* Alignment */
    fb_msg.fb_size = 0;

    /* Get pitch */
    fb_msg.tag_pitch = MBOX_TAG_GET_PITCH;
    fb_msg.value_size_pitch = 4;
    fb_msg.request_pitch = 0;
    fb_msg.pitch = 0;

    /* End tag */
    fb_msg.end_tag = 0;

    /* Call mailbox - need to pass address of our message buffer */
    /* The mailbox_call implementation expects the buffer at a fixed location
     * We'll need to copy our message to the mailbox buffer */
    memcpy(mailbox_buffer, &fb_msg, sizeof(fb_msg));

    if (!mailbox_call(MBOX_CH_PROP)) {
        return false;
    }

    /* Copy response back */
    memcpy(&fb_msg, mailbox_buffer, sizeof(fb_msg));

    /* Check if allocation was successful */
    if (fb_msg.fb_address == 0) {
        return false;
    }

    /* Store framebuffer information */
    fb.width = width;
    fb.height = height;
    fb.pitch = fb_msg.pitch;
    fb.depth = depth;

    /* Convert GPU bus address to ARM physical address
     * Clear top bits to get physical address */
    uint64_t fb_addr = fb_msg.fb_address & 0x3FFFFFFF;
    fb.buffer = (uint32_t *)fb_addr;
    fb.buffer_size = fb_msg.fb_size;
    fb.initialized = true;

    return true;
}

/*
 * Get framebuffer width
 */
uint32_t framebuffer_get_width(void) {
    return fb.width;
}

/*
 * Get framebuffer height
 */
uint32_t framebuffer_get_height(void) {
    return fb.height;
}

/*
 * Get framebuffer pitch (bytes per row)
 */
uint32_t framebuffer_get_pitch(void) {
    return fb.pitch;
}

/*
 * Get framebuffer depth (bits per pixel)
 */
uint32_t framebuffer_get_depth(void) {
    return fb.depth;
}

/*
 * Get framebuffer address
 */
void *framebuffer_get_buffer(void) {
    return fb.buffer;
}

/*
 * Clear framebuffer to color
 */
void framebuffer_clear(uint32_t color) {
    if (!fb.initialized || !fb.buffer) return;

    uint32_t pixels = (fb.pitch / 4) * fb.height;
    for (uint32_t i = 0; i < pixels; i++) {
        fb.buffer[i] = color;
    }
}

/*
 * Set pixel at (x, y) to color
 */
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.initialized || !fb.buffer) return;
    if (x >= fb.width || y >= fb.height) return;

    uint32_t offset = y * (fb.pitch / 4) + x;
    fb.buffer[offset] = color;
}

/*
 * Get pixel color at (x, y)
 */
uint32_t framebuffer_get_pixel(uint32_t x, uint32_t y) {
    if (!fb.initialized || !fb.buffer) return 0;
    if (x >= fb.width || y >= fb.height) return 0;

    uint32_t offset = y * (fb.pitch / 4) + x;
    return fb.buffer[offset];
}

/*
 * Draw horizontal line
 */
void framebuffer_draw_hline(uint32_t x, uint32_t y, uint32_t width, uint32_t color) {
    if (!fb.initialized || !fb.buffer) return;
    if (y >= fb.height) return;

    uint32_t end_x = x + width;
    if (end_x > fb.width) end_x = fb.width;

    uint32_t offset = y * (fb.pitch / 4) + x;
    for (uint32_t i = x; i < end_x; i++) {
        fb.buffer[offset++] = color;
    }
}

/*
 * Draw vertical line
 */
void framebuffer_draw_vline(uint32_t x, uint32_t y, uint32_t height, uint32_t color) {
    if (!fb.initialized || !fb.buffer) return;
    if (x >= fb.width) return;

    uint32_t end_y = y + height;
    if (end_y > fb.height) end_y = fb.height;

    for (uint32_t i = y; i < end_y; i++) {
        framebuffer_set_pixel(x, i, color);
    }
}

/*
 * Draw filled rectangle
 */
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!fb.initialized) return;

    for (uint32_t i = 0; i < height; i++) {
        framebuffer_draw_hline(x, y + i, width, color);
    }
}

/*
 * Check if framebuffer is initialized
 */
bool framebuffer_is_initialized(void) {
    return fb.initialized;
}
