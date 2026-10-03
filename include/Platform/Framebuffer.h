/* Shared framebuffer state populated during platform boot. */
#ifndef SYSTEM7_PLATFORM_FRAMEBUFFER_H
#define SYSTEM7_PLATFORM_FRAMEBUFFER_H

#include <stdint.h>

extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

#endif /* SYSTEM7_PLATFORM_FRAMEBUFFER_H */
