/*
 * QuickDrawPlatform.h - Platform Abstraction Layer for QuickDraw
 *
 * Platform-specific function declarations for QuickDraw implementation.
 * This header defines the interface between portable QuickDraw code
 * and platform-specific graphics backends.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __QUICKDRAWPLATFORM_H__
#define __QUICKDRAWPLATFORM_H__

#include "../include/QuickDraw/QuickDraw.h"
#include "../include/QuickDraw/ColorQuickDraw.h"
#include "../include/QuickDraw/QDRegions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * PLATFORM INITIALIZATION
 * ================================================================ */

/* Initialize platform graphics subsystem */
void QDPlatform_Initialize(void);

/* Shutdown platform graphics subsystem */
void QDPlatform_Shutdown(void);

/* ================================================================
 * DRAWING PRIMITIVES
 * ================================================================ */

/* Draw a line with specified pattern and mode */
void QDPlatform_DrawLine(GrafPtr port, Point startPt, Point endPt,
                        ConstPatternParam pat, int16_t mode);

/* Draw a shape (rectangle, oval, etc.) */
void QDPlatform_DrawShape(GrafPtr port, GrafVerb verb, const Rect *bounds,
                         int shapeType, ConstPatternParam pat);

/* Draw a region */
void QDPlatform_DrawRegion(RgnHandle rgn, GrafVerb verb, ConstPatternParam pat);

/* ================================================================
 * PIXEL OPERATIONS
 * ================================================================ */

/* Set a single pixel */
void QDPlatform_SetPixel(CGrafPtr port, int16_t h, int16_t v, const RGBColor *color);

/* Get a single pixel */
void QDPlatform_GetPixel(CGrafPtr port, int16_t h, int16_t v, RGBColor *color);

/* ================================================================
 * BITMAP OPERATIONS
 * ================================================================ */

/* Platform-specific CopyBits implementation */
void QDPlatform_CopyBits(const BitMap *srcBits, const BitMap *dstBits,
                        const Rect *srcRect, const Rect *dstRect,
                        int16_t mode, RgnHandle maskRgn);

/* ================================================================
 * CURSOR MANAGEMENT
 * ================================================================ */

/* Set cursor image */
void QDPlatform_SetCursor(const Cursor *cursor);

/* Show/hide cursor */
void QDPlatform_ShowCursor(void);
void QDPlatform_HideCursor(void);

/* ================================================================
 * SCREEN MANAGEMENT
 * ================================================================ */

/* Get screen dimensions */
void QDPlatform_GetScreenBounds(Rect *bounds);

/* Get screen pixel depth */
int16_t QDPlatform_GetScreenDepth(void);

/* Update screen display */
void QDPlatform_FlushDisplay(void);

/* ================================================================
 * MEMORY MANAGEMENT
 * ================================================================ */

/* Allocate graphics memory */
void* QDPlatform_AllocateGraphicsMemory(size_t size);

/* Free graphics memory */
void QDPlatform_FreeGraphicsMemory(void *ptr);

/* ================================================================
 * PLATFORM-SPECIFIC TYPES
 * ================================================================ */

/* Platform graphics context */
typedef struct PlatformGraphicsContext {
    void *nativeContext;   /* Platform-specific context */
    int16_t depth;         /* Pixel depth */
    Rect bounds;           /* Context bounds */
    bool isColor;          /* True if color context */
} PlatformGraphicsContext;

/* Get platform context for a GrafPort */
PlatformGraphicsContext* QDPlatform_GetContext(GrafPtr port);

/* Get platform context for a CGrafPort */
PlatformGraphicsContext* QDPlatform_GetColorContext(CGrafPtr port);

#ifdef __cplusplus
}
#endif

#endif /* __QUICKDRAWPLATFORM_H__ */