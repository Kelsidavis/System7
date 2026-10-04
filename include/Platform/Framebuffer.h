/* Shared framebuffer state populated during platform boot. */
#ifndef SYSTEM7_PLATFORM_FRAMEBUFFER_H
#define SYSTEM7_PLATFORM_FRAMEBUFFER_H

#include <stdint.h>

extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint8_t fb_bpp;
extern uint8_t fb_red_pos;
extern uint8_t fb_red_size;
extern uint8_t fb_green_pos;
extern uint8_t fb_green_size;
extern uint8_t fb_blue_pos;
extern uint8_t fb_blue_size;

uint32_t pack_color(uint8_t red, uint8_t green, uint8_t blue);

#endif /* SYSTEM7_PLATFORM_FRAMEBUFFER_H */
