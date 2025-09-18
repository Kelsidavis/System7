/*
 * QuickDraw.h - Complete QuickDraw Graphics System
 *
 * Master header file that includes all QuickDraw components.
 * This provides the complete Apple QuickDraw API for modern platforms.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#ifndef __QUICKDRAW_COMPLETE_H__
#define __QUICKDRAW_COMPLETE_H__

/* Core QuickDraw headers */
#include "QuickDraw/QDTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDraw/QDRegions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * QUICKDRAW SYSTEM OVERVIEW
 * ================================================================ */

/*
 * QuickDraw is Apple's 2D graphics system that formed the foundation
 * of all Macintosh graphics from 1984 through Mac OS X. This portable
 * implementation provides:
 *
 * CORE FEATURES:
 * - Basic drawing primitives (lines, rectangles, ovals, arcs, polygons)
 * - Bitmap and pixmap operations with scaling and transfer modes
 * - Complex region management and clipping
 * - 32-bit Color QuickDraw with color tables and dithering
 * - Pattern fills and texture operations
 * - Text rendering with multiple fonts and styles
 * - Coordinate system transformations
 * - Graphics ports for multiple drawing contexts
 *
 * ARCHITECTURE:
 * - Platform-abstracted implementation
 * - Hardware-accelerated operations where available
 * - Memory-efficient region arithmetic
 * - Compatible with original Mac OS semantics
 * - Support for 1-bit, 8-bit, 16-bit, and 32-bit pixel depths
 *
 * COORDINATE SYSTEM:
 * - Origin (0,0) at top-left
 * - X increases rightward, Y increases downward
 * - Local coordinates relative to graphics port
 * - Global coordinates relative to screen
 *
 * GRAPHICS PORTS:
 * - GrafPort: Basic 1-bit graphics context
 * - CGrafPort: Color graphics context with pixel patterns
 * - Multiple ports can exist simultaneously
 * - Each port has its own coordinate system and clipping
 *
 * REGIONS:
 * - Arbitrary 2D shapes defined by scan lines
 * - Boolean operations (union, intersection, difference, XOR)
 * - Efficient clipping and hit testing
 * - Memory-optimized representation
 *
 * COLOR SUPPORT:
 * - RGB color model with 16-bit components
 * - Color tables for indexed color modes
 * - Automatic color matching and dithering
 * - Graphics device management
 * - Support for multiple color depths
 */

/* ================================================================
 * INITIALIZATION SEQUENCE
 * ================================================================ */

/*
 * Typical QuickDraw initialization sequence:
 *
 * 1. InitGraf(&qd) - Initialize QuickDraw globals
 * 2. OpenPort(&myPort) - Create and set graphics port
 * 3. Set drawing attributes (pen, patterns, colors)
 * 4. Draw graphics primitives
 * 5. ClosePort(&myPort) - Clean up
 *
 * For color graphics:
 *
 * 1. InitGraf(&qd) - Initialize QuickDraw
 * 2. OpenCPort(&myColorPort) - Create color port
 * 3. Set color attributes (RGB colors, pixel patterns)
 * 4. Draw with color primitives
 * 5. CloseCPort(&myColorPort) - Clean up
 */

/* ================================================================
 * COMPATIBILITY NOTES
 * ================================================================ */

/*
 * This implementation maintains API compatibility with:
 * - Classic QuickDraw (Mac OS 1.0+)
 * - Color QuickDraw (Mac OS 4.1+)
 * - 32-Bit QuickDraw (System 7.0+)
 *
 * Key differences from original:
 * - Thread-safe implementation
 * - Modern memory management
 * - Platform-abstracted rendering
 * - Enhanced error checking
 * - Support for modern pixel formats
 *
 * Unsupported features:
 * - QuickDraw GX (separate graphics system)
 * - PostScript printing (use modern print APIs)
 * - 68K assembly optimizations (pure C implementation)
 */

/* ================================================================
 * PERFORMANCE CONSIDERATIONS
 * ================================================================ */

/*
 * For optimal performance:
 * - Use CopyBits for bitmap operations
 * - Minimize region complexity
 * - Use rectangular regions when possible
 * - Batch drawing operations
 * - Prefer color ports for color operations
 * - Use platform-accelerated primitives
 */

/* ================================================================
 * ERROR HANDLING
 * ================================================================ */

/*
 * QuickDraw errors are reported through:
 * - QDError() function returns last error code
 * - Region operations set region-specific errors
 * - Memory allocation failures return NULL handles
 * - Invalid parameters are caught by assertions in debug builds
 */

/* Global QuickDraw structure access */
extern QDGlobals qd;

/* Quick access macros for common patterns */
#define QD_WHITE_PATTERN   (&qd.white)
#define QD_BLACK_PATTERN   (&qd.black)
#define QD_GRAY_PATTERN    (&qd.gray)
#define QD_LTGRAY_PATTERN  (&qd.ltGray)
#define QD_DKGRAY_PATTERN  (&qd.dkGray)

/* Common colors */
#define QD_RGB_BLACK    (&kRGBBlack)
#define QD_RGB_WHITE    (&kRGBWhite)
#define QD_RGB_RED      (&kRGBRed)
#define QD_RGB_GREEN    (&kRGBGreen)
#define QD_RGB_BLUE     (&kRGBBlue)

/* Utility macros */
#define QD_RECT_WIDTH(r)   ((r)->right - (r)->left)
#define QD_RECT_HEIGHT(r)  ((r)->bottom - (r)->top)
#define QD_POINT_IN_RECT(pt, r) PtInRect(pt, r)
#define QD_RECT_EMPTY(r)   EmptyRect(r)

/* ================================================================
 * VERSION INFORMATION
 * ================================================================ */

#define QUICKDRAW_VERSION_MAJOR 7
#define QUICKDRAW_VERSION_MINOR 1
#define QUICKDRAW_VERSION_PATCH 0

/* Check if Color QuickDraw is available */
#define QD_HAS_COLOR_QUICKDRAW 1

/* Check if specific features are available */
#define QD_HAS_32BIT_QUICKDRAW 1
#define QD_HAS_OFFSCREEN_PIXMAPS 1
#define QD_HAS_DEEP_MASKS 1
#define QD_HAS_DEVICE_LOOP 1

/* Get version string */
const char* QDGetVersionString(void);

/* Get feature availability */
bool QDHasFeature(int featureFlag);

#ifdef __cplusplus
}
#endif

#endif /* __QUICKDRAW_COMPLETE_H__ */