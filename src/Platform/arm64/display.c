/*
 * ARM64 Display Interface Implementation
 * Wraps platform-specific display drivers
 */

#include "display.h"

#ifdef QEMU_BUILD
#include "virtio_gpu.h"
#else
/* Forward declarations for framebuffer functions (no header file) */
extern bool framebuffer_init(uint32_t width, uint32_t height, uint32_t depth);
extern bool framebuffer_is_initialized(void);
extern uint32_t framebuffer_get_width(void);
extern uint32_t framebuffer_get_height(void);
extern uint32_t framebuffer_get_depth(void);
extern uint32_t framebuffer_get_pitch(void);
extern void *framebuffer_get_buffer(void);
extern void framebuffer_clear(uint32_t color);
extern void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
extern void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
#endif

/* Stored dimensions for when display is initialized */
static uint32_t g_display_width = 0;
static uint32_t g_display_height = 0;
static uint32_t g_display_depth = 0;

/*
 * Initialize display subsystem
 */
bool display_init(uint32_t width, uint32_t height, uint32_t depth) {
    bool result;

#ifdef QEMU_BUILD
    result = virtio_gpu_init();
    if (result) {
        g_display_width = virtio_gpu_get_width();
        g_display_height = virtio_gpu_get_height();
        g_display_depth = 32;  /* VirtIO GPU uses 32bpp */
    }
#else
    result = framebuffer_init(width, height, depth);
    if (result) {
        g_display_width = framebuffer_get_width();
        g_display_height = framebuffer_get_height();
        g_display_depth = framebuffer_get_depth();
    }
#endif

    return result;
}

/*
 * Check if display is initialized
 */
bool display_is_initialized(void) {
#ifdef QEMU_BUILD
    return virtio_gpu_get_buffer() != 0;
#else
    return framebuffer_is_initialized();
#endif
}

/*
 * Get display width
 */
uint32_t display_get_width(void) {
#ifdef QEMU_BUILD
    return virtio_gpu_get_width();
#else
    return framebuffer_get_width();
#endif
}

/*
 * Get display height
 */
uint32_t display_get_height(void) {
#ifdef QEMU_BUILD
    return virtio_gpu_get_height();
#else
    return framebuffer_get_height();
#endif
}

/*
 * Get display depth
 */
uint32_t display_get_depth(void) {
#ifdef QEMU_BUILD
    return 32;  /* VirtIO GPU always 32bpp */
#else
    return framebuffer_get_depth();
#endif
}

/*
 * Get display pitch
 */
uint32_t display_get_pitch(void) {
#ifdef QEMU_BUILD
    return virtio_gpu_get_width() * 4;  /* 32bpp = 4 bytes per pixel */
#else
    return framebuffer_get_pitch();
#endif
}

/*
 * Get framebuffer address
 */
void *display_get_buffer(void) {
#ifdef QEMU_BUILD
    return virtio_gpu_get_buffer();
#else
    return framebuffer_get_buffer();
#endif
}

/*
 * Clear display
 */
void display_clear(uint32_t color) {
#ifdef QEMU_BUILD
    virtio_gpu_clear(color);
#else
    framebuffer_clear(color);
#endif
}

/*
 * Flush display
 */
void display_flush(void) {
#ifdef QEMU_BUILD
    virtio_gpu_flush();
#else
    /* Framebuffer is memory-mapped, no flush needed */
#endif
}

/*
 * Set pixel
 */
void display_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
#ifdef QEMU_BUILD
    uint32_t *buffer = virtio_gpu_get_buffer();
    uint32_t width = virtio_gpu_get_width();
    uint32_t height = virtio_gpu_get_height();
    if (buffer && x < width && y < height) {
        buffer[y * width + x] = color;
    }
#else
    framebuffer_set_pixel(x, y, color);
#endif
}

/*
 * Draw filled rectangle
 */
void display_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
#ifdef QEMU_BUILD
    virtio_gpu_draw_rect(x, y, w, h, color);
#else
    framebuffer_draw_rect(x, y, w, h, color);
#endif
}
