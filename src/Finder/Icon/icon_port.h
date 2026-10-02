#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawPlatform.h"

/* Shared QuickDraw globals */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern GrafPtr g_currentPort;

/* Write a single pixel at local (x, y) coordinates into the active QuickDraw port.
 * Falls back to the global framebuffer if no port is active. */
static inline void IconPort_WritePixel(int x, int y, uint32_t color) {
    if (g_currentPort && g_currentPort->portBits.baseAddr) {
        Rect portRect = g_currentPort->portRect;
        SInt16 localLeft = portRect.left;
        SInt16 localTop = portRect.top;

        if (x < portRect.left || x >= portRect.right ||
            y < portRect.top || y >= portRect.bottom) {
            return;
        }

        uint8_t* baseAddr = (uint8_t*)g_currentPort->portBits.baseAddr;
        SInt16 rowBytes = g_currentPort->portBits.rowBytes & 0x3FFF;

        SInt16 relX = x - localLeft;
        SInt16 relY = y - localTop;

        if (baseAddr == (uint8_t*)framebuffer) {
            SInt16 boundsLeft = g_currentPort->portBits.bounds.left;
            SInt16 boundsTop = g_currentPort->portBits.bounds.top;
            int globalX = boundsLeft + relX;
            int globalY = boundsTop + relY;

            if (globalX < 0 || globalX >= (int)fb_width ||
                globalY < 0 || globalY >= (int)fb_height) {
                return;
            }
            if (!QD_ClipHas(globalX, globalY)) {
                return;   /* clipped, like QuickDraw's own drawing */
            }

            uint8_t* fbBase = (uint8_t*)framebuffer;
            size_t offset = (size_t)globalY * (size_t)fb_pitch +
                            (size_t)globalX * sizeof(uint32_t);
            memcpy(fbBase + offset, &color, sizeof(color));
        } else {
            if (relX < 0 || relY < 0) {
                return;
            }
            size_t offset = (size_t)relY * (size_t)rowBytes +
                            (size_t)relX * sizeof(uint32_t);
            memcpy(baseAddr + offset, &color, sizeof(color));
        }
        return;
    }

    /* No active port - fall back to raw framebuffer write */
    if (!framebuffer) {
        return;
    }

    if (x < 0 || x >= (int)fb_width || y < 0 || y >= (int)fb_height) {
        return;
    }

    uint8_t* fbBase = (uint8_t*)framebuffer;
    size_t offset = (size_t)y * (size_t)fb_pitch + (size_t)x * sizeof(uint32_t);
    memcpy(fbBase + offset, &color, sizeof(color));
}
