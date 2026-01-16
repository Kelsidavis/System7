/*
 * ARM64 Display Interface
 * Unified display access for both QEMU (VirtIO GPU) and Raspberry Pi (framebuffer)
 */

#ifndef ARM64_DISPLAY_H
#define ARM64_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Initialize display subsystem
 * Returns true on success
 */
bool display_init(uint32_t width, uint32_t height, uint32_t depth);

/*
 * Check if display is initialized
 */
bool display_is_initialized(void);

/*
 * Get display width in pixels
 */
uint32_t display_get_width(void);

/*
 * Get display height in pixels
 */
uint32_t display_get_height(void);

/*
 * Get display depth (bits per pixel)
 */
uint32_t display_get_depth(void);

/*
 * Get display pitch (bytes per row)
 */
uint32_t display_get_pitch(void);

/*
 * Get framebuffer address
 */
void *display_get_buffer(void);

/*
 * Clear display to specified color
 */
void display_clear(uint32_t color);

/*
 * Flush display (for VirtIO GPU)
 */
void display_flush(void);

/*
 * Set pixel at (x, y) to color
 */
void display_set_pixel(uint32_t x, uint32_t y, uint32_t color);

/*
 * Draw filled rectangle
 */
void display_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

#endif /* ARM64_DISPLAY_H */
