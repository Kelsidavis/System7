/*
 * ARM64 Framebuffer Interface
 * VideoCore framebuffer for Raspberry Pi 3/4/5
 */

#ifndef ARM64_FRAMEBUFFER_H
#define ARM64_FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize framebuffer with specified dimensions and depth */
bool framebuffer_init(uint32_t width, uint32_t height, uint32_t depth);

/* Get framebuffer properties */
uint32_t framebuffer_get_width(void);
uint32_t framebuffer_get_height(void);
uint32_t framebuffer_get_pitch(void);
uint32_t framebuffer_get_depth(void);
void *framebuffer_get_buffer(void);

/* Drawing operations */
void framebuffer_clear(uint32_t color);
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

/* Check initialization status */
bool framebuffer_is_initialized(void);

/* Common color definitions (ARGB format) */
#define FB_COLOR_BLACK      0xFF000000
#define FB_COLOR_WHITE      0xFFFFFFFF
#define FB_COLOR_RED        0xFFFF0000
#define FB_COLOR_GREEN      0xFF00FF00
#define FB_COLOR_BLUE       0xFF0000FF
#define FB_COLOR_YELLOW     0xFFFFFF00
#define FB_COLOR_CYAN       0xFF00FFFF
#define FB_COLOR_MAGENTA    0xFFFF00FF
#define FB_COLOR_GRAY       0xFF808080

#endif /* ARM64_FRAMEBUFFER_H */
